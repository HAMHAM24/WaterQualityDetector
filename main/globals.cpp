/**
 * @file    globals.cpp
 * @brief   Definisi variabel data global dan inisialisasi objek FreeRTOS
 *          (mutex & queue) yang dipakai bersama oleh seluruh task.
 */

#include "globals.h"

// =============================================================================
// DEFINISI VARIABEL GLOBAL
// =============================================================================
SensorData  g_sensorData;
ButtonState g_buttonStates[static_cast<uint8_t>(ButtonID::COUNT)];
SystemState g_systemState;

SemaphoreHandle_t g_dataMutex        = nullptr;
QueueHandle_t     g_buttonEventQueue = nullptr;

const FuzzyProfil_t* globals_getProfile(WaterParameter param) {
    const uint8_t index = static_cast<uint8_t>(param);
    if (index >= WATER_PROFILE_COUNT) {
        return &WATER_QUALITY_PROFILES[0];
    }
    return &WATER_QUALITY_PROFILES[index];
}

/**
 * @brief Inisialisasi seluruh state global ke nilai default yang aman,
 *        serta membuat mutex data dan queue event tombol.
 */
bool globals_init() {
    // Inisialisasi parameter kalibrasi dari EEPROM Flash
    storage_init();

    // --- Nilai awal SensorData ---
    // Status awal sengaja ERROR, bukan OK: sebelum sensor benar-benar terbaca,
    // nilai 0.0 bukan pengukuran sah dan tidak boleh ditampilkan seolah valid.
    g_sensorData.temperature        = 0.0f;
    g_sensorData.temperatureRaw     = 0.0f;
    g_sensorData.tdsRaw             = 0;
    g_sensorData.tdsVoltage         = 0.0f;
    g_sensorData.tdsFiltered        = 0.0f;
    g_sensorData.turbidityRaw       = 0;
    g_sensorData.turbidityVoltage   = 0.0f;
    g_sensorData.turbidityFiltered  = 0.0f;
    g_sensorData.tdsCompensated     = 0.0f;
    g_sensorData.fuzzyScore         = 0.0f;
    g_sensorData.qualityStatus      = STATUS_TIDAK_LOLOS;
    g_sensorData.tempStatus         = SUHU_SL;
    g_sensorData.thresholdResult    = { false, false, false };
    g_sensorData.tdsSeverity        = 0;
    g_sensorData.turbiditySeverity  = 0;
    g_sensorData.temperatureSeverity = 0;
    g_sensorData.temperatureStatus  = SensorStatus::ERROR;
    g_sensorData.tdsStatus          = SensorStatus::ERROR;
    g_sensorData.turbidityStatus    = SensorStatus::ERROR;

    // --- Nilai awal ButtonState untuk seluruh tombol ---
    for (uint8_t i = 0; i < static_cast<uint8_t>(ButtonID::COUNT); i++) {
        g_buttonStates[i].pressed  = false;
        g_buttonStates[i].released = false;
        g_buttonStates[i].hold     = false;
        g_buttonStates[i].repeat   = false;
    }

    // --- Nilai awal SystemState ---
    g_systemState.activeParameter    = WaterParameter::AIR_MINUM_HIGIENE;
    g_systemState.currentMenu        = MenuState::BOOT_ANIMATION;
    g_systemState.previousMenu       = MenuState::BOOT_ANIMATION;
    g_systemState.cursorIndex        = 0;
    g_systemState.measurementSubPage = 0;
    g_systemState.aboutSubPage       = 0;
    g_systemState.ambientTemperature = AMBIENT_TEMP_DEFAULT;
    g_systemState.temperatureDelta   = 0.0f;
    g_systemState.stabilizationCount = 0;
    g_systemState.stabilizationTimedOut = false;
    g_systemState.calibTdsTarget     = TDS_CALIB_TARGET_DEFAULT;
    g_systemState.calibTurbidityTarget = static_cast<uint16_t>(g_calibParams.turbidityNtuStandard);
    g_systemState.calibTurbidityStep = 0;
    g_systemState.calibTurbidityVClear = 0.0f;
    g_systemState.calibSaving        = false;
    g_systemState.calibTdsError      = false;
    g_systemState.turbidityCalibFeedback = TurbidityCalibrationFeedback::NONE;
    g_systemState.turbidityCalibSuccessTick = 0;
    g_systemState.settingsBrightness = g_calibParams.displayBrightness;
    g_systemState.settingsContrast   = g_calibParams.displayContrast;
    g_systemState.settingsAdjustMode = false;
    g_systemState.systemOK           = true;
    g_systemState.displayDirty       = true; // gambar pertama kali wajib terjadi

    // --- Objek FreeRTOS ---
    // Kegagalan pembuatan mutex/queue harus dideteksi: bila dibiarkan,
    // seluruh xSemaphoreTake() akan gagal senyap dan data global menjadi
    // tidak terlindungi sama sekali.
    g_dataMutex = xSemaphoreCreateMutex();
    g_buttonEventQueue = xQueueCreate(BUTTON_EVENT_QUEUE_LENGTH, sizeof(ButtonEventMsg));

    return (g_dataMutex != nullptr) && (g_buttonEventQueue != nullptr);
}
