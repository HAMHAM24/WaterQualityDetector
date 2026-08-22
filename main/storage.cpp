/**
 * @file    storage.cpp
 * @brief   Implementasi driver penyimpanan EEPROM Flash non-volatile STM32.
 * @details Penulisan memakai API buffer (eeprom_buffer_fill /
 *          eeprom_buffered_write_byte / eeprom_buffer_flush) dan BUKAN
 *          EEPROM.put(). Alasannya: EEPROM.put() memanggil eeprom_write_byte()
 *          per byte, dan setiap pemanggilan menjalankan siklus utuh
 *          "isi buffer -> hapus sektor -> tulis ulang sektor". Menyimpan
 *          struct 16 byte lewat EEPROM.put() berarti 16 kali penghapusan
 *          sektor flash — sangat lambat dan memperpendek umur flash secara
 *          drastis. Dengan API buffer, cukup satu kali penghapusan.
 */

#include "storage.h"
#include "config.h"
#include <STM32FreeRTOS.h>
#include <EEPROM.h>
#include <string.h>

constexpr uint32_t CALIB_MAGIC_KEY = 0x43414C49; // ASCII "CALI"
constexpr int EEPROM_START_ADDR    = 0;

CalibrationParams g_calibParams;

// Buffer nilai yang menunggu ditulis ke flash, beserta penandanya.
static CalibrationParams s_pendingParams;
static volatile bool s_savePending = false;

/**
 * @brief Membandingkan dua parameter kalibrasi byte per byte.
 */
static bool paramsEqual(const CalibrationParams& a, const CalibrationParams& b) {
    return memcmp(&a, &b, sizeof(CalibrationParams)) == 0;
}

/**
 * @brief Menulis satu struct ke flash dalam satu siklus hapus-tulis.
 *        Scheduler ditangguhkan agar tidak ada task lain (khususnya
 *        pembacaan OneWire DS18B20 dan penggambaran I2C OLED) yang berjalan
 *        saat bus flash terhenti.
 */
static void writeToFlash(const CalibrationParams& params) {
#if defined(DATA_EEPROM_BASE)
    // MCU dengan EEPROM sejati: penulisan per byte tidak menghapus sektor,
    // sehingga EEPROM.put() sudah efisien.
    EEPROM.put(EEPROM_START_ADDR, params);
#else
    const uint8_t* src = reinterpret_cast<const uint8_t*>(&params);

    vTaskSuspendAll();

    eeprom_buffer_fill();   // salin isi flash saat ini ke buffer RAM
    for (size_t i = 0; i < sizeof(CalibrationParams); i++) {
        eeprom_buffered_write_byte(EEPROM_START_ADDR + i, src[i]);
    }
    eeprom_buffer_flush();  // satu kali hapus sektor + tulis ulang

    xTaskResumeAll();
#endif
}

void storage_loadFactoryDefaults(CalibrationParams& params) {
    params.magicHeader     = CALIB_MAGIC_KEY;
    params.tdsKFactor      = TDS_KFACTOR_DEFAULT;
    params.turbidityVClear = TURBIDITY_VCLEAR_DEFAULT;
    params.tempOffset      = TEMP_OFFSET_DEFAULT;
}

void storage_clampParams(CalibrationParams& params) {
    // NaN tidak dapat dibandingkan dengan operator biasa, jadi ditangani
    // lebih dulu sebelum penjepitan rentang.
    if (isnan(params.tdsKFactor)) {
        params.tdsKFactor = TDS_KFACTOR_DEFAULT;
    }
    if (isnan(params.turbidityVClear)) {
        params.turbidityVClear = TURBIDITY_VCLEAR_DEFAULT;
    }
    if (isnan(params.tempOffset)) {
        params.tempOffset = TEMP_OFFSET_DEFAULT;
    }

    params.tdsKFactor = constrain(params.tdsKFactor,
                                   TDS_KFACTOR_MIN, TDS_KFACTOR_MAX);

    // Batas atas V_clear adalah tegangan tertinggi yang benar-benar dapat
    // dibaca ADC setelah dikoreksi pembagi tegangan. Nilai di luar rentang
    // ini membuat perhitungan NTU tidak pernah bisa mencapai 0.
    const float vclearMax = ADC_REFERENCE_VOLTAGE * TURBIDITY_INPUT_DIVIDER;
    params.turbidityVClear = constrain(params.turbidityVClear,
                                        TURBIDITY_VCLEAR_MIN, vclearMax);

    params.tempOffset = constrain(params.tempOffset,
                                   -TEMP_OFFSET_LIMIT, TEMP_OFFSET_LIMIT);

    params.magicHeader = CALIB_MAGIC_KEY;
}

void storage_init() {
    EEPROM.get(EEPROM_START_ADDR, g_calibParams);

    if (g_calibParams.magicHeader != CALIB_MAGIC_KEY) {
        // EEPROM belum pernah diinisialisasi: pakai nilai standar pabrik.
        // Penulisan dijadwalkan, bukan dilakukan sekarang, agar boot tidak
        // tertunda oleh siklus hapus flash.
        storage_loadFactoryDefaults(g_calibParams);
        s_pendingParams = g_calibParams;
        s_savePending = true;
        return;
    }

    // Header valid, tetapi isinya tetap diperiksa.
    CalibrationParams before = g_calibParams;
    storage_clampParams(g_calibParams);

    // Deteksi V_clear firmware lama: default lama 3.0V terlalu rendah untuk
    // sensor 5V + ADC 3.3V. Jika masih di bawah 3.1V, paksa ke default baru
    // supaya user dipaksa kalibrasi ulang setelah update firmware.
    if (g_calibParams.turbidityVClear < 3.1f) {
        g_calibParams.turbidityVClear = TURBIDITY_VCLEAR_DEFAULT;
    }

    if (!paramsEqual(before, g_calibParams)) {
        s_pendingParams = g_calibParams;
        s_savePending = true;
    }
}

void storage_requestSave(const CalibrationParams& params) {
    s_pendingParams = params;
    storage_clampParams(s_pendingParams);
    s_savePending = true;
}

bool storage_processPendingSave() {
    if (!s_savePending) {
        return false;
    }
    s_savePending = false;

    // Lewati penulisan bila isi flash sudah sama dengan nilai yang diminta.
    CalibrationParams stored;
    EEPROM.get(EEPROM_START_ADDR, stored);
    if (paramsEqual(stored, s_pendingParams)) {
        return false;
    }

    writeToFlash(s_pendingParams);
    return true;
}
