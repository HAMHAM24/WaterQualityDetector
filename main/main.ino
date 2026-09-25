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
    Serial.print(F("] "));
    Serial.println((s1 >= 0.999f && s1 <= 1.001f) ? F("PASS") : F("FAIL"));

    // Test 2: TDS pada area proses sedang, suhu dan turbidity baik.
    float s2 = FuzzyKualitasAir_HitungSkor_AirMinum(pAirMinum, 250.0f, 0.5f, 0.5f);
    Serial.print(F("Air Minum (1 Batas)   : "));
    Serial.print(s2, 2);
    Serial.print(F(" [")); Serial.print(FuzzyKualitasAir_GetStatusBadge(FuzzyKualitasAir_GetStatusProfil(pAirMinum, s2)));
    Serial.print(F("] "));
    const bool s2Pass = s2 >= 0.555f && s2 <= 0.558f &&
                        FuzzyKualitasAir_GetStatusProfil(pAirMinum, s2) == STATUS_PROSES_SEDANG;
    Serial.println(s2Pass ? F("PASS") : F("FAIL"));

    // Uji seluruh 64 konsekuen pada titik puncak setiap membership FIS.
    const float tdsCenters[] = {100.0f, 225.0f, 300.0f, 500.0f};
    const float turbCenters[] = {1.0f, 2.25f, 3.0f, 5.0f};
    const float tempCenters[] = {0.5f, 1.75f, 2.75f, 5.0f};
    const float singleton[] = {1.00f, 0.67f, 0.33f, 0.00f};
    bool rulesPass = true;
    for (uint8_t t = 0; t < 4; ++t) {
        for (uint8_t b = 0; b < 4; ++b) {
            for (uint8_t s = 0; s < 4; ++s) {
                uint8_t worst = t > b ? t : b;
                if (s > worst) worst = s;
                const float actual = FuzzyKualitasAir_HitungSkor_AirMinum(
                    pAirMinum, tdsCenters[t], turbCenters[b], tempCenters[s]);
                if (fabs(actual - singleton[worst]) > 0.001f) rulesPass = false;
            }
        }
    }
    Serial.print(F("FIS 64 rule           : "));
    Serial.println(rulesPass ? F("PASS") : F("FAIL"));

    // Test 3: Pemandian / Kolam (Threshold Check: Suhu 16-35 C, Turb < 0.5 NTU) - Kasus Lolos
    ThresholdResult_t resAman = Threshold_CekPemandianKolam(28.0f, 0.3f);
    Serial.print(F("Pemandian (28C, 0.3NTU): "));
    Serial.print(resAman.semuaAman ? F("[LAYAK] ") : F("[TDK LAYAK] "));
    Serial.println(resAman.semuaAman ? F("PASS") : F("FAIL"));

    // Test 4: Pemandian / Kolam (Threshold Check) - Kasus Gagal (Turbidity keruh)
    ThresholdResult_t resGagal = Threshold_CekPemandianKolam(28.0f, 2.5f);
    Serial.print(F("Pemandian (28C, 2.5NTU): "));
    Serial.print(resGagal.semuaAman ? F("[LAYAK] ") : F("[TDK LAYAK] "));
    Serial.println(!resGagal.semuaAman ? F("PASS") : F("FAIL"));

    // PASS menguji implementasi rumus, bukan akurasi terhadap Lab Bante.
    Serial.println(F("Turbidity: interpolasi asumsi + slope regresi estimasi"));
    Serial.print(F("Slope NTU/ADC: ")); Serial.println(TURBIDITY_SLOPE, 6);
    Serial.print(F("Intercept    : ")); Serial.println(TURBIDITY_INTERCEPT, 6);
    const float adcTests[] = {1254.0f, 1671.8f, 1691.0f, 1692.0f, 1693.0f,
                              1700.0f, 1708.0f, 1709.0f, 1723.60f, 2006.16f, 2552.44f};
    const float expectedNtu[] = {0.0f, 0.0f, 0.0f, 0.0f, 0.08f,
                                  0.61f, 1.22f, 1.30f, 8.81f, 154.06f, 434.90f};
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
