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
    Serial.setRx(PIN_UART_RX);
    Serial.setTx(PIN_UART_TX);
    Serial.begin(SERIAL_BAUD_RATE);

    if (!globals_init()) {
        // Mutex atau queue gagal dibuat (heap FreeRTOS tidak cukup atau
        // kerusakan memori). Tidak bisa lanjut: cetak pesan fatal dan henti.
        Serial.println(F("FATAL: globals_init() gagal. Free heap mungkin habis."));
        while (true) {}
    }

    buttons_init();
    sensors_init();
    display_init();
    gui_init();

    // --- TEST VALIDASI BASELINE (TDS=350, Turb=10, Suhu=28) ---
    float testTds = 350.0f;
    float testTurb = 10.0f;
    float testSuhu = 28.0f;
    float testSkor = FuzzyKualitasAir_HitungSkor(testTds, testTurb, testSuhu);
    KualitasAir_t testStatus = FuzzyKualitasAir_GetStatus(testSkor);

    Serial.println(F("========================================"));
    Serial.println(F("    VALIDASI AUTOMATIS FUZZY LOGIC    "));
    Serial.println(F("  3 INPUT: TDS + Turbidity + Suhu     "));
    Serial.println(F("========================================"));
    Serial.print(F("Input  : TDS=350 Turb=10 Suhu=28C\n"));
    Serial.print(F("Skor   : ")); Serial.print(testSkor, 2);
    Serial.print(F(" [")); Serial.print(FuzzyKualitasAir_GetStatusBadge(testStatus));
    Serial.println(F("]"));

    // Test 1: air jernih ideal + suhu normal → harus EXCELLENT
    float s1 = FuzzyKualitasAir_HitungSkor(50.0f, 0.5f, 28.0f);
    Serial.print(F("Ideal   (50/0.5/28C): "));
    Serial.print(s1, 2);
    Serial.print(F(" [")); Serial.print(FuzzyKualitasAir_GetStatusBadge(FuzzyKualitasAir_GetStatus(s1)));
    Serial.println(F("]"));

    // Test 2: air jernih ideal + dingin → harus GOOD (penalti suhu)
    float s2 = FuzzyKualitasAir_HitungSkor(50.0f, 0.5f, 22.0f);
    Serial.print(F("Dingin  (50/0.5/22C): "));
    Serial.print(s2, 2);
    Serial.print(F(" [")); Serial.print(FuzzyKualitasAir_GetStatusBadge(FuzzyKualitasAir_GetStatus(s2)));
    Serial.println(F("]"));

    // Test 3: air jernih ideal + panas → harus GOOD (penalti suhu)
    float s3 = FuzzyKualitasAir_HitungSkor(50.0f, 0.5f, 34.0f);
    Serial.print(F("Panas   (50/0.5/34C): "));
    Serial.print(s3, 2);
    Serial.print(F(" [")); Serial.print(FuzzyKualitasAir_GetStatusBadge(FuzzyKualitasAir_GetStatus(s3)));
    Serial.println(F("]"));

    // Test 4: air buruk → harus NOT_SUITABLE
    float s4 = FuzzyKualitasAir_HitungSkor(1100.0f, 28.0f, 28.0f);
    Serial.print(F("Buruk   (1100/28/28C): "));
    Serial.print(s4, 2);
    Serial.print(F(" [")); Serial.print(FuzzyKualitasAir_GetStatusBadge(FuzzyKualitasAir_GetStatus(s4)));
    Serial.println(F("]"));
    Serial.println(F("========================================"));

    if (!tasks_createAll()) {
        Serial.println(F("FATAL: tasks_createAll() gagal. Heap tidak cukup."));
        while (true) {}
    }

    vTaskStartScheduler();

    // Baris di bawah ini seharusnya tidak pernah tercapai.
    Serial.println(F("FATAL: vTaskStartScheduler() gagal dijalankan."));
    while (true) {}
}

void loop() {
    // Intentionally empty — FreeRTOS scheduler mengambil alih eksekusi.
}
