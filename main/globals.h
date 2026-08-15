/**
 * @file    globals.h
 * @brief   Deklarasi seluruh struktur data global, enum sistem, serta
 *          handle FreeRTOS (mutex/queue) yang dipakai lintas modul.
 * @details Modul lain tidak boleh mendefinisikan variabel global sendiri
 *          di luar file ini. Semua akses ke SensorData dan SystemState
 *          WAJIB dilindungi oleh g_dataMutex agar aman terhadap race
 *          condition antar task FreeRTOS.
 */

#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <STM32FreeRTOS.h>
#include "config.h"
#include "fuzzy_kualitas_air.h"
#include "storage.h"

// =============================================================================
// ENUM — STATUS SENSOR
// =============================================================================
enum class SensorStatus : uint8_t {
    OK = 0,      // Sensor terbaca normal
    ERROR,       // Sensor gagal / tidak terdeteksi
    NOT_USED     // Sensor sengaja tidak dipakai pada parameter aktif
};

// =============================================================================
// ENUM — IDENTITAS TOMBOL
// =============================================================================
enum class ButtonID : uint8_t {
    UP = 0,
    DOWN,
    LEFT,
    RIGHT,
    OK,
    BACK,
    COUNT   // jumlah total tombol, jangan dipakai sebagai ID
};

// =============================================================================
// ENUM — JENIS EVENT TOMBOL
// =============================================================================
enum class ButtonEvent : uint8_t {
    NONE = 0,
    PRESSED,
    RELEASED,
    HOLD,
    REPEAT
};

// =============================================================================
// ENUM — PARAMETER STANDAR PENGUJIAN AIR (2 KATEGORI)
// =============================================================================
enum class WaterParameter : uint8_t {
    AIR_MINUM_HIGIENE = 0,
    PEMANDIAN_KOLAM,
    COUNT
};

// =============================================================================
// ENUM — STATE HALAMAN GUI (Finite State Machine)
// =============================================================================
enum class MenuState : uint8_t {
    SPLASH = 0,
    HOME,
    WAITING_SAMPLING,
    MEASUREMENT,
    CALIBRATION,
    CALIBRATION_TDS,
    CALIBRATION_TURBIDITY,
    CALIBRATION_TEMPERATURE,
    SETTINGS,
    ABOUT,
    COUNT   // jumlah total state, dipakai untuk ukuran dispatch table
};

// =============================================================================
// STRUCT — DATA HASIL PEMBACAAN SELURUH SENSOR & HASIL FUZZY
// =============================================================================
struct SensorData {
    float temperature;          // Celsius (sudah termasuk offset kalibrasi)
    float temperatureRaw;        // Celsius sebelum offset kalibrasi
    uint16_t tdsRaw;             // ADC mentah 0-4095
    float tdsVoltage;            // tegangan sisi sensor setelah koreksi divider
    float tdsFiltered;           // TDS hasil konversi + kalibrasi (ppm)
    uint16_t turbidityRaw;       // ADC mentah 0-4095
    float turbidityVoltage;      // tegangan sisi sensor setelah koreksi divider
    float turbidityFiltered;     // kekeruhan hasil konversi (NTU)

    // --- Hasil Olahan Fuzzy Logic ---
    float tdsCompensated;        // TDS setelah kompensasi suhu
    float fuzzyScore;            // Skor Fuzzy Sugeno (0-100)
    KualitasAir_t qualityStatus; // STATUS_LAYAK, STATUS_LTM, STATUS_TL
    StatusSuhu_t tempStatus;     // SUHU_NORMAL, SUHU_ABNORMAL

    SensorStatus temperatureStatus;
    SensorStatus tdsStatus;
    SensorStatus turbidityStatus;
};

// =============================================================================
// STRUCT — STATUS RINGKAS TOMBOL (untuk keperluan debug/serial)
// =============================================================================
struct ButtonState {
    bool pressed;   // baru saja ditekan
    bool released;  // baru saja dilepas
    bool hold;      // sedang ditahan lama
    bool repeat;    // event pengulangan saat ditahan
};

// =============================================================================
// STRUCT — PESAN EVENT TOMBOL (dikirim lewat queue Button -> GUI)
// =============================================================================
struct ButtonEventMsg {
    ButtonID id;
    ButtonEvent event;
};

// =============================================================================
// STRUCT — STATE SISTEM SECARA KESELURUHAN
// =============================================================================
struct SystemState {
    WaterParameter activeParameter;   // parameter pengujian air yang aktif
    MenuState currentMenu;            // halaman GUI yang sedang tampil
    MenuState previousMenu;           // halaman sebelumnya (untuk tombol BACK)
    uint8_t cursorIndex;              // index kursor pada menu berjalan
    uint8_t measurementSubPage;       // 0 = Data Sensor + Skor, 1 = Detail Fuzzy & Rekomendasi

    uint16_t calibTdsTarget;          // nilai acuan larutan TDS pada layar kalibrasi
    bool calibSaving;                 // true saat pesan "Menyimpan..." perlu tampil

    uint8_t settingsBrightness;       // 10-255
    uint8_t settingsContrast;         // 10-255
    bool settingsAdjustMode;          // true = LEFT/RIGHT mengubah nilai setting

    bool systemOK;                    // status kesehatan sistem keseluruhan
    bool displayDirty;                // true jika OLED perlu digambar ulang
};

// =============================================================================
// VARIABEL GLOBAL (didefinisikan di globals.cpp)
// =============================================================================
extern SensorData   g_sensorData;
extern ButtonState   g_buttonStates[static_cast<uint8_t>(ButtonID::COUNT)];
extern SystemState  g_systemState;

extern SemaphoreHandle_t g_dataMutex;         // melindungi g_sensorData & g_systemState
extern QueueHandle_t     g_buttonEventQueue;  // producer: TaskButton, consumer: TaskGUI

// Lama tunggu standar saat mengambil g_dataMutex. Semua modul memakai nilai
// yang sama agar perilaku penguncian konsisten dan mudah ditelusuri.
constexpr TickType_t DATA_MUTEX_TIMEOUT = pdMS_TO_TICKS(50);

/**
 * @brief Mengembalikan profil baku mutu fuzzy untuk parameter air tertentu.
 *        Selalu mengembalikan pointer valid; parameter di luar rentang
 *        dipetakan ke profil Higiene Sanitasi.
 */
const FuzzyProfil_t* globals_getProfile(WaterParameter param);

/**
 * @brief Inisialisasi seluruh variabel global ke nilai default dan
 *        membuat objek FreeRTOS (mutex, queue) yang dipakai bersama.
 *        Wajib dipanggil satu kali dari setup() sebelum task dibuat.
 * @return true jika seluruh objek FreeRTOS berhasil dibuat.
 */
bool globals_init();

#endif // GLOBALS_H
