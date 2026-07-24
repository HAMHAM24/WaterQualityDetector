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
// Nilai dipilih konservatif untuk efisiensi RAM STM32F401CCU6 (64 KB SRAM).
// =============================================================================
constexpr uint16_t STACK_SIZE_BUTTON        = 128;
constexpr uint16_t STACK_SIZE_TEMPERATURE   = 160;
constexpr uint16_t STACK_SIZE_WATER_SENSOR  = 160;
constexpr uint16_t STACK_SIZE_GUI           = 192;
constexpr uint16_t STACK_SIZE_OLED          = 256;
constexpr uint16_t STACK_SIZE_SERIAL_DEBUG  = 192;

// =============================================================================
// LAIN-LAIN
// =============================================================================
constexpr uint32_t SERIAL_BAUD_RATE   = 115200;
constexpr TickType_t SPLASH_SCREEN_MS = 2000;
constexpr uint8_t  BUTTON_EVENT_QUEUE_LENGTH = 8;

#endif // CONFIG_H