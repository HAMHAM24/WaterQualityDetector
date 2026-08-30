/**
 * @file    sensors.cpp
 * @brief   Implementasi driver sensor DS18B20 (suhu), TDS DFRobot, dan
 *          turbidity SEN0189, lengkap dengan filter moving average,
 *          konversi satuan fisik, dan deteksi kegagalan sensor.
 */

#include "sensors.h"
#include "globals.h"
#include "config.h"
#include "storage.h"

#include <OneWire.h>
#include <DallasTemperature.h>

// =============================================================================
// OBJEK DRIVER SUHU (OneWire + DallasTemperature)
// =============================================================================
static OneWire s_oneWire(PIN_DS18B20);
static DallasTemperature s_dallasSensors(&s_oneWire);
static DeviceAddress s_tempSensorAddress;
static bool s_tempSensorFound = false;

// =============================================================================
// BUFFER MOVING AVERAGE — TDS
// =============================================================================
static uint16_t s_tdsSampleBuffer[FILTER_SAMPLE_COUNT];
static uint8_t  s_tdsSampleIndex  = 0;
static uint8_t  s_tdsSampleFilled = 0;
static uint32_t s_tdsSampleSum    = 0;

// =============================================================================
// BUFFER MOVING AVERAGE — TURBIDITY
// =============================================================================
static uint16_t s_turbiditySampleBuffer[FILTER_SAMPLE_COUNT];
static uint8_t  s_turbiditySampleIndex  = 0;
static uint8_t  s_turbiditySampleFilled = 0;
static uint32_t s_turbiditySampleSum    = 0;

/**
 * @brief Memasukkan satu sampel baru ke buffer circular moving average
 *        dan mengembalikan rata-rata terbaru.
 * @param buffer       Array buffer circular.
 * @param index         Referensi index tulis berikutnya (di-update).
 * @param filledCount   Referensi jumlah sampel valid saat ini (di-update).
 * @param sum           Referensi akumulasi jumlah sampel (di-update).
 * @param newSample     Sampel ADC baru yang akan dimasukkan.
 * @return Rata-rata (float) dari seluruh sampel dalam buffer.
 */
static float pushSampleAndAverage(uint16_t* buffer, uint8_t& index,
                                   uint8_t& filledCount, uint32_t& sum,
                                   uint16_t newSample) {
    if (filledCount == FILTER_SAMPLE_COUNT) {
        // Buffer penuh: kurangi kontribusi sampel paling lama sebelum ditimpa
        sum -= buffer[index];
    } else {
        filledCount++;
    }

    buffer[index] = newSample;
    sum += newSample;
    index = static_cast<uint8_t>((index + 1) % FILTER_SAMPLE_COUNT);

    return static_cast<float>(sum) / static_cast<float>(filledCount);
}

/**
 * @brief Inisialisasi seluruh perangkat keras sensor.
 */
void sensors_init() {
    analogReadResolution(ADC_RESOLUTION_BITS);

    s_dallasSensors.begin();
    s_dallasSensors.setWaitForConversion(false); // konversi non-blocking

    s_tempSensorFound = s_dallasSensors.getAddress(s_tempSensorAddress, 0);
    if (s_tempSensorFound) {
        s_dallasSensors.setResolution(s_tempSensorAddress, 12);
    }

    // Reset buffer filter
    s_tdsSampleIndex = 0;
    s_tdsSampleFilled = 0;
    s_tdsSampleSum = 0;
    s_turbiditySampleIndex = 0;
    s_turbiditySampleFilled = 0;
    s_turbiditySampleSum = 0;
}

/**
 * @brief Memulai konversi suhu DS18B20 tanpa memblokir task lain.
 */
void sensors_requestTemperature() {
    if (s_tempSensorFound) {
        s_dallasSensors.requestTemperatures();
    }
}

/**
 * @brief Membaca hasil konversi suhu DS18B20 dan memperbarui g_sensorData.
 */
void sensors_readTemperature() {
    float temperatureC = DEVICE_DISCONNECTED_C;
    SensorStatus status = SensorStatus::ERROR;

    if (s_tempSensorFound) {
        temperatureC = s_dallasSensors.getTempC(s_tempSensorAddress);
        if (temperatureC != DEVICE_DISCONNECTED_C && temperatureC > -55.0f && temperatureC < 125.0f) {
            status = SensorStatus::OK;
        } else {
            temperatureC = 0.0f;
            status = SensorStatus::ERROR;
        }
    } else {
        // Sensor belum pernah terdeteksi saat init; coba deteksi ulang.
        s_tempSensorFound = s_dallasSensors.getAddress(s_tempSensorAddress, 0);
        if (s_tempSensorFound) {
            s_dallasSensors.setResolution(s_tempSensorAddress, 12);
        }
        temperatureC = 0.0f;
        status = SensorStatus::ERROR;
    }

    const float offset = (status == SensorStatus::OK) ? g_calibParams.tempOffset : 0.0f;

    if (xSemaphoreTake(g_dataMutex, DATA_MUTEX_TIMEOUT) == pdTRUE) {
        g_sensorData.temperatureRaw    = temperatureC;
        g_sensorData.temperature       = temperatureC + offset;
        g_sensorData.temperatureStatus = status;
        g_systemState.displayDirty     = true;
        xSemaphoreGive(g_dataMutex);
    }
}

/**
 * @brief Membaca ADC mentah sensor TDS.
 */
uint16_t sensors_readTDSRaw() {
    return static_cast<uint16_t>(analogRead(PIN_TDS_ANALOG));
}

float sensors_adcToVoltage(float raw, float divider) {
    return (raw / static_cast<float>(ADC_MAX_VALUE)) * ADC_REFERENCE_VOLTAGE * divider;
}

float sensors_voltageToTds(float voltage, float temperature) {
    if (voltage <= 0.0f) {
        return 0.0f;
    }

    // TDS mentah dihitung dahulu dari polinomial, lalu dikompensasi ke 25 C.
    float temperatureForComp = temperature;
    if (temperatureForComp < 1.0f || temperatureForComp > 60.0f) {
        temperatureForComp = TDS_TEMP_REFERENCE;
    }

    float factor = 1.0f + 0.02f * (temperatureForComp - 25.0f);
    if (factor < 0.1f) {
        factor = 0.1f;   // proteksi pembagian mendekati nol
    }
    // Polinomial DFRobot: EC (uS/cm) dari tegangan, lalu EC -> ppm.
    const float v  = voltage;
    const float v2 = v * v;
    const float v3 = v2 * v;
    const float ec = TDS_POLY_C3 * v3 + TDS_POLY_C2 * v2 + TDS_POLY_C1 * v;

    const float rawPpm = ec * TDS_EC_TO_PPM_FACTOR;
    float ppm = (rawPpm / factor) * g_calibParams.tdsKFactor;

    if (ppm < 0.0f) ppm = 0.0f;                 // polinomial bisa negatif di V sangat kecil
    if (ppm > 2000.0f) ppm = 2000.0f;           // jepit ke batas wajar sensor

    return ppm;
}

/**
 * @brief Mengambil sampel TDS, memperbarui filter, dan menyimpan hasil.
 */
void sensors_updateTDS() {
    const uint16_t raw = sensors_readTDSRaw();
    const float filteredRaw = pushSampleAndAverage(s_tdsSampleBuffer, s_tdsSampleIndex,
                                                    s_tdsSampleFilled, s_tdsSampleSum, raw);

    const float voltage = sensors_adcToVoltage(filteredRaw, TDS_INPUT_DIVIDER);

    // Suhu terakhir dibaca lebih dulu agar kompensasi memakai nilai terbaru.
    float temperature = TDS_TEMP_REFERENCE;
    if (xSemaphoreTake(g_dataMutex, DATA_MUTEX_TIMEOUT) == pdTRUE) {
        if (g_sensorData.temperatureStatus == SensorStatus::OK) {
            temperature = g_sensorData.temperature;
        }
        xSemaphoreGive(g_dataMutex);
    }

    const float ppm = sensors_voltageToTds(voltage, temperature);

    // ADC yang terjepit di salah satu ujung skala menandakan kabel lepas atau
    // sensor korslet, bukan pembacaan sah.
    const SensorStatus status = (raw == 0 || raw >= ADC_MAX_VALUE) ? SensorStatus::ERROR
                                                                   : SensorStatus::OK;

    if (xSemaphoreTake(g_dataMutex, DATA_MUTEX_TIMEOUT) == pdTRUE) {
        g_sensorData.tdsRaw      = raw;
        g_sensorData.tdsVoltage  = voltage;
        g_sensorData.tdsFiltered = ppm;
        g_sensorData.tdsStatus   = status;
        g_systemState.displayDirty = true;
        xSemaphoreGive(g_dataMutex);
    }
}

/**
 * @brief Membaca ADC mentah sensor turbidity.
 */
uint16_t sensors_readTurbidityRaw() {
    return static_cast<uint16_t>(analogRead(PIN_TURBIDITY_ANALOG));
}

float sensors_voltageToNtu(float voltage) {
    // Dua titik: 0 NTU (Vclear) dan larutan standar custom (Vstandard).
    // Bila titik kedua belum sah, gunakan slope legacy agar alat tetap bekerja.
    float slope = TURBIDITY_NTU_PER_VOLT;
    const float deltaV = g_calibParams.turbidityVStandard - g_calibParams.turbidityVClear;
    if (g_calibParams.turbidityVStandard > 0.0f &&
        fabsf(deltaV) >= TURBIDITY_MIN_CALIBRATION_DELTA_V) {
        slope = g_calibParams.turbidityNtuStandard / deltaV;
        float ntu = (voltage - g_calibParams.turbidityVClear) * slope;
        if (ntu < 0.0f) ntu = 0.0f;
        if (ntu > 3000.0f) ntu = 3000.0f;
        return ntu;
    }

    // Pertahankan kurva fallback lama sampai kalibrasi dua titik berhasil dilakukan.
    float ntu = (g_calibParams.turbidityVClear - voltage) * slope;

    if (ntu < 0.0f) ntu = 0.0f;
    if (ntu > 3000.0f) ntu = 3000.0f; // Limit to typical sensor max

    return ntu;
}

/**
 * @brief Mengambil sampel turbidity, memperbarui filter, dan menyimpan hasil.
 */
void sensors_updateTurbidity() {
    const uint16_t raw = sensors_readTurbidityRaw();
    const float filteredRaw = pushSampleAndAverage(s_turbiditySampleBuffer, s_turbiditySampleIndex,
                                                    s_turbiditySampleFilled, s_turbiditySampleSum, raw);

    const float voltage = sensors_adcToVoltage(filteredRaw, TURBIDITY_INPUT_DIVIDER);
    const float ntu = sensors_voltageToNtu(voltage);

    const SensorStatus status = (raw == 0) ? SensorStatus::ERROR : SensorStatus::OK;

    if (xSemaphoreTake(g_dataMutex, DATA_MUTEX_TIMEOUT) == pdTRUE) {
        g_sensorData.turbidityRaw      = raw;
        g_sensorData.turbidityVoltage  = voltage;
        g_sensorData.turbidityFiltered = ntu;
        g_sensorData.turbidityStatus   = status;
        g_systemState.displayDirty     = true;
        xSemaphoreGive(g_dataMutex);
    }
}

/**
 * @brief Mengeksekusi perhitungan skor Fuzzy Sugeno memakai profil baku mutu
 *        peruntukan air yang sedang aktif.
 */
void sensors_processFuzzy() {
    float tempSnapshot = TDS_TEMP_REFERENCE;
    float tdsSnapshot = 0.0f;
    float turbSnapshot = 0.0f;
    bool tempValid = false;
    float ambientSnapshot = AMBIENT_TEMP_DEFAULT;
    WaterParameter activeParam = WaterParameter::AIR_MINUM_HIGIENE;

    if (xSemaphoreTake(g_dataMutex, DATA_MUTEX_TIMEOUT) != pdTRUE) {
        return; // gagal mengambil mutex: lewati siklus ini, jangan pakai data basi
    }
    tempValid    = (g_sensorData.temperatureStatus == SensorStatus::OK);
    if (tempValid) {
        tempSnapshot = g_sensorData.temperature;
    }
    tdsSnapshot  = g_sensorData.tdsFiltered;
    turbSnapshot = g_sensorData.turbidityFiltered;
    activeParam  = g_systemState.activeParameter;
    ambientSnapshot = g_systemState.ambientTemperature;
    xSemaphoreGive(g_dataMutex);

    const float tdsComp = tdsSnapshot;
    const FuzzyProfil_t* profil = globals_getProfile(activeParam);
    float skor = 0.0f;

    ThresholdResult_t thResult = { false, false, false };
    KualitasAir_t qStatus = STATUS_TIDAK_LOLOS;
    StatusSuhu_t tStatus = SUHU_SL;
    uint8_t tdsSeverity = 0;
    uint8_t turbiditySeverity = 0;
    uint8_t temperatureSeverity = 0;
    float deltaTemp = 0.0f;

    if (activeParam == WaterParameter::PEMANDIAN_KOLAM) {
        // Mode Pemandian / Kolam: Evaluasi Threshold Langsung (Non-Fuzzy)
        // Ambang batas gabungan konservatif: Suhu 16-35 C, Turbidity < 0.5 NTU, TDS bypass
        thResult = Threshold_CekPemandianKolam(tempSnapshot, turbSnapshot);
        skor = thResult.semuaAman ? 1.0f : 0.0f;
        qStatus = thResult.semuaAman ? STATUS_SANGAT_LAYAK : STATUS_TIDAK_LOLOS;
        tStatus = thResult.suhuAman ? SUHU_SL : SUHU_TL;
        turbiditySeverity = thResult.turbidityAman ? 0 : 2;
        temperatureSeverity = thResult.suhuAman ? 0 : 2;
    } else {
        deltaTemp = tempValid ? fabs(tempSnapshot - ambientSnapshot) : profil->tempMax;
        skor = FuzzyKualitasAir_HitungSkor_AirMinum(profil, tdsComp, turbSnapshot, deltaTemp);
        qStatus = FuzzyKualitasAir_GetStatusProfil(profil, skor);
        tStatus = FuzzyKualitasAir_CekStatusSuhu(deltaTemp, profil);
        tdsSeverity = (tdsComp < 225.0f) ? 0 : (tdsComp < 300.0f ? 1 : (tdsComp < 450.0f ? 2 : 3));
        turbiditySeverity = (turbSnapshot < 2.25f) ? 0 : (turbSnapshot < 3.0f ? 1 : (turbSnapshot < 4.5f ? 2 : 3));
        temperatureSeverity = static_cast<uint8_t>(tStatus);
        // Kepatuhan regulasi bersifat tegas; fuzzy tetap dipakai sebagai early warning.
        if (tdsComp >= 300.0f || turbSnapshot >= 3.0f || deltaTemp > 3.0f) {
            skor = 0.0f;
            qStatus = STATUS_TIDAK_LOLOS;
        }
    }

    if (xSemaphoreTake(g_dataMutex, DATA_MUTEX_TIMEOUT) == pdTRUE) {
        g_sensorData.tdsCompensated   = tdsComp;
        g_sensorData.fuzzyScore       = skor;
        g_sensorData.fuzzyScoreRaw    = FuzzyKualitasAir_HitungSkor_AirMinum(profil, tdsComp, turbSnapshot, deltaTemp);
        g_sensorData.qualityStatus    = qStatus;
        g_sensorData.tempStatus       = tStatus;
        g_sensorData.thresholdResult  = thResult;
        g_sensorData.tdsSeverity      = tdsSeverity;
        g_sensorData.turbiditySeverity = turbiditySeverity;
        g_sensorData.temperatureSeverity = temperatureSeverity;
        g_systemState.temperatureDelta = deltaTemp;

        g_systemState.displayDirty  = true;
        xSemaphoreGive(g_dataMutex);
    }
}
