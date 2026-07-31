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

/**
 * @brief Inisialisasi seluruh state global ke nilai default yang aman,
 *        serta membuat mutex data dan queue event tombol.
 */
void globals_init() {
    // Inisialisasi parameter kalibrasi dari EEPROM Flash
    storage_init();

    // --- Nilai awal SensorData ---
    g_sensorData.temperature        = 0.0f;
    g_sensorData.tdsRaw             = 0;
    g_sensorData.tdsFiltered        = 0.0f;
    g_sensorData.turbidityRaw       = 0;
    g_sensorData.turbidityFiltered  = 0.0f;
    g_sensorData.tdsCompensated     = 0.0f;
    g_sensorData.fuzzyScore         = 0.0f;
    g_sensorData.qualityStatus      = STATUS_LAYAK;
    g_sensorData.tempStatus         = SUHU_NORMAL;
    g_sensorData.temperatureStatus  = SensorStatus::OK;
    g_sensorData.tdsStatus          = SensorStatus::OK;
    g_sensorData.turbidityStatus    = SensorStatus::OK;

    // --- Nilai awal ButtonState untuk seluruh tombol ---
    for (uint8_t i = 0; i < static_cast<uint8_t>(ButtonID::COUNT); i++) {
        g_buttonStates[i].pressed  = false;
        g_buttonStates[i].released = false;
        g_buttonStates[i].hold     = false;
        g_buttonStates[i].repeat   = false;
    }

    // --- Nilai awal SystemState ---
    g_systemState.activeParameter    = WaterParameter::HIGIENE_SANITASI;
    g_systemState.currentMenu        = MenuState::SPLASH;
    g_systemState.previousMenu       = MenuState::SPLASH;
    g_systemState.cursorIndex        = 0;
    g_systemState.measurementSubPage = 0;
    g_systemState.settingsBrightness = DISPLAY_DEFAULT_BRIGHTNESS;
    g_systemState.settingsContrast   = DISPLAY_DEFAULT_CONTRAST;
    g_systemState.settingsAdjustMode = false;
    g_systemState.systemOK           = true;
    g_systemState.displayDirty       = true; // gambar pertama kali wajib terjadi

    // --- Objek FreeRTOS ---
    g_dataMutex = xSemaphoreCreateMutex();
    g_buttonEventQueue = xQueueCreate(BUTTON_EVENT_QUEUE_LENGTH, sizeof(ButtonEventMsg));
}
