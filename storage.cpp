/**
 * @file    storage.cpp
 * @brief   Implementasi driver penyimpanan EEPROM Flash non-volatile STM32.
 */

#include "storage.h"
#include <EEPROM.h>

constexpr uint32_t CALIB_MAGIC_KEY = 0x43414C49; // ASCI "CALI"
constexpr int EEPROM_START_ADDR    = 0;

CalibrationParams g_calibParams;

void storage_init() {
    EEPROM.get(EEPROM_START_ADDR, g_calibParams);

    // Cek apakah memori EEPROM pernah diinisialisasi
    if (g_calibParams.magicHeader != CALIB_MAGIC_KEY ||
        isnan(g_calibParams.tdsKFactor) ||
        isnan(g_calibParams.turbidityVClear) ||
        isnan(g_calibParams.tempOffset)) {
        storage_resetFactory();
    }
}

void storage_saveCalibration() {
    g_calibParams.magicHeader = CALIB_MAGIC_KEY;
    EEPROM.put(EEPROM_START_ADDR, g_calibParams);
}

void storage_resetFactory() {
    g_calibParams.magicHeader     = CALIB_MAGIC_KEY;
    g_calibParams.tdsKFactor      = 1.0f;
    g_calibParams.turbidityVClear  = 4.1f;
    g_calibParams.tempOffset       = 0.0f;
    storage_saveCalibration();
}
