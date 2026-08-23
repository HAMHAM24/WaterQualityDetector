#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <STM32FreeRTOS.h>
#include "fuzzy_kualitas_air.h"

// =============================================================================
// INFORMASI FIRMWARE / HARDWARE
// =============================================================================
constexpr const char* FIRMWARE_NAME     = "Water Quality Analyzer";
constexpr const char* FIRMWARE_VERSION  = "2.0.0 (Permenkes 2023)";
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
// PIN MAPPING — OLED SH1106 (I2C)
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
constexpr uint16_t TDS_CALIB_TARGET_STEP    = 5;

// =============================================================================
// KALIBRASI SENSOR TURBIDITY
// =============================================================================
constexpr float TURBIDITY_VCLEAR_DEFAULT = 3.3f;   
constexpr float TURBIDITY_VCLEAR_MIN     = 0.5f;   
constexpr float TURBIDITY_NTU_PER_VOLT   = 30.0f;  

// =============================================================================
// KALIBRASI OFFSET SUHU
// =============================================================================
constexpr float TEMP_OFFSET_DEFAULT = 0.0f;
constexpr float TEMP_OFFSET_STEP    = 0.1f;   
constexpr float TEMP_OFFSET_LIMIT   = 5.0f;   

// =============================================================================
// PROFIL BAKU MUTU PER PERUNTUKAN AIR (Permenkes 2023)
// =============================================================================
constexpr uint8_t WATER_PROFILE_COUNT = 3;

constexpr FuzzyProfil_t WATER_QUALITY_PROFILES[WATER_PROFILE_COUNT] = {
    // [0] AIR MINUM (TDS, Turb, Deviasi Suhu dari ruang)
    {
        150.0f, 250.0f,          // TDS 0
        150.0f, 300.0f, 450.0f,  // TDS 1
        300.0f, 450.0f, 2000.0f, // TDS 2

        1.5f, 2.5f,              // Turb 0
        1.5f, 3.0f, 5.0f,        // Turb 1
        4.0f, 6.0f, 100.0f,      // Turb 2

        1.0f, 3.0f,              // Temp 0 (Deviasi <= 3)
        2.0f, 4.0f, 6.0f,        // Temp 1 
        5.0f, 8.0f, 50.0f,       // Temp 2

        0.875f, 0.625f, 0.375f, 0.125f // Thresholds
    },

    // [1] HIGIENE SANITASI (TDS, Turbidity. dTemp diabaikan/identik dgn Air Minum)
    {
        150.0f, 250.0f,          
        150.0f, 300.0f, 450.0f,  
        300.0f, 450.0f, 2000.0f, 

        1.5f, 2.5f,              
        1.5f, 3.0f, 5.0f,        
        4.0f, 6.0f, 100.0f,      

        1.0f, 3.0f,              
        2.0f, 4.0f, 6.0f,        
        5.0f, 8.0f, 50.0f,       

        0.875f, 0.625f, 0.375f, 0.125f
    },

    // [2] PEMANDIAN UMUM (Deviasi Suhu dari 15-35, Turbidity sbg kejernihan. TDS bypass)
    {
        0.0f, 0.0f,              // TDS (diabaikan)
        0.0f, 0.0f, 0.0f,        
        0.0f, 0.0f, 0.0f,        

        10.0f, 20.0f,            // Turb 0 (Lebih longgar)
        15.0f, 30.0f, 50.0f,     // Turb 1
        40.0f, 60.0f, 500.0f,    // Turb 2

        1.0f, 3.0f,              // Temp 0 (Deviasi dari rentang mutlak 15-35)
        2.0f, 5.0f, 8.0f,        // Temp 1
        6.0f, 10.0f, 30.0f,      // Temp 2

        0.875f, 0.625f, 0.375f, 0.125f
    }
};

// Konstanta Suhu Udara/Ruangan Referensi untuk Air Minum & Higiene Sanitasi
constexpr float BASE_ROOM_TEMP = 28.0f;

// =============================================================================
// AMBANG BATAS CRISP PEMANDIAN UMUM (Permenkes 2/2023 Tabel 10)
// -----------------------------------------------------------------------------
// Khusus mode Pemandian Umum, evaluasi tidak melalui sistem fuzzy melainkan
// pengecekan ambang batas langsung (threshold checker).
// =============================================================================
constexpr float PEMANDIAN_SUHU_MIN  = 15.0f;  // Permenkes 2/2023 Tabel 10: 15-35 C
constexpr float PEMANDIAN_SUHU_MAX  = 35.0f;  // Permenkes 2/2023 Tabel 10: 15-35 C
constexpr float PEMANDIAN_TURB_MAX  = 50.0f;  // Proksi rekayasa kekeruhan (< 50 NTU)

// =============================================================================
// DISPLAY OLED 1.3" SH1106 128x64
// =============================================================================
constexpr uint8_t DISPLAY_WIDTH       = 128;
constexpr uint8_t DISPLAY_HEIGHT      = 64;
constexpr uint8_t DISPLAY_HEADER_H    = 13;   
constexpr uint8_t DISPLAY_STATUSBAR_H = 10;   

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
constexpr TickType_t TASK_PERIOD_GUI_MS          = 50;    
constexpr TickType_t TASK_PERIOD_OLED_MS         = 100;   
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
// TATA LETAK DAFTAR MENU
// =============================================================================
constexpr uint8_t MENU_LINE_HEIGHT    = 10;
constexpr uint8_t MENU_VISIBLE_ROWS   = 4;
constexpr uint8_t MENU_FIRST_LINE_Y   = DISPLAY_HEADER_H + 8;   
constexpr uint8_t MENU_LAST_LINE_Y    = DISPLAY_HEIGHT - DISPLAY_STATUSBAR_H - 1; 

// =============================================================================
// LAIN-LAIN
// =============================================================================
constexpr uint32_t SERIAL_BAUD_RATE   = 115200;
constexpr TickType_t SPLASH_SCREEN_MS = 2000;
constexpr TickType_t SAMPLING_SCREEN_MS = 5000; 
constexpr uint8_t  BUTTON_EVENT_QUEUE_LENGTH = 8;

#endif // CONFIG_H
