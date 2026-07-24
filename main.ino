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
