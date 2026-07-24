/**
 * @file    sensors.cpp
 * @brief   Implementasi driver sensor DS18B20 (suhu), TDS DFRobot, dan
 *          turbidity SEN0189, lengkap dengan filter moving average dan
 *          deteksi kegagalan sensor.
 */

#include "sensors.h"
#include "globals.h"
#include "config.h"

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
        temperatureC = 0.0f;
        status = SensorStatus::ERROR;
    }

    if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        g_sensorData.temperature = temperatureC;
        g_sensorData.temperatureStatus = status;
        g_systemState.displayDirty = true;
        xSemaphoreGive(g_dataMutex);
    }
}

/**
 * @brief Membaca ADC mentah sensor TDS.
 */
uint16_t sensors_readTDSRaw() {
    return static_cast<uint16_t>(analogRead(PIN_TDS_ANALOG));
}

/**
 * @brief Mengambil sampel TDS, memperbarui filter, dan menyimpan hasil.
 */
void sensors_updateTDS() {
    uint16_t raw = sensors_readTDSRaw();
    float filtered = pushSampleAndAverage(s_tdsSampleBuffer, s_tdsSampleIndex,
                                           s_tdsSampleFilled, s_tdsSampleSum, raw);

    // Heuristik sederhana: pembacaan mentok di batas bawah/atas ADC secara
    // terus-menerus mengindikasikan sensor terputus atau short-circuit.
    SensorStatus status = (raw == 0 || raw >= ADC_MAX_VALUE) ? SensorStatus::ERROR
                                                              : SensorStatus::OK;

    if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        g_sensorData.tdsRaw = raw;
        g_sensorData.tdsFiltered = filtered;
        g_sensorData.tdsStatus = status;
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

/**
 * @brief Mengambil sampel turbidity, memperbarui filter, dan menyimpan hasil.
 */
void sensors_updateTurbidity() {
    uint16_t raw = sensors_readTurbidityRaw();
    float filtered = pushSampleAndAverage(s_turbiditySampleBuffer, s_turbiditySampleIndex,
                                           s_turbiditySampleFilled, s_turbiditySampleSum, raw);

    SensorStatus status = (raw == 0 || raw >= ADC_MAX_VALUE) ? SensorStatus::ERROR
                                                              : SensorStatus::OK;

    if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        g_sensorData.turbidityRaw = raw;
        g_sensorData.turbidityFiltered = filtered;
        g_sensorData.turbidityStatus = status;
        g_systemState.displayDirty = true;
        xSemaphoreGive(g_dataMutex);
    }
}
