/**
 * @file    main.ino
 * @brief   Firmware Water Quality Analyzer — entry point Arduino.
 * @details Target hardware : STM32F401CCU6 (Blackpill)
 *          Framework       : STM32duino (Arduino Core STM32)
 *          RTOS            : STM32FreeRTOS
 */

#include "config.h"
#include "globals.h"
#include "sensors.h"
#include "buttons.h"
#include "display.h"
#include "gui.h"
#include "tasks.h"

void setup() {
    Serial1.begin(SERIAL_BAUD_RATE);

    if (!globals_init()) {
        // Mutex atau queue gagal dibuat (heap FreeRTOS tidak cukup atau
        // kerusakan memori). Tidak bisa lanjut: cetak pesan fatal dan henti.
        Serial1.println(F("FATAL: globals_init() gagal. Free heap mungkin habis."));
        while (true) {}
    }

    buttons_init();
    sensors_init();
    display_init();
    gui_init();

    // --- TEST VALIDASI BASELINE (TDS=350, Turb=10 → 0.50 [Poor]) ---
    float testTds = 350.0f;
    float testTurb = 10.0f;
    float testSkor = FuzzyKualitasAir_HitungSkor(testTds, testTurb);
    KualitasAir_t testStatus = FuzzyKualitasAir_GetStatus(testSkor);

    Serial1.println(F("========================================"));
    Serial1.println(F("    VALIDASI AUTOMATIS FUZZY LOGIC    "));
    Serial1.println(F("========================================"));
    Serial1.print(F("Input Test  : TDS = 350.0 ppm, Turbidity = 10.0 NTU\n"));
    Serial1.print(F("Hasil Skor  : ")); Serial1.println(testSkor, 2);
    Serial1.print(F("Status Badge: ")); Serial1.println(FuzzyKualitasAir_GetStatusBadge(testStatus));
    Serial1.print(F("Status Pesan: ")); Serial1.println(FuzzyKualitasAir_GetPesan(testStatus));
    Serial1.println(F("========================================"));

    // Test kasus ekstrem: air jernih ideal → harus EXCELLENT (1.00).
    float testSkorLayak = FuzzyKualitasAir_HitungSkor(50.0f, 0.5f);
    KualitasAir_t statusLayak = FuzzyKualitasAir_GetStatus(testSkorLayak);
    Serial1.print(F("Air Jernih (50 ppm, 0.5 NTU): "));
    Serial1.print(testSkorLayak, 2);
    Serial1.print(F(" ["));
    Serial1.print(FuzzyKualitasAir_GetStatusBadge(statusLayak));
    Serial1.println(F("]"));

    // Test kasus ekstrem: air buruk → harus NOT_SUITABLE (0.00).
    float testSkorBuruk = FuzzyKualitasAir_HitungSkor(1100.0f, 28.0f);
    KualitasAir_t statusBuruk = FuzzyKualitasAir_GetStatus(testSkorBuruk);
    Serial1.print(F("Air Buruk (1100 ppm, 28 NTU): "));
    Serial1.print(testSkorBuruk, 2);
    Serial1.print(F(" ["));
    Serial1.print(FuzzyKualitasAir_GetStatusBadge(statusBuruk));
    Serial1.println(F("]"));
    Serial1.println(F("========================================"));

    if (!tasks_createAll()) {
        Serial1.println(F("FATAL: tasks_createAll() gagal. Heap tidak cukup."));
        while (true) {}
    }

    vTaskStartScheduler();

    // Baris di bawah ini seharusnya tidak pernah tercapai.
    Serial1.println(F("FATAL: vTaskStartScheduler() gagal dijalankan."));
    while (true) {}
}

void loop() {
    // Intentionally empty — FreeRTOS scheduler mengambil alih eksekusi.
}
