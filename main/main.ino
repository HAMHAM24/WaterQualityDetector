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

    // --- TEST VALIDASI BASELINE (TDS=350, Turb=10 → 0.50 [Poor]) ---
    float testTds = 350.0f;
    float testTurb = 10.0f;
    float testSkor = FuzzyKualitasAir_HitungSkor(testTds, testTurb);
    KualitasAir_t testStatus = FuzzyKualitasAir_GetStatus(testSkor);

    Serial.println(F("========================================"));
    Serial.println(F("    VALIDASI AUTOMATIS FUZZY LOGIC    "));
    Serial.println(F("========================================"));
    Serial.print(F("Input Test  : TDS = 350.0 ppm, Turbidity = 10.0 NTU\n"));
    Serial.print(F("Hasil Skor  : ")); Serial.println(testSkor, 2);
    Serial.print(F("Status Badge: ")); Serial.println(FuzzyKualitasAir_GetStatusBadge(testStatus));
    Serial.print(F("Status Pesan: ")); Serial.println(FuzzyKualitasAir_GetPesan(testStatus));
    Serial.println(F("========================================"));

    // Test kasus ekstrem: air jernih ideal → harus EXCELLENT (1.00).
    float testSkorLayak = FuzzyKualitasAir_HitungSkor(50.0f, 0.5f);
    KualitasAir_t statusLayak = FuzzyKualitasAir_GetStatus(testSkorLayak);
    Serial.print(F("Air Jernih (50 ppm, 0.5 NTU): "));
    Serial.print(testSkorLayak, 2);
    Serial.print(F(" ["));
    Serial.print(FuzzyKualitasAir_GetStatusBadge(statusLayak));
    Serial.println(F("]"));

    // Test kasus ekstrem: air buruk → harus NOT_SUITABLE (0.00).
    float testSkorBuruk = FuzzyKualitasAir_HitungSkor(1100.0f, 28.0f);
    KualitasAir_t statusBuruk = FuzzyKualitasAir_GetStatus(testSkorBuruk);
    Serial.print(F("Air Buruk (1100 ppm, 28 NTU): "));
    Serial.print(testSkorBuruk, 2);
    Serial.print(F(" ["));
    Serial.print(FuzzyKualitasAir_GetStatusBadge(statusBuruk));
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
