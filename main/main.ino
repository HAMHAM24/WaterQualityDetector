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
    // Serial disiapkan lebih dahulu agar pesan kegagalan inisialisasi terlihat.
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

    // Uji cepat ini memvalidasi perhitungan fuzzy dan threshold sebelum scheduler.
    // --- TEST VALIDASI BASELINE ---
    const FuzzyProfil_t* pAirMinum = globals_getProfile(WaterParameter::AIR_MINUM_HIGIENE);

    Serial.println(F("========================================"));
    Serial.println(F("    VALIDASI AUTOMATIS FIRMWARE         "));
    Serial.println(F("========================================"));

    // Test 1: 3 input SL (TDS, Turbidity, Delta T).
    float s1 = FuzzyKualitasAir_HitungSkor_AirMinum(pAirMinum, 50.0f, 0.5f, 0.5f);
    Serial.print(F("Air Minum (Ideal)     : "));
    Serial.print(s1, 2);
    Serial.print(F(" [")); Serial.print(FuzzyKualitasAir_GetStatusBadge(FuzzyKualitasAir_GetStatusProfil(pAirMinum, s1)));
    Serial.println(F("]"));

    // Test 2: TDS pada area proses sedang, suhu dan turbidity baik.
    float s2 = FuzzyKualitasAir_HitungSkor_AirMinum(pAirMinum, 250.0f, 0.5f, 0.5f);
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

    // PASS menguji implementasi rumus, bukan akurasi terhadap Lab Bante.
    Serial.println(F("Kalibrasi turbidity: 17 titik gabungan"));
    Serial.print(F("Slope NTU/ADC: ")); Serial.println(TURBIDITY_SLOPE, 6);
    Serial.print(F("Intercept    : ")); Serial.println(TURBIDITY_INTERCEPT, 6);
    const float adcTests[] = {710.0f, 735.0f, 736.0f, 754.0f, 795.5f, 852.0f, 1084.0f, 1200.0f};
    const float expectedNtu[] = {0.0f, 0.0f, 0.12f, 21.91f, 72.14f, 140.54f, 421.37f, 561.78f};
    for (unsigned int i = 0; i < sizeof(adcTests) / sizeof(adcTests[0]); ++i) {
        const float ntuTest = sensors_turbidityAdcToNtu(adcTests[i]);
        const float error = ntuTest - expectedNtu[i];
        Serial.print(F("ADC ")); Serial.print(adcTests[i], 1);
        Serial.print(F(" -> ")); Serial.print(ntuTest, 2);
        Serial.print(F(" NTU : "));
        Serial.println((error >= -0.01f && error <= 0.01f) ? F("PASS") : F("FAIL"));
    }

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
