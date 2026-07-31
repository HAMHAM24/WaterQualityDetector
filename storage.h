/**
 * @file    storage.h
 * @brief   Modul Storage EEPROM — Mengelola penyimpanan permanen parameter
 *          kalibrasi sensor (TDS K-Factor, Turbidity VClear, Temp Offset)
 *          pada memori Flash non-volatile STM32.
 */

#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>

/**
 * @struct CalibrationParams
 * @brief  Struktur data parameter kalibrasi sensor yang disimpan di EEPROM.
 */
struct CalibrationParams {
    uint32_t magicHeader;      // Marker 0x43414C49 ("CALI") untuk memvalidasi memori
    float    tdsKFactor;       // Faktor pengali kalibrasi TDS (default: 1.0f)
    float    turbidityVClear;  // Tegangan ADC air murni/jernih 0 NTU (default: 4.1f V)
    float    tempOffset;       // Offset koreksi suhu °C (default: 0.0f °C)
};

extern CalibrationParams g_calibParams;

/**
 * @brief Inisialisasi memori EEPROM dan membaca data kalibrasi. Jika EEPROM
 *        belum pernah diinisialisasi (magicHeader salah), otomatis
 *        menggunakan nilai standar pabrik (factory defaults).
 */
void storage_init();

/**
 * @brief Menyimpan parameter kalibrasi terkini (g_calibParams) ke EEPROM.
 */
void storage_saveCalibration();

/**
 * @brief Mengembalikan parameter kalibrasi ke nilai standar pabrik (factory defaults).
 */
void storage_resetFactory();

#endif // STORAGE_H
