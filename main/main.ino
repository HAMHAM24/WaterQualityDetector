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
    const FuzzyProfil_t* pAirMinum = globals_getProfile(WaterParameter::AIR_MINUM);
    const FuzzyProfil_t* pHigiene = globals_getProfile(WaterParameter::HIGIENE_SANITASI);

    Serial.println(F("========================================"));
    Serial.println(F("    VALIDASI AUTOMATIS FUZZY SUGENO     "));
    Serial.println(F("========================================"));

    // Test 1: Air Minum (3 input) - Semua parameter Ideal
    float s1 = FuzzyKualitasAir_HitungSkor_AirMinum(pAirMinum, 50.0f, 0.5f, 0.0f);
    Serial.print(F("Air Minum (Ideal)     : "));
    Serial.print(s1, 2);
    Serial.print(F(" [")); Serial.print(FuzzyKualitasAir_GetStatusBadge(FuzzyKualitasAir_GetStatusProfil(pAirMinum, s1)));
    Serial.println(F("]"));

    // Test 2: Air Minum (3 input) - TDS Batas, Turb Ideal, Temp Ideal -> Harusnya Layak Saring Ringan
    float s2 = FuzzyKualitasAir_HitungSkor_AirMinum(pAirMinum, 280.0f, 0.5f, 0.0f);
    Serial.print(F("Air Minum (1 Batas)   : "));
    Serial.print(s2, 2);
    Serial.print(F(" [")); Serial.print(FuzzyKualitasAir_GetStatusBadge(FuzzyKualitasAir_GetStatusProfil(pAirMinum, s2)));
    Serial.println(F("]"));

    // Test 3: Higiene Sanitasi (2 input) - Turb Keruh -> Harusnya Kritis/Tidak Lolos
    float s3 = FuzzyKualitasAir_HitungSkor_Higiene(pHigiene, 100.0f, 15.0f);
    Serial.print(F("Higiene (Turb Buruk)  : "));
    Serial.print(s3, 2);
    Serial.print(F(" [")); Serial.print(FuzzyKualitasAir_GetStatusBadge(FuzzyKualitasAir_GetStatusProfil(pHigiene, s3)));
    Serial.println(F("]"));

    // Test 4: Pemandian Umum (Threshold Check Langsung - Suhu 15-35 C, Turb < 50 NTU)
    ThresholdResult_t resAman = Threshold_CekPemandian(28.0f, 15.0f);
    Serial.print(F("Pemandian (28C, 15NTU): "));
    Serial.println(resAman.semuaAman ? F("[LAYAK]") : F("[TDK LAYAK]"));

    ThresholdResult_t resGagal = Threshold_CekPemandian(38.0f, 15.0f);
    Serial.print(F("Pemandian (38C, 15NTU): "));
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
