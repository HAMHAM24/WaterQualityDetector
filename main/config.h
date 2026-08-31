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
constexpr const char* DISPLAY_NAME      = "OLED 1.3\" SH1106";

#define OLED_USE_SH1106  1

// =============================================================================
// PIN MAPPING — UART (FTDI / Serial Monitor)
// =============================================================================
constexpr uint8_t PIN_UART_TX = PA_9;   
constexpr uint8_t PIN_UART_RX = PA_10;  

// =============================================================================
// PIN MAPPING — OLED SH1106 / SSD1306 (I2C)
// =============================================================================
constexpr uint8_t PIN_OLED_SCL = PB_8;
constexpr uint8_t PIN_OLED_SDA = PB_9;

// =============================================================================
// PIN MAPPING — SENSOR
// =============================================================================
constexpr uint8_t PIN_DS18B20        = PB_10;  
constexpr uint8_t PIN_TDS_ANALOG     = PA_0;   
constexpr uint8_t PIN_TURBIDITY_ANALOG = PA_1; 

// =============================================================================
// PIN MAPPING — PUSH BUTTON (Active LOW, Internal Pull-Up)
// =============================================================================
constexpr uint8_t PIN_BTN_UP    = PB_14;
constexpr uint8_t PIN_BTN_DOWN  = PA_8;
constexpr uint8_t PIN_BTN_LEFT  = PB_0;   
constexpr uint8_t PIN_BTN_RIGHT = PB_13;
constexpr uint8_t PIN_BTN_OK    = PB_12;
constexpr uint8_t PIN_BTN_BACK  = PB_11;

constexpr uint8_t BUTTON_COUNT = 6;

// =============================================================================
// PARAMETER ADC & SIGNAL CONDITIONING
// =============================================================================
constexpr uint8_t  ADC_RESOLUTION_BITS = 12;               
constexpr uint16_t ADC_MAX_VALUE       = 4095;              
constexpr float    ADC_REFERENCE_VOLTAGE = 3.3f;
constexpr uint8_t FILTER_SAMPLE_COUNT = 20;

constexpr float TDS_INPUT_DIVIDER       = 1.0f;  
constexpr float TURBIDITY_INPUT_DIVIDER = 1.0f;  

// =============================================================================
// KALIBRASI SENSOR TDS
// =============================================================================
constexpr float TDS_POLY_C3 = 133.42f;
constexpr float TDS_POLY_C2 = -255.86f;
constexpr float TDS_POLY_C1 = 857.39f;
constexpr float TDS_EC_TO_PPM_FACTOR = 0.5f;

constexpr float TDS_KFACTOR_DEFAULT = 1.0f;
constexpr float TDS_KFACTOR_MIN     = 0.20f;    
constexpr float TDS_KFACTOR_MAX     = 5.00f;

constexpr uint16_t TDS_CALIB_TARGET_DEFAULT = 707;  
constexpr uint16_t TDS_CALIB_TARGET_MIN     = 50;
constexpr uint16_t TDS_CALIB_TARGET_MAX     = 2000;
constexpr uint16_t TDS_CALIB_TARGET_STEP    = 1;   // increment/decrement +- 1 ppm

// =============================================================================
// KALIBRASI SENSOR TURBIDITY
// =============================================================================
constexpr float TURBIDITY_VCLEAR_DEFAULT = 3.3f;   
constexpr float TURBIDITY_VCLEAR_MIN     = 0.5f;   
constexpr float TURBIDITY_NTU_PER_VOLT   = 30.0f;  
constexpr float TURBIDITY_VSTANDARD_DEFAULT = 0.0f; // 0 = titik kedua belum dikalibrasi
constexpr float TURBIDITY_NTU_STANDARD_DEFAULT = 100.0f;
constexpr uint16_t TURBIDITY_NTU_STANDARD_MIN  = 1;
constexpr uint16_t TURBIDITY_NTU_STANDARD_MAX  = 3000;
constexpr uint16_t TURBIDITY_NTU_STANDARD_STEP = 5;    // increment/decrement +- 5 NTU
constexpr float TURBIDITY_MIN_CALIBRATION_DELTA_V = 0.02f;

// =============================================================================
// KALIBRASI OFFSET SUHU
// =============================================================================
constexpr float TEMP_OFFSET_DEFAULT = 0.0f;
constexpr float TEMP_OFFSET_STEP    = 0.1f;   
constexpr float TEMP_OFFSET_LIMIT   = 5.0f;   

// =============================================================================
// PROFIL BAKU MUTU FUZZY (AIR MINUM & HIGIENE SANITASI)
// =============================================================================
constexpr uint8_t WATER_PROFILE_COUNT = 1;

constexpr FuzzyProfil_t WATER_QUALITY_PROFILES[WATER_PROFILE_COUNT] = {
    // [0] AIR MINUM & HIGIENE: TDS, Turbidity, Delta Suhu terhadap udara manual.
    {
        150.0f, 225.0f, 150.0f, 225.0f, 300.0f,  // TDS SL, PS
        225.0f, 300.0f, 450.0f, 300.0f, 450.0f, 600.0f, // TDS PI, TL
        1.5f, 2.25f, 1.5f, 2.25f, 3.0f,             // Turb SL, PS
        2.25f, 3.0f, 4.5f, 3.0f, 4.5f, 25.0f,       // Turb PI, TL
        1.0f, 1.5f, 1.0f, 1.75f, 2.5f,              // Delta T SL, PS
        2.0f, 2.75f, 3.5f, 3.0f, 4.0f, 10.0f,       // Delta T PI, TL
        0.83f, 0.50f, 0.17f                         // Threshold output
    }
};

// Referensi kompensasi sensor TDS standar adalah 25 C; bukan input fuzzy suhu.
constexpr float TDS_TEMP_REFERENCE = 25.0f;
constexpr float AMBIENT_TEMP_DEFAULT = 25.0f;
constexpr float AMBIENT_TEMP_MIN = 10.0f;
constexpr float AMBIENT_TEMP_MAX = 45.0f;
constexpr float AMBIENT_TEMP_FINE_STEP = 0.1f;
constexpr float AMBIENT_TEMP_COARSE_STEP = 1.0f;
constexpr float TEMP_STABLE_DELTA_C = 0.2f;
constexpr uint8_t TEMP_STABLE_REQUIRED_SAMPLES = 3;
constexpr uint32_t TEMP_STABILIZATION_TIMEOUT_MS = 60000;

// =============================================================================
// AMBANG BATAS CRISP PEMANDIAN / KOLAM (Non-Fuzzy Threshold Checker)
// -----------------------------------------------------------------------------
// Sesuai kesepakatan gabungan konservatif:
//   - Suhu: 16.0 - 35.0 C (Pemandian 15-35 C, Kolam 16-40 C -> gabungan: 16-35 C)
//   - Turbidity: < 0.5 NTU (Standar kejernihan air kolam renang)
//   - TDS: Tidak dievaluasi (bypass), nilai tetap ditampilkan di OLED
// =============================================================================
constexpr float PEMANDIAN_KOLAM_SUHU_MIN = 16.0f;  // Batas minimum suhu aman
constexpr float PEMANDIAN_KOLAM_SUHU_MAX = 35.0f;  // Batas maksimum suhu aman
constexpr float PEMANDIAN_KOLAM_TURB_MAX = 0.5f;   // Batas maksimum kekeruhan (< 0.5 NTU)

// =============================================================================
// DISPLAY OLED 1.3" / 0.96" 128x64
// =============================================================================
constexpr uint8_t DISPLAY_WIDTH       = 128;
constexpr uint8_t DISPLAY_HEIGHT      = 64;
constexpr uint8_t DISPLAY_HEADER_H    = 12;   
constexpr uint8_t DISPLAY_STATUSBAR_H = 8;    

constexpr uint8_t DISPLAY_DEFAULT_CONTRAST   = 128; 
constexpr uint8_t DISPLAY_DEFAULT_BRIGHTNESS = 200; 
constexpr uint8_t DISPLAY_MIN_LEVEL = 10;
constexpr uint8_t DISPLAY_MAX_LEVEL = 255;
constexpr uint8_t DISPLAY_LEVEL_STEP = 15;

// =============================================================================
// TIMING TOMBOL 
// =============================================================================
constexpr TickType_t BUTTON_DEBOUNCE_MS      = 30;
constexpr TickType_t BUTTON_HOLD_MS          = 600;
constexpr TickType_t BUTTON_REPEAT_MS        = 150;

// =============================================================================
// PERIODE TASK (dalam milidetik) 
// =============================================================================
constexpr TickType_t TASK_PERIOD_BUTTON_MS       = 15;    
constexpr TickType_t TASK_PERIOD_TEMPERATURE_MS  = 1000;  
constexpr TickType_t TASK_PERIOD_WATER_SENSOR_MS = 200;   
constexpr TickType_t TASK_PERIOD_GUI_MS          = 35;    
constexpr TickType_t TASK_PERIOD_OLED_MS         = 40;   
constexpr TickType_t TASK_PERIOD_SERIAL_DEBUG_MS = 1000;  

constexpr TickType_t DS18B20_CONVERSION_MS = 750;

// =============================================================================
// PRIORITAS TASK FreeRTOS
// =============================================================================
constexpr UBaseType_t TASK_PRIORITY_BUTTON        = tskIDLE_PRIORITY + 4; 
constexpr UBaseType_t TASK_PRIORITY_GUI            = tskIDLE_PRIORITY + 3;
constexpr UBaseType_t TASK_PRIORITY_OLED           = tskIDLE_PRIORITY + 2;
constexpr UBaseType_t TASK_PRIORITY_WATER_SENSOR   = tskIDLE_PRIORITY + 2;
constexpr UBaseType_t TASK_PRIORITY_TEMPERATURE    = tskIDLE_PRIORITY + 1;
constexpr UBaseType_t TASK_PRIORITY_SERIAL_DEBUG   = tskIDLE_PRIORITY + 1;

// =============================================================================
// UKURAN STACK TASK
// =============================================================================
constexpr uint16_t STACK_SIZE_BUTTON        = 160;
constexpr uint16_t STACK_SIZE_TEMPERATURE   = 192;
constexpr uint16_t STACK_SIZE_WATER_SENSOR  = 256;
constexpr uint16_t STACK_SIZE_GUI           = 320;
constexpr uint16_t STACK_SIZE_OLED          = 512;
constexpr uint16_t STACK_SIZE_SERIAL_DEBUG  = 352;

// =============================================================================
// TATA LETAK DAFTAR MENU (Font proporsional & nyaman untuk 128x64)
// =============================================================================
constexpr uint8_t MENU_LINE_HEIGHT    = 11;
constexpr uint8_t MENU_VISIBLE_ROWS   = 4;
constexpr uint8_t MENU_FIRST_LINE_Y   = DISPLAY_HEADER_H + 9;   
constexpr uint8_t MENU_LAST_LINE_Y    = DISPLAY_HEIGHT - DISPLAY_STATUSBAR_H - 1; 

// =============================================================================
// LAIN-LAIN
// =============================================================================
constexpr uint32_t SERIAL_BAUD_RATE   = 115200;
constexpr TickType_t BOOT_ANIMATION_MS = 5000;
constexpr TickType_t SPLASH_SCREEN_MS  = 2000;
constexpr TickType_t SAMPLING_SCREEN_MS = 5000; 
constexpr uint8_t  BUTTON_EVENT_QUEUE_LENGTH = 8;

#endif // CONFIG_H
