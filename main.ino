/**
 * @file    main.ino
 * @brief   Firmware Water Quality Analyzer — entry point Arduino.
 * @details Target hardware : STM32F401CCU6 (Blackpill)
 *          Framework       : STM32duino (Arduino Core STM32)
 *          RTOS            : STM32FreeRTOS
 *
 *          setup() hanya bertugas menginisialisasi seluruh modul lalu
 *          membuat task dan menjalankan scheduler. Setelah
 *          vTaskStartScheduler() dipanggil, kendali program sepenuhnya
 *          berada pada FreeRTOS dan loop() tidak pernah dieksekusi
 *          (ini adalah pola standar aplikasi FreeRTOS pada Arduino).
 *
 * Library yang dibutuhkan (install via Arduino Library Manager):
 *   - STM32duino FreeRTOS (STM32FreeRTOS)
 *   - U8g2
 *   - OneWire
 *   - DallasTemperature
 *
 * Board package  : STM32 MCU based boards (STMicroelectronics), pilih
 *                  board "Generic STM32F4 series" -> "BlackPill F401CC".
 */

#include "config.h"
#include "globals.h"
#include "sensors.h"
#include "buttons.h"
#include "display.h"
#include "gui.h"
#include "tasks.h"

/**
 * @brief Inisialisasi seluruh perangkat keras dan perangkat lunak, lalu
 *        membuat task FreeRTOS dan menjalankan scheduler.
 */
void setup() {
    Serial1.begin(SERIAL_BAUD_RATE);

    globals_init();   // state global + mutex + queue
    buttons_init();    // GPIO tombol
    sensors_init();    // ADC + OneWire + DS18B20
    display_init();    // I2C + OLED SSD1306
    gui_init();         // FSM GUI mulai dari Splash Screen

    // --- TEST VALIDASI TEMPATAN (BASELINE MATLAB: TDS=350, Turbidity=10) ---
    float testTds = 350.0f;
    float testTurb = 10.0f;
    float testSkor = FuzzyKualitasAir_HitungSkor(testTds, testTurb);
    KualitasAir_t testStatus = FuzzyKualitasAir_GetStatus(testSkor);

    Serial1.println(F("========================================"));
    Serial1.println(F("    VALIDASI AUTOMATIS FUZZY LOGIC    "));
    Serial1.println(F("========================================"));
    Serial1.print(F("Input Test  : TDS = 350.0 ppm, Turbidity = 10.0 NTU\n"));
    Serial1.print(F("Hasil Skor  : ")); Serial1.println(testSkor, 1); // Harus ≈ 32.8
    Serial1.print(F("Status Label: ")); Serial1.println(FuzzyKualitasAir_GetPesan(testStatus)); // Target: LTM
    Serial1.println(F("========================================"));

    tasks_createAll(); // buat seluruh task FreeRTOS

    vTaskStartScheduler(); // alih kendali penuh ke FreeRTOS

    // Baris di bawah ini seharusnya tidak pernah tercapai. Jika tercapai,
    // berarti pembuatan scheduler gagal (mis. heap FreeRTOS tidak cukup).
    Serial1.println(F("FATAL: vTaskStartScheduler() gagal dijalankan."));
    while (true) {
        // Sengaja tidak menggunakan delay(); ini adalah kondisi fatal
        // yang seharusnya tidak pernah terjadi pada operasi normal.
    }
}

/**
 * @brief Tidak dipakai. Seluruh eksekusi program berjalan di dalam task
 *        FreeRTOS setelah scheduler dimulai di setup().
 */
void loop() {
    // Intentionally empty — FreeRTOS scheduler mengambil alih eksekusi.
}
