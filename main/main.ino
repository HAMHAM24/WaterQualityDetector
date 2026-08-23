/**
 * @file    main.ino
 * @brief   Firmware Water Quality Analyzer — entry point Arduino.
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
        Serial.println(F("FATAL: globals_init() gagal. Free heap mungkin habis."));
        while (true) {}
    }

    buttons_init();
    sensors_init();
    display_init();
    gui_init();

    // --- TEST VALIDASI BASELINE ---
    const FuzzyProfil_t* pAirMinum = globals_getProfile(WaterParameter::AIR_MINUM_HIGIENE);

    Serial.println(F("========================================"));
    Serial.println(F("    VALIDASI AUTOMATIS FIRMWARE         "));
    Serial.println(F("========================================"));

    // Test 1: Air Minum & Higiene (3 input Fuzzy) - Suhu normal 28 C
    float s1 = FuzzyKualitasAir_HitungSkor_AirMinum(pAirMinum, 50.0f, 0.5f, 28.0f);
    Serial.print(F("Air Minum (Ideal)     : "));
    Serial.print(s1, 2);
    Serial.print(F(" [")); Serial.print(FuzzyKualitasAir_GetStatusBadge(FuzzyKualitasAir_GetStatusProfil(pAirMinum, s1)));
    Serial.println(F("]"));

    // Test 2: Air Minum & Higiene - TDS batas, suhu normal 28 C
    float s2 = FuzzyKualitasAir_HitungSkor_AirMinum(pAirMinum, 280.0f, 0.5f, 28.0f);
    Serial.print(F("Air Minum (1 Batas)   : "));
    Serial.print(s2, 2);
    Serial.print(F(" [")); Serial.print(FuzzyKualitasAir_GetStatusBadge(FuzzyKualitasAir_GetStatusProfil(pAirMinum, s2)));
    Serial.println(F("]"));

    // Test 3: Pemandian / Kolam (Threshold Check: Suhu 16-35 C, Turb < 0.5 NTU) - Kasus Lolos
    ThresholdResult_t resAman = Threshold_CekPemandianKolam(28.0f, 0.3f);
    Serial.print(F("Pemandian (28C, 0.3NTU): "));
    Serial.println(resAman.semuaAman ? F("[LAYAK]") : F("[TDK LAYAK]"));

    // Test 4: Pemandian / Kolam (Threshold Check) - Kasus Gagal (Turbidity keruh)
    ThresholdResult_t resGagal = Threshold_CekPemandianKolam(28.0f, 2.5f);
    Serial.print(F("Pemandian (28C, 2.5NTU): "));
    Serial.println(resGagal.semuaAman ? F("[LAYAK]") : F("[TDK LAYAK]"));

    Serial.println(F("========================================"));

    if (!tasks_createAll()) {
        Serial.println(F("FATAL: tasks_createAll() gagal. Heap tidak cukup."));
        while (true) {}
    }

    vTaskStartScheduler();

    Serial.println(F("FATAL: vTaskStartScheduler() gagal dijalankan."));
    while (true) {}
}

void loop() {
    // Intentionally empty
}
