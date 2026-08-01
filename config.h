/**
 * @file    config.h
 * @brief   Konfigurasi statis firmware: pin mapping, timing task, dan
 *          konstanta perangkat keras untuk STM32F401CCU6 (Blackpill).
 * @details File ini adalah satu-satunya tempat yang boleh berisi "magic
 *          number" perangkat keras. Seluruh modul lain WAJIB mengambil
 *          nilai dari sini agar perubahan hardware cukup dilakukan di
 *          satu tempat (single source of truth).
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <STM32FreeRTOS.h>
#include "fuzzy_kualitas_air.h"

// =============================================================================
// INFORMASI FIRMWARE / HARDWARE
// =============================================================================
constexpr const char* FIRMWARE_NAME     = "Water Quality Analyzer";
constexpr const char* FIRMWARE_VERSION  = "1.0.0";
constexpr const char* HARDWARE_VERSION  = "Rev-A (Blackpill F401CCU6)";
constexpr const char* MCU_NAME          = "STM32F401CCU6";

// =============================================================================
// PIN MAPPING — UART (FTDI) — dipakai otomatis oleh Serial1 STM32duino
// =============================================================================
constexpr uint8_t PIN_UART_RX = PA_9;   // FTDI TX -> MCU RX
constexpr uint8_t PIN_UART_TX = PA_10;  // FTDI RX -> MCU TX

// =============================================================================
// PIN MAPPING — OLED SSD1306 (I2C)
// =============================================================================
constexpr uint8_t PIN_OLED_SCL = PB_8;
constexpr uint8_t PIN_OLED_SDA = PB_9;

// =============================================================================
// PIN MAPPING — SENSOR
// =============================================================================
constexpr uint8_t PIN_DS18B20        = PB_10;  // OneWire data
constexpr uint8_t PIN_TDS_ANALOG     = PA_0;   // DFRobot TDS analog
constexpr uint8_t PIN_TURBIDITY_ANALOG = PA_1; // SEN0189 analog

// =============================================================================
// PIN MAPPING — PUSH BUTTON (Active LOW, Internal Pull-Up)
// =============================================================================
constexpr uint8_t PIN_BTN_UP    = PB_12;
constexpr uint8_t PIN_BTN_DOWN  = PB_13;
constexpr uint8_t PIN_BTN_LEFT  = PB_14;
constexpr uint8_t PIN_BTN_RIGHT = PB_15;
constexpr uint8_t PIN_BTN_OK    = PA_8;
constexpr uint8_t PIN_BTN_BACK  = PB_11;

constexpr uint8_t BUTTON_COUNT = 6;

// =============================================================================
// PARAMETER ADC
// =============================================================================
constexpr uint8_t  ADC_RESOLUTION_BITS = 12;               // STM32F401 ADC 12-bit
constexpr uint16_t ADC_MAX_VALUE       = 4095;              // 2^12 - 1
constexpr float    ADC_REFERENCE_VOLTAGE = 3.3f;

// Jumlah sampel Moving Average untuk sensor analog (minimal 20 sesuai spesifikasi)
constexpr uint8_t FILTER_SAMPLE_COUNT = 20;

// =============================================================================
// PENYESUAIAN RANGKAIAN INPUT ANALOG (SIGNAL CONDITIONING)
// -----------------------------------------------------------------------------
// ADC STM32F401 hanya mampu membaca 0-3.3 V. Bila modul sensor disuplai 5 V,
// output-nya WAJIB diturunkan lewat pembagi tegangan sebelum masuk ADC.
// Konstanta di bawah ini mengembalikan tegangan hasil baca ADC menjadi
// tegangan asli di sisi sensor:
//
//     V_sensor = V_adc * DIVIDER
//
// Nilai yang harus dipakai:
//   1.0f  -> sensor disuplai 3.3 V, output langsung ke ADC (tanpa pembagi)
//   2.0f  -> sensor 5 V dengan pembagi 2:1 (dua resistor sama nilai)
//
// PENTING: nilai 1.0f di bawah adalah ASUMSI SEMENTARA. Konfirmasi ke
// perancang hardware bagaimana output sensor disambungkan, lalu ubah HANYA
// dua angka ini. Seluruh rumus konversi ppm/NTU akan mengikuti otomatis.
// =============================================================================
constexpr float TDS_INPUT_DIVIDER       = 1.0f;  // TODO konfirmasi ke tim hardware
constexpr float TURBIDITY_INPUT_DIVIDER = 1.0f;  // TODO konfirmasi ke tim hardware

// =============================================================================
// KALIBRASI SENSOR TDS (DFRobot SEN0244 / analog TDS meter)
// -----------------------------------------------------------------------------
// Rumus resmi DFRobot mengubah tegangan menjadi nilai TDS:
//
//   ecValue = 133.42*V^3 - 255.86*V^2 + 857.39*V   (dalam uS/cm)
//   tdsValue = ecValue * 0.5                        (faktor konversi EC->TDS)
//
// Kompensasi suhu dilakukan di domain tegangan (bukan di ppm) sesuai contoh
// resmi DFRobot: V_kompensasi = V / (1 + 0.02*(T - 25)).
// =============================================================================
constexpr float TDS_POLY_C3 = 133.42f;
constexpr float TDS_POLY_C2 = -255.86f;
constexpr float TDS_POLY_C1 = 857.39f;
constexpr float TDS_EC_TO_PPM_FACTOR = 0.5f;
constexpr float TDS_TEMP_COEFFICIENT = 0.02f;   // per derajat Celsius
constexpr float TDS_TEMP_REFERENCE   = 25.0f;   // suhu acuan kompensasi
constexpr float TDS_PPM_MAX          = 2000.0f; // batas wajar pembacaan ppm

// Nilai kalibrasi default (dipakai saat EEPROM masih kosong)
constexpr float TDS_KFACTOR_DEFAULT = 1.0f;
constexpr float TDS_KFACTOR_MIN     = 0.20f;    // batas aman K-factor
constexpr float TDS_KFACTOR_MAX     = 5.00f;

// Rentang nilai target larutan acuan pada layar kalibrasi TDS
constexpr uint16_t TDS_CALIB_TARGET_DEFAULT = 707;  // larutan standar 707 ppm
constexpr uint16_t TDS_CALIB_TARGET_MIN     = 50;
constexpr uint16_t TDS_CALIB_TARGET_MAX     = 2000;
constexpr uint16_t TDS_CALIB_TARGET_STEP    = 5;

// =============================================================================
// KALIBRASI SENSOR TURBIDITY (SEN0189)
// -----------------------------------------------------------------------------
// Sensor ini mengeluarkan tegangan TINGGI saat air jernih dan turun saat air
// makin keruh. Kalibrasi dilakukan dengan merekam tegangan air aquades
// (0 NTU) sebagai V_clear, lalu:
//
//   NTU = (V_clear - V_terukur) * TURBIDITY_NTU_PER_VOLT
//
// V_clear default HARUS berada dalam rentang yang benar-benar dapat dicapai
// oleh ADC (0-3.3 V setelah dikalikan divider). Nilai lama 4.1 V mustahil
// tercapai sehingga air paling jernih pun selalu terbaca keruh.
// =============================================================================
constexpr float TURBIDITY_VCLEAR_DEFAULT = 3.0f;   // volt, wajar untuk air jernih
constexpr float TURBIDITY_VCLEAR_MIN     = 0.5f;   // batas bawah nilai kalibrasi valid
constexpr float TURBIDITY_NTU_PER_VOLT   = 200.0f; // slope konversi volt -> NTU
constexpr float TURBIDITY_NTU_MAX        = 30.0f;  // batas atas semesta fuzzy

// =============================================================================
// KALIBRASI OFFSET SUHU (DS18B20)
// =============================================================================
constexpr float TEMP_OFFSET_DEFAULT = 0.0f;
constexpr float TEMP_OFFSET_STEP    = 0.1f;   // per penekanan LEFT/RIGHT
constexpr float TEMP_OFFSET_LIMIT   = 5.0f;   // batas +/- offset yang diizinkan

// =============================================================================
// PROFIL BAKU MUTU PER PERUNTUKAN AIR
// -----------------------------------------------------------------------------
// Urutan array WAJIB sama dengan enum WaterParameter di globals.h.
//
// PERINGATAN: angka di bawah ini adalah DRAF yang perlu diverifikasi terhadap
// dokumen resmi (Permenkes 32/2017 dan penggantinya Permenkes 2/2023).
// Setiap baris ditandai TODO agar mudah dikoreksi. Format tiap profil:
//   { tdsA, tdsB, tdsC, turbA, turbB, turbC, threshLayak, threshLTM }
// dengan A = ambang "baik", B = baku mutu maksimum, C = batas semesta.
// =============================================================================
constexpr uint8_t WATER_PROFILE_COUNT = 4;

constexpr FuzzyProfil_t WATER_QUALITY_PROFILES[WATER_PROFILE_COUNT] = {
    // [0] HIGIENE SANITASI — identik dengan kualitas_air.fis (baseline MATLAB).
    // TODO verifikasi: TDS maks 1000 ppm, kekeruhan maks 25 NTU.
    { 300.0f, 1000.0f, 1200.0f,   5.0f, 25.0f, 30.0f,   75.0f, 25.0f },

    // [1] AIR SPA — mendekati mutu air minum, kekeruhan jauh lebih ketat.
    // TODO verifikasi: kekeruhan maks 1.5 NTU.
    { 200.0f,  500.0f,  700.0f,   0.5f,  1.5f,  5.0f,   75.0f, 25.0f },

    // [2] AIR KOLAM RENANG — kekeruhan paling ketat karena harus tembus pandang.
    // TODO verifikasi: kekeruhan maks 0.5 NTU.
    { 200.0f,  500.0f,  700.0f,   0.2f,  0.5f,  3.0f,   75.0f, 25.0f },

    // [3] PEMANDIAN UMUM — paling longgar (badan air alami).
    // TODO verifikasi: kekeruhan maks 25 NTU.
    { 500.0f, 1000.0f, 1500.0f,   5.0f, 25.0f, 50.0f,   75.0f, 25.0f }
};

// =============================================================================
// DISPLAY OLED SSD1306 128x64
// =============================================================================
constexpr uint8_t DISPLAY_WIDTH       = 128;
constexpr uint8_t DISPLAY_HEIGHT      = 64;
constexpr uint8_t DISPLAY_HEADER_H    = 12;   // tinggi area header (judul halaman)
constexpr uint8_t DISPLAY_STATUSBAR_H = 8;    // tinggi area status bar bawah

constexpr uint8_t DISPLAY_DEFAULT_CONTRAST   = 128; // 0-255
constexpr uint8_t DISPLAY_DEFAULT_BRIGHTNESS = 200; // 0-255 (khusus SSD1306: setContrast juga)
constexpr uint8_t DISPLAY_MIN_LEVEL = 10;
constexpr uint8_t DISPLAY_MAX_LEVEL = 255;
constexpr uint8_t DISPLAY_LEVEL_STEP = 15;

// =============================================================================
// TIMING TOMBOL (debounce, hold, repeat)
// =============================================================================
constexpr TickType_t BUTTON_DEBOUNCE_MS      = 30;
constexpr TickType_t BUTTON_HOLD_MS          = 600;
constexpr TickType_t BUTTON_REPEAT_MS        = 150;

// =============================================================================
// PERIODE TASK (dalam milidetik) — sesuai spesifikasi
// =============================================================================
constexpr TickType_t TASK_PERIOD_BUTTON_MS       = 15;    // 10-20 ms
constexpr TickType_t TASK_PERIOD_TEMPERATURE_MS  = 1000;  // 1000 ms
constexpr TickType_t TASK_PERIOD_WATER_SENSOR_MS = 200;   // 200 ms
constexpr TickType_t TASK_PERIOD_GUI_MS          = 50;    // navigasi responsif
constexpr TickType_t TASK_PERIOD_OLED_MS         = 100;   // 100 ms
constexpr TickType_t TASK_PERIOD_SERIAL_DEBUG_MS = 1000;  // 1000 ms

// Waktu konversi DS18B20 pada resolusi 12-bit (datasheet Maxim: maks 750 ms)
constexpr TickType_t DS18B20_CONVERSION_MS = 750;

// =============================================================================
// PRIORITAS TASK FreeRTOS (semakin besar = semakin prioritas)
// =============================================================================
constexpr UBaseType_t TASK_PRIORITY_BUTTON        = tskIDLE_PRIORITY + 4; // tinggi
constexpr UBaseType_t TASK_PRIORITY_GUI            = tskIDLE_PRIORITY + 3;
constexpr UBaseType_t TASK_PRIORITY_OLED           = tskIDLE_PRIORITY + 2;
constexpr UBaseType_t TASK_PRIORITY_WATER_SENSOR   = tskIDLE_PRIORITY + 2;
constexpr UBaseType_t TASK_PRIORITY_TEMPERATURE    = tskIDLE_PRIORITY + 1;
constexpr UBaseType_t TASK_PRIORITY_SERIAL_DEBUG   = tskIDLE_PRIORITY + 1;

// =============================================================================
// UKURAN STACK TASK (dalam word, 1 word = 4 byte pada ARM Cortex-M)
// -----------------------------------------------------------------------------
// Nilai dinaikkan dari draf awal karena task yang memformat float (dtostrf,
// snprintf) dan memanggil U8g2 membutuhkan ruang stack jauh lebih besar
// daripada perkiraan konservatif. Stack overflow pada FreeRTOS bersifat
// senyap (configCHECK_FOR_STACK_OVERFLOW = 0 secara default), sehingga lebih
// aman melebihkan: total 1792 word = 7 KB, masih ringan untuk 64 KB SRAM.
// =============================================================================
constexpr uint16_t STACK_SIZE_BUTTON        = 160;
constexpr uint16_t STACK_SIZE_TEMPERATURE   = 192;
constexpr uint16_t STACK_SIZE_WATER_SENSOR  = 256;
constexpr uint16_t STACK_SIZE_GUI           = 320;
constexpr uint16_t STACK_SIZE_OLED          = 512;
constexpr uint16_t STACK_SIZE_SERIAL_DEBUG  = 352;

// =============================================================================
// TATA LETAK DAFTAR MENU (dipakai gui.cpp)
// -----------------------------------------------------------------------------
// Area konten = tinggi layar - header - status bar = 64 - 12 - 8 = 44 px.
// Dengan lineHeight 11 px, hanya 4 baris yang benar-benar terlihat. Menu
// dengan item lebih banyak WAJIB memakai viewport bergulir, jika tidak item
// terakhir tidak akan pernah tergambar.
// =============================================================================
constexpr uint8_t MENU_LINE_HEIGHT    = 11;
constexpr uint8_t MENU_VISIBLE_ROWS   = 4;
constexpr uint8_t MENU_FIRST_LINE_Y   = DISPLAY_HEADER_H + 9;

// =============================================================================
// LAIN-LAIN
// =============================================================================
constexpr uint32_t SERIAL_BAUD_RATE   = 115200;
constexpr TickType_t SPLASH_SCREEN_MS = 2000;
constexpr uint8_t  BUTTON_EVENT_QUEUE_LENGTH = 8;

#endif // CONFIG_H
