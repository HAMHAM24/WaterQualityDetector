/**
 * @file    gui.cpp
 * @brief   Implementasi GUI Manager berbasis Finite State Machine (FSM).
 *          Alur Workflow: Splash (FBN) -> Menu Utama (Pilih Mode: 4 item) ->
 *          Screen Tunggu / Stabilisasi Sensor (5s) -> Dashboard Hasil (1/2) <->
 *          Detail Rekomendasi (2/2) -> BACK ke Menu Utama.
 *
 *          Mode 1: Air Minum & Higiene -> Evaluasi Fuzzy Sugeno (3 Input)
 *          Mode 2: Pemandian / Kolam   -> Threshold Check Langsung (Non-Fuzzy)
 *          Mode 3: Kalibrasi Sensor    -> TDS, Turbidity, Suhu, Reset
 *          Mode 4: Pengaturan OLED     -> Brightness, Kontras, Info
 *
 *          Pola snapshot thread-safe: gui_draw() mengambil salinan data di
 *          bawah g_dataMutex sebelum merender layar OLED 1.3" (SH1106 / SSD1306).
 */

#include "gui.h"
#include "display.h"
#include "config.h"
#include "storage.h"
#include <STM32FreeRTOS.h>
#include <stdio.h>
#include <math.h>

// =============================================================================
// KONSTANTA DAFTAR MENU (4 ITEM UTAMA)
// =============================================================================
static const char* const HOME_ITEMS[] = {
    "Air Minum & Higiene",
    "Pemandian / Kolam",
    "Kalibrasi Sensor",
    "Pengaturan"
};
static constexpr uint8_t HOME_ITEM_COUNT = 4;

static const char* const CALIBRATION_ITEMS[] = {
    "TDS",
    "Turbidity",
    "Suhu",
    "Reset Pabrik"
};
static constexpr uint8_t CALIBRATION_ITEM_COUNT = 4;

static const char* const SENSOR_SUB_ITEMS[] = {
    "Kalibrasi",
    "Live Monitor"
};
static constexpr uint8_t SENSOR_SUB_ITEM_COUNT = 2;

static const char* const SETTINGS_ITEMS[] = {
    "Brightness",
    "Kontras",
    "Reset Pengaturan",
    "Informasi Firmware"
};
static constexpr uint8_t SETTINGS_ITEM_COUNT = 4;

static constexpr uint8_t SETTINGS_IDX_BRIGHTNESS = 0;
static constexpr uint8_t SETTINGS_IDX_CONTRAST   = 1;
static constexpr uint8_t SETTINGS_IDX_RESET      = 2;
static constexpr uint8_t SETTINGS_IDX_INFO       = 3;

// Pewaktu berbasis milidetik sejak boot
static uint32_t s_bootAnimStartTick = 0;
static uint32_t s_splashStartTick   = 0;
static uint32_t s_samplingStartTick = 0;
static uint32_t s_stabilitySampleTick = 0;
static constexpr uint32_t TURBIDITY_CALIB_SUCCESS_DISPLAY_MS = 2000;
static float s_stabilityMin = 0.0f;
static float s_stabilityMax = 0.0f;

// =============================================================================
// SNAPSHOT — salinan lokal data global yang dibaca semua fungsi draw*()
// =============================================================================
static SensorData         s_view;
static SystemState        s_viewState;
static CalibrationParams  s_viewCalib;

// =============================================================================
// HELPERS BER-MUTEX (dipanggil saat mutex sudah dipegang)
// =============================================================================
static void transitionToLocked(MenuState newState) {
    // Simpan halaman sebelumnya agar tombol BACK dan alur navigasi konsisten.
    g_systemState.previousMenu = g_systemState.currentMenu;
    g_systemState.currentMenu = newState;
    g_systemState.cursorIndex = 0;
    g_systemState.measurementSubPage = 0;
    g_systemState.aboutSubPage = 0;
    g_systemState.settingsAdjustMode = false;
    g_systemState.displayDirty = true;
}

static void moveCursorLocked(bool moveDown, uint8_t itemCount) {
    // Cursor berputar dari item terakhir kembali ke item pertama, dan sebaliknya.
    if (moveDown) {
        g_systemState.cursorIndex = static_cast<uint8_t>(
            (g_systemState.cursorIndex + 1) % itemCount);
    } else {
        g_systemState.cursorIndex = static_cast<uint8_t>(
            (g_systemState.cursorIndex == 0) ? (itemCount - 1)
                                             : (g_systemState.cursorIndex - 1));
    }
    g_systemState.displayDirty = true;
}

// =============================================================================
// FUNGSI DRAW (membaca dari snapshot s_view / s_viewState / s_viewCalib)
// =============================================================================

static uint8_t centeredX(const char* text) {
    // Hitung posisi horizontal agar teks berada di tengah layar OLED.
    uint8_t textWidth = g_u8g2.getStrWidth(text);
    if (textWidth >= DISPLAY_WIDTH) return 0;
    return static_cast<uint8_t>((DISPLAY_WIDTH - textWidth) / 2);
}

/** @brief Viewport daftar menu: 4 baris muat penuh tanpa desak-desakan. */
static void drawSimpleList(const char* title, const char* const* items,
                            uint8_t count, uint8_t cursor) {
    // Render menu dan geser viewport bila posisi cursor melewati layar.
    display_drawHeader(title);
    g_u8g2.setFont(u8g2_font_6x10_tf);

    uint8_t firstVisible = 0;
    if (count > MENU_VISIBLE_ROWS) {
        if (cursor >= MENU_VISIBLE_ROWS) {
            firstVisible = cursor - (MENU_VISIBLE_ROWS - 1);
        }
        if (firstVisible + MENU_VISIBLE_ROWS > count) {
            firstVisible = count - MENU_VISIBLE_ROWS;
        }
    }

    for (uint8_t i = firstVisible; i < count; i++) {
        uint8_t row = i - firstVisible;
        if (row >= MENU_VISIBLE_ROWS) break;

        uint8_t y = MENU_FIRST_LINE_Y + static_cast<uint8_t>(row * MENU_LINE_HEIGHT);
        if (y > MENU_LAST_LINE_Y) break;

        if (i == cursor) {
            g_u8g2.drawStr(2, y, ">");
        }
        g_u8g2.drawStr(11, y, items[i]);
    }

    // Indikator scroll (bila ada item di luar viewport)
    if (firstVisible > 0) {
        g_u8g2.drawStr(121, MENU_FIRST_LINE_Y, "^");
    }
    if (count > firstVisible + MENU_VISIBLE_ROWS) {
        g_u8g2.drawStr(121, MENU_LAST_LINE_Y, "v");
    }
}

/**
 * @brief Animasi pembuka (Boot Animation) bertema air berdurasi 5 detik.
 *        Alur Visual:
 *          1. 0.0s - 1.3s: Droplet forming at top nozzle & free-falling with gravity.
 *          2. 1.3s - 2.6s: Impact splash particles + concentric expanding ripples + rebound drop.
 *          3. 2.6s - 4.7s: Dual flowing sine waves rising up to fill tank + floating rising bubbles.
 *          4. 4.7s - 5.0s: Calming wave settle transition into Splash Screen.
 */
static void drawBootAnimation() {
    uint32_t elapsed = millis() - s_bootAnimStartTick;
    if (elapsed > BOOT_ANIMATION_MS) {
        elapsed = BOOT_ANIMATION_MS;
    }

    if (elapsed < 1300) {
        // -------------------------------------------------------------
        // FASE 1: TETESAN AIR JATUH (0 - 1300 ms)
        // -------------------------------------------------------------
        // Nozzle / titik tetesan di atas tengah
        g_u8g2.drawHLine(61, 0, 7);
        g_u8g2.drawHLine(62, 1, 5);
        g_u8g2.drawHLine(63, 2, 3);

        float dropY = 4.0f;
        uint8_t dropRadius = 2;

        if (elapsed < 300) {
            // Tetesan membesar di nozzle
            float formP = static_cast<float>(elapsed) / 300.0f;
            dropRadius = (formP < 0.5f) ? 1 : 2;
            dropY = 4.0f + formP * 2.0f;
            g_u8g2.drawDisc(64, static_cast<uint8_t>(dropY), dropRadius);
        } else {
            // Tetesan jatuh bebas (akselerasi gravitasi kuadratik)
            float t = static_cast<float>(elapsed - 300) / 1000.0f;
            if (t > 1.0f) t = 1.0f;
            dropY = 6.0f + 42.0f * (t * t); // dari Y=6 ke Y=48
            uint8_t iy = static_cast<uint8_t>(dropY);

            // Bentuk teardrop: disc bulat + segitiga runcing ke atas
            g_u8g2.drawDisc(64, iy, 3);
            if (iy >= 7) {
                g_u8g2.drawTriangle(62, iy - 1, 66, iy - 1, 64, iy - 6);
            }
            // Pantulan kilau cahaya (shine highlight) di dalam tetesan
            g_u8g2.setDrawColor(0);
            g_u8g2.drawPixel(63, iy - 1);
            g_u8g2.setDrawColor(1);
        }

        // Permukaan air di dasar (garis tenang dengan batas halus)
        g_u8g2.drawHLine(20, 48, 88);

        // Teks branding minimalis di bawah
        g_u8g2.setFont(u8g2_font_5x7_tf);
        const char* const prompt = "WATER DETECTOR";
        g_u8g2.drawStr(centeredX(prompt), 60, prompt);

    } else if (elapsed < 2600) {
        // -------------------------------------------------------------
        // FASE 2: IMPAK SPLASH & GELOMBANG RIAK KONSENTRIS (1300 - 2600 ms)
        // -------------------------------------------------------------
        uint32_t tPhase = elapsed - 1300;
        constexpr uint8_t impactX = 64;
        constexpr uint8_t impactY = 48;

        // 1. Partikel percikan air memantul (0 - 650 ms)
        if (tPhase < 650) {
            float pSplash = static_cast<float>(tPhase) / 650.0f;
            float arcH = sinf(pSplash * 3.14159f);

            int16_t px1 = impactX - static_cast<int16_t>(pSplash * 22.0f);
            int16_t py1 = impactY - static_cast<int16_t>(arcH * 16.0f);
            if (px1 >= 0 && py1 >= 0 && py1 < DISPLAY_HEIGHT) {
                g_u8g2.drawDisc(static_cast<uint8_t>(px1), static_cast<uint8_t>(py1), 1);
            }

            int16_t px2 = impactX - static_cast<int16_t>(pSplash * 11.0f);
            int16_t py2 = impactY - static_cast<int16_t>(arcH * 22.0f);
            if (px2 >= 0 && py2 >= 0 && py2 < DISPLAY_HEIGHT) {
                g_u8g2.drawDisc(static_cast<uint8_t>(px2), static_cast<uint8_t>(py2), 1);
            }

            int16_t px3 = impactX + static_cast<int16_t>(pSplash * 11.0f);
            int16_t py3 = impactY - static_cast<int16_t>(arcH * 22.0f);
            if (px3 < DISPLAY_WIDTH && py3 >= 0 && py3 < DISPLAY_HEIGHT) {
                g_u8g2.drawDisc(static_cast<uint8_t>(px3), static_cast<uint8_t>(py3), 1);
            }

            int16_t px4 = impactX + static_cast<int16_t>(pSplash * 22.0f);
            int16_t py4 = impactY - static_cast<int16_t>(arcH * 16.0f);
            if (px4 < DISPLAY_WIDTH && py4 >= 0 && py4 < DISPLAY_HEIGHT) {
                g_u8g2.drawDisc(static_cast<uint8_t>(px4), static_cast<uint8_t>(py4), 1);
            }
        }

        // 2. Tetesan pantulan tengah (rebound crown spike) (150 - 750 ms)
        if (tPhase >= 150 && tPhase < 750) {
            float pReb = static_cast<float>(tPhase - 150) / 600.0f;
            float rebH = sinf(pReb * 3.14159f);
            int16_t rebY = impactY - static_cast<int16_t>(rebH * 18.0f);
            if (rebY >= 0 && rebY < DISPLAY_HEIGHT) {
                g_u8g2.drawDisc(impactX, static_cast<uint8_t>(rebY), 2);
                g_u8g2.drawLine(impactX, static_cast<uint8_t>(rebY), impactX, impactY);
            }
        }

        // 3. Gelombang elips konsentris melebar (Ripples)
        // Riak 1
        float rx1 = (static_cast<float>(tPhase) / 1300.0f) * 60.0f;
        float ry1 = rx1 * 0.28f;
        if (rx1 >= 2.0f && rx1 < 64.0f && ry1 >= 1.0f) {
            g_u8g2.drawEllipse(impactX, impactY, static_cast<uint8_t>(rx1), static_cast<uint8_t>(ry1), U8G2_DRAW_ALL);
        }

        // Riak 2 (delay 250ms)
        if (tPhase > 250) {
            float rx2 = (static_cast<float>(tPhase - 250) / 1050.0f) * 46.0f;
            float ry2 = rx2 * 0.28f;
            if (rx2 >= 2.0f && rx2 < 64.0f && ry2 >= 1.0f) {
                g_u8g2.drawEllipse(impactX, impactY, static_cast<uint8_t>(rx2), static_cast<uint8_t>(ry2), U8G2_DRAW_ALL);
            }
        }

        // Riak 3 (delay 500ms)
        if (tPhase > 500) {
            float rx3 = (static_cast<float>(tPhase - 500) / 800.0f) * 32.0f;
            float ry3 = rx3 * 0.28f;
            if (rx3 >= 2.0f && rx3 < 64.0f && ry3 >= 1.0f) {
                g_u8g2.drawEllipse(impactX, impactY, static_cast<uint8_t>(rx3), static_cast<uint8_t>(ry3), U8G2_DRAW_ALL);
            }
        }

        // Teks branding
        g_u8g2.setFont(u8g2_font_5x7_tf);
        const char* const prompt = "WATER DETECTOR";
        g_u8g2.drawStr(centeredX(prompt), 60, prompt);

    } else {
        // -------------------------------------------------------------
        // FASE 3 & 4: GELOMBANG AIR NAIK & GELEMBUNG (2600 - 5000 ms)
        // -------------------------------------------------------------
        uint32_t tPhase = elapsed - 2600;
        float pWave = static_cast<float>(tPhase) / 2400.0f;
        if (pWave > 1.0f) pWave = 1.0f;

        // Ketinggian dasar air naik dari Y=52 hingga Y=26 secara halus (ease in-out)
        float ease = 0.5f * (1.0f - cosf(pWave * 3.14159f));
        float baseWaterY = 52.0f - (26.0f * ease);

        // Render Dual Sine Wave (lapisan gelombang air)
        float waveAmp = 3.0f * (1.0f - pWave * 0.25f);
        float waveSpeed1 = static_cast<float>(elapsed) * 0.008f;
        float waveSpeed2 = static_cast<float>(elapsed) * 0.005f;

        for (uint8_t x = 0; x < DISPLAY_WIDTH; x++) {
            // Gelombang utama (Foreground wave)
            float w1 = sinf(static_cast<float>(x) * 0.09f + waveSpeed1) * waveAmp;
            int16_t y1 = static_cast<int16_t>(baseWaterY + w1);
            if (y1 < 0) y1 = 0;
            if (y1 < DISPLAY_HEIGHT) {
                g_u8g2.drawVLine(x, y1, DISPLAY_HEIGHT - y1);
            }

            // Puncak gelombang kedua (Background crest highlight)
            if ((x & 1) == 0) {
                float w2 = sinf(static_cast<float>(x) * 0.07f - waveSpeed2 + 1.6f) * (waveAmp * 0.8f);
                int16_t y2 = static_cast<int16_t>(baseWaterY - 3.0f + w2);
                if (y2 >= 0 && y2 < y1) {
                    g_u8g2.drawPixel(x, static_cast<uint8_t>(y2));
                }
            }
        }

        // Render Gelembung Udara yang mengapung ke atas (Floating Bubbles)
        struct BubbleDef {
            uint8_t  baseX;
            uint16_t period;
            uint16_t phase;
            uint8_t  radius;
        };
        static constexpr BubbleDef BUBBLES[5] = {
            { 20, 1300, 0,   2 },
            { 46, 1100, 280, 1 },
            { 70, 1400, 520, 3 },
            { 94, 1200, 150, 2 },
            { 112, 1000, 700, 1 }
        };

        for (uint8_t i = 0; i < 5; i++) {
            uint32_t bTime = (elapsed + BUBBLES[i].phase) % BUBBLES[i].period;
            float bProgress = static_cast<float>(bTime) / static_cast<float>(BUBBLES[i].period);
            
            float wobble = sinf(static_cast<float>(elapsed) * 0.007f + static_cast<float>(i)) * 2.5f;
            int16_t bx = static_cast<int16_t>(BUBBLES[i].baseX + wobble);
            int16_t by = static_cast<int16_t>(63.0f - bProgress * (63.0f - baseWaterY));

            if (by > static_cast<int16_t>(baseWaterY) + 3 && by < 62 && bx >= 3 && bx < DISPLAY_WIDTH - 3) {
                g_u8g2.setDrawColor(0);
                g_u8g2.drawDisc(static_cast<uint8_t>(bx), static_cast<uint8_t>(by), BUBBLES[i].radius);
                
                g_u8g2.setDrawColor(1);
                g_u8g2.drawCircle(static_cast<uint8_t>(bx), static_cast<uint8_t>(by), BUBBLES[i].radius);
                if (BUBBLES[i].radius >= 2) {
                    g_u8g2.drawPixel(static_cast<uint8_t>(bx - 1), static_cast<uint8_t>(by - 1));
                }
            }
        }

        // Teks Judul & Status di atas permukaan air
        g_u8g2.setDrawColor(1);
        g_u8g2.setFont(u8g2_font_7x14B_tf);
        const char* const appTitle = "WATER QUALITY";
        g_u8g2.drawStr(centeredX(appTitle), 12, appTitle);

        g_u8g2.setFont(u8g2_font_5x7_tf);
        const char* const statusStr = "INITIALIZING SENSORS...";
        g_u8g2.drawStr(centeredX(statusStr), 21, statusStr);
    }
}

/** @brief Splash Screen (Sesuai Note/INTRO: Physic Water Quality Index FBN) */
static void drawSplash() {
    static const char* const titleLine1 = "Physic Water";
    static const char* const titleLine2 = "Quality Index";
    static const char* const titleLine3 = "FBN";

    g_u8g2.setFont(u8g2_font_7x14B_tf);
    g_u8g2.drawStr(centeredX(titleLine1), 18, titleLine1);
    g_u8g2.drawStr(centeredX(titleLine2), 33, titleLine2);
    g_u8g2.drawStr(centeredX(titleLine3), 47, titleLine3);

    g_u8g2.setFont(u8g2_font_5x7_tf);
    char versionLine[24];
    snprintf(versionLine, sizeof(versionLine), "v%s", FIRMWARE_VERSION);
    g_u8g2.drawStr(centeredX(versionLine), 60, versionLine);
}

/** @brief Menu Utama (Pemilihan Objek Air & Fitur) */
static void drawHome() {
    drawSimpleList("Pilih Mode Uji Air", HOME_ITEMS, HOME_ITEM_COUNT, s_viewState.cursorIndex);
    display_drawStatusBar("UP/DN:Pilih", "OK:Masuk");
}

/** @brief Screen Tunggu / Stabilisasi Pembacaan Sensor (5 Detik) */
static void drawWaitingSampling() {
    // Halaman ini memberi waktu sensor menstabilkan pembacaan sebelum hasil tampil.
    const char* modeTitle = (s_viewState.activeParameter == WaterParameter::AIR_MINUM_HIGIENE)
                                ? "MODE: AIR MINUM"
                                : "MODE: PEMANDIAN";
    display_drawHeader(modeTitle);

    g_u8g2.setFont(u8g2_font_6x10_tf);
    if (s_viewState.stabilizationTimedOut) {
        g_u8g2.drawStr(2, MENU_FIRST_LINE_Y, "Suhu belum stabil");
        g_u8g2.drawStr(2, MENU_FIRST_LINE_Y + MENU_LINE_HEIGHT, "OK: lanjut manual");
        display_drawStatusBar("OK:Lanjut", "BACK:Batal");
        return;
    }
    g_u8g2.drawStr(12, 26, "Membaca Sensor...");

    // Progress bar dinamis (durasi SAMPLING_SCREEN_MS = 5000 ms)
    uint32_t elapsed = millis() - s_samplingStartTick;
    if (elapsed > SAMPLING_SCREEN_MS) elapsed = SAMPLING_SCREEN_MS;
    float progress = static_cast<float>(elapsed) / static_cast<float>(SAMPLING_SCREEN_MS);

    // Bingkai progress bar
    constexpr uint8_t barX = 12;
    constexpr uint8_t barY = 32;
    constexpr uint8_t barW = 104;
    constexpr uint8_t barH = 10;
    g_u8g2.drawFrame(barX, barY, barW, barH);

    // Isi progress bar
    uint8_t fillW = static_cast<uint8_t>(progress * (barW - 4));
    if (fillW > 0) {
        g_u8g2.drawBox(barX + 2, barY + 2, fillW, barH - 4);
    }

    // Status kestabilan di kiri dan persentase di kanan (font 5x7, y = 51)
    g_u8g2.setFont(u8g2_font_5x7_tf);

    char stability[16];
    snprintf(stability, sizeof(stability), "Stabil: %u/%u", s_viewState.stabilizationCount,
             TEMP_STABLE_REQUIRED_SAMPLES);
    g_u8g2.drawStr(barX, 51, stability);

    char pctBuf[8];
    snprintf(pctBuf, sizeof(pctBuf), "%u%%", static_cast<unsigned>(progress * 100.0f));
    uint8_t pctW = g_u8g2.getStrWidth(pctBuf);
    uint8_t pctX = (barX + barW) - pctW;
    g_u8g2.drawStr(pctX, 51, pctBuf);

    display_drawStatusBar("Tunggu stabil.", "BACK:Batal");
}

static void drawAmbientTemperatureInput() {
    display_drawHeader("Input Suhu Udara");
    g_u8g2.setFont(u8g2_font_6x10_tf);
    char value[8];
    char line[32];
    dtostrf(s_viewState.ambientTemperature, 4, 1, value);
    char* p = value; while (*p == ' ') ++p;
    snprintf(line, sizeof(line), "Udara: [ %s C ]", p);
    g_u8g2.drawStr(2, MENU_FIRST_LINE_Y, line);
    g_u8g2.drawStr(2, MENU_FIRST_LINE_Y + MENU_LINE_HEIGHT, "UP/DN: +/-0.1 C");
    g_u8g2.drawStr(2, MENU_FIRST_LINE_Y + 2 * MENU_LINE_HEIGHT, "LF/RT: +/-1.0 C");
    display_drawStatusBar("OK:Mulai", "BACK:Batal");
}

/** @brief Mencetak satu baris nilai sensor dengan label dan satuan. */
static void drawSensorLine(uint8_t y, const char* label, float value,
                            const char* unit, SensorStatus status) {
    g_u8g2.setFont(u8g2_font_6x10_tf);
    char line[32];
    if (status == SensorStatus::OK) {
        if (value > 999.0f) {
            snprintf(line, sizeof(line), "%s >999 %s", label, unit);
        } else {
            char valueStr[12];
            dtostrf(value, 4, 1, valueStr);
            char* p = valueStr;
            while (*p == ' ') p++;
            snprintf(line, sizeof(line), "%s %s %s", label, p, unit);
        }
    } else if (status == SensorStatus::NOT_USED) {
        snprintf(line, sizeof(line), "%s -", label);
    } else {
        snprintf(line, sizeof(line), "%s ERROR", label);
    }
    g_u8g2.drawStr(2, y, line);
}

static const char* severityLabel(uint8_t severity, const char* ideal,
                                 const char* batas, const char* buruk) {
    if (severity == 0) return ideal;
    if (severity == 1) return batas;
    return buruk;
}

/** @brief Dashboard, diagnosis, dan saran spesifik (Three-Page View). */
static void drawMeasurement() {
    // Tampilkan ringkasan kualitas air berdasarkan snapshot data sensor.
    if (s_viewState.activeParameter == WaterParameter::PEMANDIAN_KOLAM) {
        // =====================================================================
        // MODE 2: PEMANDIAN / KOLAM — EVALUASI THRESHOLD CHECKER (NON-FUZZY)
        // =====================================================================
        if (s_viewState.measurementSubPage == 0) {
            // --- HALAMAN 1: DASHBOARD PER-PARAMETER ---
            display_drawHeader("Pemandian (1/3)");
            uint8_t y = MENU_FIRST_LINE_Y;
            g_u8g2.setFont(u8g2_font_6x10_tf);

            // 1. Suhu (16-35 C)
            char tempBuf[32];
            if (s_view.temperatureStatus == SensorStatus::OK) {
                char tStr[8];
                dtostrf(s_view.temperature, 4, 1, tStr);
                char* p = tStr; while (*p == ' ') p++;
                const char* sTag = s_view.thresholdResult.suhuAman ? "[LAYAK]" : "[TDK]";
                snprintf(tempBuf, sizeof(tempBuf), "Suhu: %s C %s", p, sTag);
            } else {
                snprintf(tempBuf, sizeof(tempBuf), "Suhu: ERROR");
            }
            g_u8g2.drawStr(2, y, tempBuf);
            y += MENU_LINE_HEIGHT;

            // 2. Turbidity (< 0.5 NTU)
            char turbBuf[32];
            if (s_view.turbidityStatus == SensorStatus::OK) {
                const char* tbTag = s_view.thresholdResult.turbidityAman ? "[LAYAK]" : "[TDK]";
                if (s_view.turbidityFiltered > 999.0f) {
                    snprintf(turbBuf, sizeof(turbBuf), "Turb: >999 NTU %s", tbTag);
                } else {
                    char tbStr[8];
                    dtostrf(s_view.turbidityFiltered, 4, 1, tbStr);
                    char* p = tbStr; while (*p == ' ') p++;
                    snprintf(turbBuf, sizeof(turbBuf), "Turb: %s NTU %s", p, tbTag);
                }
            } else {
                snprintf(turbBuf, sizeof(turbBuf), "Turb: ERROR");
            }
            g_u8g2.drawStr(2, y, turbBuf);
            y += MENU_LINE_HEIGHT;

            // 3. TDS (tetap ditampilkan nilainya + tag BYPASS)
            char tdsBuf[32];
            if (s_view.tdsStatus == SensorStatus::OK) {
                if (s_view.tdsCompensated > 999.0f) {
                    snprintf(tdsBuf, sizeof(tdsBuf), "TDS : >999 ppm [BYP]");
                } else {
                    char tdsStr[8];
                    dtostrf(s_view.tdsCompensated, 4, 1, tdsStr);
                    char* p = tdsStr; while (*p == ' ') p++;
                    snprintf(tdsBuf, sizeof(tdsBuf), "TDS : %s ppm [BYP]", p);
                }
            } else {
                snprintf(tdsBuf, sizeof(tdsBuf), "TDS : ERROR");
            }
            g_u8g2.drawStr(2, y, tdsBuf);
            y += MENU_LINE_HEIGHT;

            // 4. Status Keseluruhan (singkat)
            char stBuf[32];
            const char* allTag = s_view.thresholdResult.semuaAman ? "LAYAK" : "TIDAK LAYAK";
            snprintf(stBuf, sizeof(stBuf), "Status: %s", allTag);
            g_u8g2.drawStr(2, y, stBuf);

            display_drawStatusBar("DN:Detail", "BACK:Menu");

        } else if (s_viewState.measurementSubPage == 1) {
            // --- HALAMAN 2: DIAGNOSIS PARAMETER ---
            display_drawHeader("Diagnosis (2/3)");
            uint8_t y = MENU_FIRST_LINE_Y;
            g_u8g2.setFont(u8g2_font_6x10_tf);

            g_u8g2.drawStr(2, y, "Batas Pemandian/Kolam");
            y += MENU_LINE_HEIGHT;

            char l1[32];
            const char* sTag = s_view.thresholdResult.suhuAman ? "[LAYAK]" : "[TDK]";
            snprintf(l1, sizeof(l1), "Suhu: 16-35C %s", sTag);
            g_u8g2.drawStr(2, y, l1);
            y += MENU_LINE_HEIGHT;

            char l2[32];
            const char* tbTag = s_view.thresholdResult.turbidityAman ? "[LAYAK]" : "[TDK]";
            snprintf(l2, sizeof(l2), "Turb: <0.5NTU %s", tbTag);
            g_u8g2.drawStr(2, y, l2);
            y += MENU_LINE_HEIGHT;

            g_u8g2.drawStr(2, y, "TDS : BYPASS");

            display_drawStatusBar("DN:Saran", "BACK:Menu");
        } else {
            // --- HALAMAN 3: SARAN TINDAKAN ---
            display_drawHeader("Saran (3/3)");
            uint8_t y = MENU_FIRST_LINE_Y;
            g_u8g2.setFont(u8g2_font_6x10_tf);

            if (s_view.thresholdResult.semuaAman) {
                g_u8g2.drawStr(2, y, "Air layak digunakan");
                y += MENU_LINE_HEIGHT;
                g_u8g2.drawStr(2, y, "Jaga air tetap jernih");
            } else {
                if (!s_view.thresholdResult.suhuAman) {
                    g_u8g2.drawStr(2, y, "Suhu: atur suhu air");
                    y += MENU_LINE_HEIGHT;
                }
                if (!s_view.thresholdResult.turbidityAman) {
                    g_u8g2.drawStr(2, y, "Turb: jernihkan air");
                    y += MENU_LINE_HEIGHT;
                }
                g_u8g2.drawStr(2, y, "Uji ulang air");
            }

            display_drawStatusBar("UP:Diagnosis", "BACK:Menu");
        }
        return;
    }

    // =========================================================================
    // MODE 1: AIR MINUM & HIGIENE — EVALUASI FUZZY SUGENO (3 INPUT)
    // =========================================================================
    if (s_viewState.measurementSubPage == 0) {
        // --- HALAMAN 1: DATA SENSOR + SKOR FUZZY (DASHBOARD) ---
        display_drawHeader("Air Minum (1/3)");
        uint8_t y = MENU_FIRST_LINE_Y;

        // Suhu + status suhu
        char tempBuf[32];
        if (s_view.temperatureStatus == SensorStatus::OK) {
            char tStr[8];
            char dtStr[8];
            dtostrf(s_view.temperature, 4, 1, tStr);
            dtostrf(s_viewState.temperatureDelta, 3, 1, dtStr);
            char* p = tStr; while (*p == ' ') p++;
            char* pDt = dtStr; while (*pDt == ' ') pDt++;
            snprintf(tempBuf, sizeof(tempBuf), "Air:%sC dT:%s", p, pDt);
        } else {
            snprintf(tempBuf, sizeof(tempBuf), "Suhu : ERROR");
        }
        g_u8g2.setFont(u8g2_font_6x10_tf);
        g_u8g2.drawStr(2, y, tempBuf);
        y += MENU_LINE_HEIGHT;

        // TDS
        drawSensorLine(y, "TDS  :", s_view.tdsCompensated, "ppm", s_view.tdsStatus);
        y += MENU_LINE_HEIGHT;

        // Turbidity
        drawSensorLine(y, "Turb :", s_view.turbidityFiltered, "NTU",
                       s_view.turbidityStatus);
        y += MENU_LINE_HEIGHT;

        // Skor + badge (skor 0.00 - 1.00)
        const char* badge = FuzzyKualitasAir_GetStatusBadge(s_view.qualityStatus);
        char scoreStr[8];
        char skorBuf[32];
        dtostrf(s_view.fuzzyScore, 4, 2, scoreStr);
        char* p = scoreStr;
        while (*p == ' ') p++;
        snprintf(skorBuf, sizeof(skorBuf), "Skor : %s [%s]", p, badge);
        g_u8g2.drawStr(2, y, skorBuf);

        display_drawStatusBar("DN:Detail", "BACK:Menu");

    } else if (s_viewState.measurementSubPage == 1) {
        // --- HALAMAN 2: DIAGNOSIS PARAMETER ---
        display_drawHeader("Diagnosis (2/3)");
        uint8_t y = MENU_FIRST_LINE_Y;
        g_u8g2.setFont(u8g2_font_6x10_tf);

        char lineBuf[32];
        snprintf(lineBuf, sizeof(lineBuf), "Mutu: %s", FuzzyKualitasAir_GetStatusBadge(s_view.qualityStatus));
        g_u8g2.drawStr(2, y, lineBuf);
        y += MENU_LINE_HEIGHT;

        snprintf(lineBuf, sizeof(lineBuf), "dT  : %s", FuzzyKualitasAir_GetStatusSuhuStr(s_view.tempStatus));
        g_u8g2.drawStr(2, y, lineBuf);
        y += MENU_LINE_HEIGHT;

        snprintf(lineBuf, sizeof(lineBuf), "TDS : %s", severityLabel(
                 s_view.tdsSeverity, "Ideal", "Batas", "Tinggi"));
        g_u8g2.drawStr(2, y, lineBuf);
        y += MENU_LINE_HEIGHT;

        snprintf(lineBuf, sizeof(lineBuf), "Turb: %s", severityLabel(
                 s_view.turbiditySeverity, "Jernih", "Sedang", "Keruh"));
        g_u8g2.drawStr(2, y, lineBuf);

        display_drawStatusBar("DN:Saran", "BACK:Menu");
    } else {
        // --- HALAMAN 3: SARAN TINDAKAN SPESIFIK ---
        display_drawHeader("Saran (3/3)");
        uint8_t y = MENU_FIRST_LINE_Y;
        g_u8g2.setFont(u8g2_font_6x10_tf);

        if (s_view.temperatureSeverity == 0 && s_view.tdsSeverity == 0 &&
            s_view.turbiditySeverity == 0) {
            g_u8g2.drawStr(2, y, "Air layak digunakan");
            y += MENU_LINE_HEIGHT;
            g_u8g2.drawStr(2, y, "Pantau berkala");
        } else {
            if (s_view.tdsSeverity == 1) {
                g_u8g2.drawStr(2, y, "TDS : saring ringan");
                y += MENU_LINE_HEIGHT;
            } else if (s_view.tdsSeverity >= 2) {
                g_u8g2.drawStr(2, y, "TDS : RO/ganti air");
                y += MENU_LINE_HEIGHT;
            }
            if (s_view.turbiditySeverity == 1) {
                g_u8g2.drawStr(2, y, "Turb: endap+saring");
                y += MENU_LINE_HEIGHT;
            } else if (s_view.turbiditySeverity >= 2) {
                g_u8g2.drawStr(2, y, "Turb: filter total");
                y += MENU_LINE_HEIGHT;
            }
            if (s_view.temperatureSeverity == 1) {
                g_u8g2.drawStr(2, y, "Suhu: sesuaikan suhu");
                y += MENU_LINE_HEIGHT;
            } else if (s_view.temperatureSeverity >= 2) {
                g_u8g2.drawStr(2, y, "Suhu: deviasi tinggi");
                y += MENU_LINE_HEIGHT;
            }
            if (y <= MENU_LAST_LINE_Y) {
                g_u8g2.drawStr(2, y, "Uji ulang air");
            }
        }

        display_drawStatusBar("UP:Diagnosis", "BACK:Menu");
    }
}

static void drawCalibration() {
    // Pilih tampilan kalibrasi sesuai submenu yang sedang aktif.
    drawSimpleList("Kalibrasi Sensor", CALIBRATION_ITEMS, CALIBRATION_ITEM_COUNT,
                   s_viewState.cursorIndex);
    if (s_viewState.calibSaving) {
        display_drawStatusBar("Menyimpan...", nullptr);
    } else {
        display_drawStatusBar("OK:Pilih", "BACK:Menu");
    }
}

/** @brief Sub-menu TDS: pilih antara Kalibrasi atau Live Monitor. */
static void drawCalibrationTdsMenu() {
    drawSimpleList("TDS", SENSOR_SUB_ITEMS, SENSOR_SUB_ITEM_COUNT,
                   s_viewState.cursorIndex);
    display_drawStatusBar("OK:Pilih", "BACK:Menu");
}

/** @brief Live Monitor TDS — 1 layar raw data. */
static void drawTdsMonitor() {
    display_drawHeader("Live TDS");
    g_u8g2.setFont(u8g2_font_6x10_tf);
    uint8_t y = MENU_FIRST_LINE_Y;
    char lineBuf[32];

    snprintf(lineBuf, sizeof(lineBuf), "ADC   : %u", s_view.tdsRaw);
    g_u8g2.drawStr(2, y, lineBuf); y += MENU_LINE_HEIGHT;

    char vStr[8];
    dtostrf(s_view.tdsVoltage, 4, 2, vStr);
    char* p = vStr; while (*p == ' ') p++;
    snprintf(lineBuf, sizeof(lineBuf), "Volt  : %s V", p);
    g_u8g2.drawStr(2, y, lineBuf); y += MENU_LINE_HEIGHT;

    if (s_view.temperatureStatus == SensorStatus::OK) {
        char tStr[8];
        dtostrf(s_view.temperature, 4, 1, tStr);
        char* pT = tStr; while (*pT == ' ') pT++;
        snprintf(lineBuf, sizeof(lineBuf), "Suhu  : %s C", pT);
    } else {
        snprintf(lineBuf, sizeof(lineBuf), "Suhu  : ERROR");
    }
    g_u8g2.drawStr(2, y, lineBuf); y += MENU_LINE_HEIGHT;

    snprintf(lineBuf, sizeof(lineBuf), "Status: %s",
             (s_view.tdsStatus == SensorStatus::OK) ? "OK" : "ERROR");
    g_u8g2.drawStr(2, y, lineBuf);

    display_drawStatusBar("", "BACK:Menu");
}

/** @brief Sub-menu Turbidity: pilih antara Kalibrasi atau Live Monitor. */
static void drawCalibrationTurbidityMenu() {
    drawSimpleList("Turbidity", SENSOR_SUB_ITEMS, SENSOR_SUB_ITEM_COUNT,
                   s_viewState.cursorIndex);
    display_drawStatusBar("OK:Pilih", "BACK:Menu");
}

/** @brief Live Monitor Turbidity — 1 layar raw data. */
static void drawTurbidityMonitor() {
    display_drawHeader("Live Turb");
    g_u8g2.setFont(u8g2_font_6x10_tf);
    uint8_t y = MENU_FIRST_LINE_Y;
    char lineBuf[32];

    snprintf(lineBuf, sizeof(lineBuf), "ADC   : %u", s_view.turbidityRaw);
    g_u8g2.drawStr(2, y, lineBuf); y += MENU_LINE_HEIGHT;

    char vStr[8];
    dtostrf(s_view.turbidityVoltage, 4, 2, vStr);
    char* p = vStr; while (*p == ' ') p++;
    snprintf(lineBuf, sizeof(lineBuf), "Volt  : %s V", p);
    g_u8g2.drawStr(2, y, lineBuf); y += MENU_LINE_HEIGHT;

    if (s_view.temperatureStatus == SensorStatus::OK) {
        char tStr[8];
        dtostrf(s_view.temperature, 4, 1, tStr);
        char* pT = tStr; while (*pT == ' ') pT++;
        snprintf(lineBuf, sizeof(lineBuf), "Suhu  : %s C", pT);
    } else {
        snprintf(lineBuf, sizeof(lineBuf), "Suhu  : ERROR");
    }
    g_u8g2.drawStr(2, y, lineBuf); y += MENU_LINE_HEIGHT;

    snprintf(lineBuf, sizeof(lineBuf), "Status: %s",
             (s_view.turbidityStatus == SensorStatus::OK) ? "OK" : "ERROR");
    g_u8g2.drawStr(2, y, lineBuf);

    display_drawStatusBar("", "BACK:Menu");
}

/** @brief Sub-menu Suhu: pilih antara Kalibrasi atau Live Monitor. */
static void drawCalibrationTemperatureMenu() {
    drawSimpleList("Suhu", SENSOR_SUB_ITEMS, SENSOR_SUB_ITEM_COUNT,
                   s_viewState.cursorIndex);
    display_drawStatusBar("OK:Pilih", "BACK:Menu");
}

/** @brief Live Monitor Suhu — 1 layar raw data. */
static void drawTemperatureMonitor() {
    display_drawHeader("Live Suhu");
    g_u8g2.setFont(u8g2_font_6x10_tf);
    uint8_t y = MENU_FIRST_LINE_Y;
    char lineBuf[32];

    if (s_view.temperatureStatus == SensorStatus::OK) {
        char rawStr[8];
        dtostrf(s_view.temperatureRaw, 4, 1, rawStr);
        char* pRaw = rawStr; while (*pRaw == ' ') pRaw++;
        snprintf(lineBuf, sizeof(lineBuf), "Raw   : %s C", pRaw);
    } else {
        snprintf(lineBuf, sizeof(lineBuf), "Raw   : ERROR");
    }
    g_u8g2.drawStr(2, y, lineBuf); y += MENU_LINE_HEIGHT;

    char offStr[8];
    dtostrf(s_viewCalib.tempOffset, 4, 1, offStr);
    char* pOff = offStr; while (*pOff == ' ') pOff++;
    char sign = (s_viewCalib.tempOffset >= 0.0f) ? '+' : ' ';
    snprintf(lineBuf, sizeof(lineBuf), "Offset: %c%s C", sign, pOff);
    g_u8g2.drawStr(2, y, lineBuf); y += MENU_LINE_HEIGHT;

    if (s_view.temperatureStatus == SensorStatus::OK) {
        char airStr[8];
        dtostrf(s_view.temperature, 4, 1, airStr);
        char* pAir = airStr; while (*pAir == ' ') pAir++;
        snprintf(lineBuf, sizeof(lineBuf), "Air   : %s C", pAir);
    } else {
        snprintf(lineBuf, sizeof(lineBuf), "Air   : ERROR");
    }
    g_u8g2.drawStr(2, y, lineBuf); y += MENU_LINE_HEIGHT;

    snprintf(lineBuf, sizeof(lineBuf), "Status: %s",
             (s_view.temperatureStatus == SensorStatus::OK) ? "OK" : "ERROR");
    g_u8g2.drawStr(2, y, lineBuf);

    display_drawStatusBar("", "BACK:Menu");
}

/**
 * @brief Menggambar layar kalibrasi interaktif. Fungsi tunggal untuk ketiga
 *        wizard (TDS, Turbidity, Suhu) via switch pada currentMenu.
 */
static void drawCalibrationSub() {
    g_u8g2.setFont(u8g2_font_6x10_tf);
    uint8_t y = MENU_FIRST_LINE_Y;
    char lineBuf[40];

    switch (s_viewState.currentMenu) {
        case MenuState::CALIBRATION_TDS_WIZARD: {
            display_drawHeader("Kalibrasi TDS");

            snprintf(lineBuf, sizeof(lineBuf), "ADC Raw : %u", s_view.tdsRaw);
            g_u8g2.drawStr(2, y, lineBuf); y += MENU_LINE_HEIGHT;

            snprintf(lineBuf, sizeof(lineBuf), "Target  : [ %u ppm ]", s_viewState.calibTdsTarget);
            g_u8g2.drawStr(2, y, lineBuf); y += MENU_LINE_HEIGHT;

            if (s_viewState.calibSaving) {
                g_u8g2.drawStr(2, y, "Menyimpan...");
            } else if (s_viewState.calibTdsError) {
                g_u8g2.drawStr(2, y, "! Sinyal TDS lemah!");
            } else {
                g_u8g2.drawStr(2, y, "UP/DN:Atur  OK:Simpan");
            }
            display_drawStatusBar("Celup larutan", "BACK:Batal");
            break;
        }

        case MenuState::CALIBRATION_TURBIDITY_WIZARD: {
            display_drawHeader(s_viewState.calibTurbidityStep == 0
                                ? "Turbidity (1/2)" : "Turbidity (2/2)");

            char vStr[8];
            dtostrf(s_view.turbidityVoltage, 4, 2, vStr);
            char* p = vStr; while (*p == ' ') p++;
            snprintf(lineBuf, sizeof(lineBuf), "Volt   : %s V", p);
            g_u8g2.drawStr(2, y, lineBuf); y += MENU_LINE_HEIGHT;

            if (s_viewState.turbidityCalibFeedback == TurbidityCalibrationFeedback::SAVING) {
                g_u8g2.drawStr(2, y, "Menyimpan...");
            } else if (s_viewState.turbidityCalibFeedback == TurbidityCalibrationFeedback::SUCCESS) {
                g_u8g2.drawStr(2, y, "Kalibrasi berhasil"); y += MENU_LINE_HEIGHT;
                snprintf(lineBuf, sizeof(lineBuf), "%u NTU tersimpan", s_viewState.calibTurbidityTarget);
                g_u8g2.drawStr(2, y, lineBuf);
            } else if (s_viewState.turbidityCalibFeedback == TurbidityCalibrationFeedback::SENSOR_ERROR) {
                g_u8g2.drawStr(2, y, "Turbidity error"); y += MENU_LINE_HEIGHT;
                g_u8g2.drawStr(2, y, "Periksa kabel/probe");
            } else if (s_viewState.turbidityCalibFeedback == TurbidityCalibrationFeedback::VOLTAGE_TOO_LOW) {
                g_u8g2.drawStr(2, y, "Volt terlalu rendah"); y += MENU_LINE_HEIGHT;
                g_u8g2.drawStr(2, y, "Cek probe/larutan");
            } else if (s_viewState.turbidityCalibFeedback == TurbidityCalibrationFeedback::DELTA_V_TOO_SMALL) {
                const float deltaV = s_viewState.calibTurbidityVClear - s_view.turbidityVoltage;
                char dvStr[8];
                dtostrf(deltaV, 4, 3, dvStr);
                char* pDv = dvStr; while (*pDv == ' ') pDv++;
                snprintf(lineBuf, sizeof(lineBuf), "dV kecil: %s V", pDv);
                g_u8g2.drawStr(2, y, lineBuf); y += MENU_LINE_HEIGHT;
                g_u8g2.drawStr(2, y, "Ganti/cek larutan std");
            } else if (s_viewState.calibTurbidityStep == 0) {
                g_u8g2.drawStr(2, y, "Air jernih: OK ambil");
            } else {
                snprintf(lineBuf, sizeof(lineBuf), "Std: %u NTU", s_viewState.calibTurbidityTarget);
                g_u8g2.drawStr(2, y, lineBuf); y += MENU_LINE_HEIGHT;
                char clearStr[8];
                dtostrf(s_viewState.calibTurbidityVClear, 4, 2, clearStr);
                p = clearStr; while (*p == ' ') p++;
                snprintf(lineBuf, sizeof(lineBuf), "V0 : %s V", p);
                g_u8g2.drawStr(2, y, lineBuf); y += MENU_LINE_HEIGHT;
                g_u8g2.drawStr(2, y, "UP/DN atur OK simpan");
            }
            display_drawStatusBar(s_viewState.calibTurbidityStep == 0
                                  ? "Air jernih 0 NTU" : "Larutan standar", "BACK:Batal");
            break;
        }

        case MenuState::CALIBRATION_TEMPERATURE_WIZARD: {
            display_drawHeader("Kalibrasi Suhu");

            char tStr[8];
            dtostrf(s_view.temperatureRaw, 4, 1, tStr);
            char* p = tStr; while (*p == ' ') p++;
            snprintf(lineBuf, sizeof(lineBuf), "Suhu Raw: %s C", p);
            g_u8g2.drawStr(2, y, lineBuf); y += MENU_LINE_HEIGHT;

            char oStr[8];
            dtostrf(s_viewCalib.tempOffset, 4, 1, oStr);
            p = oStr;
            char sign = (s_viewCalib.tempOffset >= 0.0f) ? '+' : ' ';
            snprintf(lineBuf, sizeof(lineBuf), "Offset  : [ %c%s C ]", sign, p);
            g_u8g2.drawStr(2, y, lineBuf); y += MENU_LINE_HEIGHT;

            if (s_viewState.calibSaving) {
                g_u8g2.drawStr(2, y, "Menyimpan...");
            } else {
                g_u8g2.drawStr(2, y, "LF/RT:Atur  OK:Simpan");
            }
            display_drawStatusBar("Cek termometer", "BACK:Batal");
            break;
        }

        default:
            break;
    }
}

static void drawSettings() {
    display_drawHeader("Pengaturan");
    g_u8g2.setFont(u8g2_font_6x10_tf);

    char valueBuf[8];
    for (uint8_t i = 0; i < SETTINGS_ITEM_COUNT; i++) {
        uint8_t y = MENU_FIRST_LINE_Y + static_cast<uint8_t>(i * MENU_LINE_HEIGHT);
        if (y > MENU_LAST_LINE_Y) break;

        if (i == s_viewState.cursorIndex) {
            g_u8g2.drawStr(2, y, s_viewState.settingsAdjustMode ? "*" : ">");
        }
        g_u8g2.drawStr(12, y, SETTINGS_ITEMS[i]);

        if (i == SETTINGS_IDX_BRIGHTNESS) {
            snprintf(valueBuf, sizeof(valueBuf), "%3u", s_viewState.settingsBrightness);
            g_u8g2.drawStr(100, y, valueBuf);
        } else if (i == SETTINGS_IDX_CONTRAST) {
            snprintf(valueBuf, sizeof(valueBuf), "%3u", s_viewState.settingsContrast);
            g_u8g2.drawStr(100, y, valueBuf);
        }
    }
    display_drawStatusBar(s_viewState.calibSaving ? "Menyimpan..." : "OK:Pilih", "BACK:Menu");
}

static void drawAbout() {
    if (s_viewState.aboutSubPage == 0) {
        display_drawHeader("Tentang (1/2)");
        g_u8g2.setFont(u8g2_font_6x10_tf);
        uint8_t y = MENU_FIRST_LINE_Y;

        g_u8g2.drawStr(2, y, "Alat: WQ Analyzer");
        y += MENU_LINE_HEIGHT;

        char fwLine[32];
        snprintf(fwLine, sizeof(fwLine), "FW  : v%s", FIRMWARE_VERSION);
        g_u8g2.drawStr(2, y, fwLine);
        y += MENU_LINE_HEIGHT;

        g_u8g2.drawStr(2, y, "Reg : Permenkes 2023");
        y += MENU_LINE_HEIGHT;

        g_u8g2.drawStr(2, y, "RTOS: FreeRTOS Aktif");

        display_drawStatusBar("DN:Hardware", "BACK:Menu");
    } else {
        display_drawHeader("Tentang (2/2)");
        g_u8g2.setFont(u8g2_font_6x10_tf);
        uint8_t y = MENU_FIRST_LINE_Y;

        g_u8g2.drawStr(2, y, "HW  : Blackpill F401");
        y += MENU_LINE_HEIGHT;

        g_u8g2.drawStr(2, y, "MCU : STM32F401CC");
        y += MENU_LINE_HEIGHT;

        g_u8g2.drawStr(2, y, "OLED: 1.3 SH1106");
        y += MENU_LINE_HEIGHT;

        char line[32];
        snprintf(line, sizeof(line), "Heap: %lu B",
                 static_cast<unsigned long>(xPortGetFreeHeapSize()));
        g_u8g2.drawStr(2, y, line);

        display_drawStatusBar("UP:Firmware", "BACK:Menu");
    }
}

static void drawFactoryResetConfirm() {
    display_drawHeader("Reset Pabrik?");
    g_u8g2.setFont(u8g2_font_6x10_tf);
    uint8_t y = MENU_FIRST_LINE_Y;

    g_u8g2.drawStr(2, y, "Semua kalibrasi &");
    y += MENU_LINE_HEIGHT;
    g_u8g2.drawStr(2, y, "OLED akan direset!");
    y += MENU_LINE_HEIGHT;

    if (s_viewState.calibSaving) {
        g_u8g2.drawStr(2, y, "Menyimpan...");
    } else {
        g_u8g2.drawStr(2, y, "OK:Ya      BACK:Batal");
    }

    display_drawStatusBar("Yakin reset?", "BACK:Batal");
}

// =============================================================================
// DISPATCH TABLE — indeks sesuai nilai enum MenuState
// =============================================================================
typedef void (*DrawFn)();

static DrawFn s_drawTable[static_cast<uint8_t>(MenuState::COUNT)] = {
    drawBootAnimation,                  // BOOT_ANIMATION
    drawSplash,                         // SPLASH
    drawHome,                           // HOME
    drawAmbientTemperatureInput,        // INPUT_AMBIENT_TEMPERATURE
    drawWaitingSampling,                // WAITING_SAMPLING
    drawMeasurement,                    // MEASUREMENT
    drawCalibration,                    // CALIBRATION
    drawCalibrationTdsMenu,             // CALIBRATION_TDS_MENU
    drawCalibrationSub,                 // CALIBRATION_TDS_WIZARD
    drawTdsMonitor,                     // TDS_MONITOR
    drawCalibrationTurbidityMenu,       // CALIBRATION_TURBIDITY_MENU
    drawCalibrationSub,                 // CALIBRATION_TURBIDITY_WIZARD
    drawTurbidityMonitor,               // TURBIDITY_MONITOR
    drawCalibrationTemperatureMenu,     // CALIBRATION_TEMPERATURE_MENU
    drawCalibrationSub,                 // CALIBRATION_TEMPERATURE_WIZARD
    drawTemperatureMonitor,             // TEMPERATURE_MONITOR
    drawSettings,                       // SETTINGS
    drawAbout,                          // ABOUT
    drawFactoryResetConfirm             // FACTORY_RESET_CONFIRM
};

// =============================================================================
// PENERAPAN BRIGHTNESS / CONTRAST (hanya dari TaskOled)
// =============================================================================
static void applyDisplaySettings() {
    static uint8_t lastBrightness = 255;
    static uint8_t lastContrast = 255;

    if (s_viewState.settingsBrightness != lastBrightness) {
        lastBrightness = s_viewState.settingsBrightness;
        display_setBrightness(lastBrightness);
    }
    if (s_viewState.settingsContrast != lastContrast) {
        lastContrast = s_viewState.settingsContrast;
        display_setContrast(lastContrast);
    }
}

// =============================================================================
// API PUBLIK
// =============================================================================

void gui_init() {
    s_bootAnimStartTick = millis();
    s_splashStartTick   = 0;
    s_samplingStartTick = 0;
    // Set langsung tanpa mutex: pre-scheduler, tidak ada task lain.
    g_systemState.previousMenu = MenuState::BOOT_ANIMATION;
    g_systemState.currentMenu = MenuState::BOOT_ANIMATION;
    g_systemState.cursorIndex = 0;
    g_systemState.measurementSubPage = 0;
    g_systemState.aboutSubPage = 0;
    g_systemState.settingsAdjustMode = false;
    g_systemState.calibSaving = false;
    g_systemState.displayDirty = true;
}

void gui_update(const ButtonEventMsg& msg) {
    const bool isActivate = (msg.event == ButtonEvent::PRESSED);
    const bool isRepeatable = (msg.event == ButtonEvent::PRESSED ||
                               msg.event == ButtonEvent::REPEAT);

    if (xSemaphoreTake(g_dataMutex, DATA_MUTEX_TIMEOUT) != pdTRUE) {
        return;
    }

    const MenuState state = g_systemState.currentMenu;

    switch (state) {
        case MenuState::BOOT_ANIMATION:
            if (isActivate) {
                s_splashStartTick = millis();
                transitionToLocked(MenuState::SPLASH);
            }
            break;

        case MenuState::SPLASH:
            if (isActivate) {
                transitionToLocked(MenuState::HOME);
            }
            break;

        case MenuState::HOME:
            if (isRepeatable && msg.id == ButtonID::UP)
                moveCursorLocked(false, HOME_ITEM_COUNT);
            else if (isRepeatable && msg.id == ButtonID::DOWN)
                moveCursorLocked(true, HOME_ITEM_COUNT);
            else if (isActivate && msg.id == ButtonID::OK) {
                switch (g_systemState.cursorIndex) {
                    case 0:
                        g_systemState.activeParameter = WaterParameter::AIR_MINUM_HIGIENE;
                        g_systemState.ambientTemperature = AMBIENT_TEMP_DEFAULT;
                        transitionToLocked(MenuState::INPUT_AMBIENT_TEMPERATURE);
                        break;
                    case 1:
                        g_systemState.activeParameter = WaterParameter::PEMANDIAN_KOLAM;
                        s_samplingStartTick = millis();
                        s_stabilitySampleTick = 0;
                        g_systemState.stabilizationCount = 0;
                        g_systemState.stabilizationTimedOut = false;
                        transitionToLocked(MenuState::WAITING_SAMPLING);
                        break;
                    case 2:
                        transitionToLocked(MenuState::CALIBRATION);
                        break;
                    case 3:
                        transitionToLocked(MenuState::SETTINGS);
                        break;
                    default:
                        break;
                }
            }
            break;

        case MenuState::INPUT_AMBIENT_TEMPERATURE:
            if (isRepeatable && (msg.id == ButtonID::UP || msg.id == ButtonID::DOWN ||
                                 msg.id == ButtonID::LEFT || msg.id == ButtonID::RIGHT)) {
                float step = (msg.id == ButtonID::UP || msg.id == ButtonID::DOWN)
                                 ? AMBIENT_TEMP_FINE_STEP : AMBIENT_TEMP_COARSE_STEP;
                if (msg.id == ButtonID::DOWN || msg.id == ButtonID::LEFT) step = -step;
                g_systemState.ambientTemperature = constrain(g_systemState.ambientTemperature + step,
                                                             AMBIENT_TEMP_MIN, AMBIENT_TEMP_MAX);
                g_systemState.displayDirty = true;
            } else if (isActivate && msg.id == ButtonID::OK) {
                s_samplingStartTick = millis();
                s_stabilitySampleTick = 0;
                g_systemState.stabilizationCount = 0;
                g_systemState.stabilizationTimedOut = false;
                transitionToLocked(MenuState::WAITING_SAMPLING);
            } else if (isActivate && msg.id == ButtonID::BACK) {
                transitionToLocked(MenuState::HOME);
            }
            break;

        case MenuState::WAITING_SAMPLING:
            if (isActivate && msg.id == ButtonID::OK && g_systemState.stabilizationTimedOut) {
                transitionToLocked(MenuState::MEASUREMENT);
            }
            // Tombol BACK membatalkan tunggu pembacaan dan kembali ke Menu Utama
            if (isActivate && msg.id == ButtonID::BACK) {
                transitionToLocked(MenuState::HOME);
            }
            break;

        case MenuState::MEASUREMENT:
            // Navigasi tiga halaman: Hasil -> Diagnosis -> Saran.
            if (isActivate && (msg.id == ButtonID::DOWN || msg.id == ButtonID::OK)) {
                if (g_systemState.measurementSubPage < 2) {
                    g_systemState.measurementSubPage++;
                    g_systemState.displayDirty = true;
                }
            } else if (isActivate && (msg.id == ButtonID::UP)) {
                if (g_systemState.measurementSubPage > 0) {
                    g_systemState.measurementSubPage--;
                    g_systemState.displayDirty = true;
                }
            } else if (isActivate && msg.id == ButtonID::BACK) {
                transitionToLocked(MenuState::HOME);
            }
            break;

        case MenuState::CALIBRATION:
            if (isRepeatable && msg.id == ButtonID::UP)
                moveCursorLocked(false, CALIBRATION_ITEM_COUNT);
            else if (isRepeatable && msg.id == ButtonID::DOWN)
                moveCursorLocked(true, CALIBRATION_ITEM_COUNT);
            else if (isActivate && msg.id == ButtonID::OK) {
                if (g_systemState.cursorIndex == 0) {
                    transitionToLocked(MenuState::CALIBRATION_TDS_MENU);
                } else if (g_systemState.cursorIndex == 1) {
                    transitionToLocked(MenuState::CALIBRATION_TURBIDITY_MENU);
                } else if (g_systemState.cursorIndex == 2) {
                    transitionToLocked(MenuState::CALIBRATION_TEMPERATURE_MENU);
                } else if (g_systemState.cursorIndex == 3) {
                    // Masuk ke halaman konfirmasi Reset Pabrik
                    transitionToLocked(MenuState::FACTORY_RESET_CONFIRM);
                }
            } else if (isActivate && msg.id == ButtonID::BACK) {
                transitionToLocked(MenuState::HOME);
            }
            break;

        case MenuState::CALIBRATION_TDS_MENU:
            if (isRepeatable && msg.id == ButtonID::UP)
                moveCursorLocked(false, SENSOR_SUB_ITEM_COUNT);
            else if (isRepeatable && msg.id == ButtonID::DOWN)
                moveCursorLocked(true, SENSOR_SUB_ITEM_COUNT);
            else if (isActivate && msg.id == ButtonID::OK) {
                if (g_systemState.cursorIndex == 0) {
                    g_systemState.calibTdsError = false;
                    transitionToLocked(MenuState::CALIBRATION_TDS_WIZARD);
                } else {
                    transitionToLocked(MenuState::TDS_MONITOR);
                }
            } else if (isActivate && msg.id == ButtonID::BACK) {
                transitionToLocked(MenuState::CALIBRATION);
            }
            break;

        case MenuState::CALIBRATION_TDS_WIZARD:
            if (isRepeatable && msg.id == ButtonID::UP) {
                if (g_systemState.calibTdsTarget + TDS_CALIB_TARGET_STEP
                    <= TDS_CALIB_TARGET_MAX) {
                    g_systemState.calibTdsTarget += TDS_CALIB_TARGET_STEP;
                }
                g_systemState.displayDirty = true;
            } else if (isRepeatable && msg.id == ButtonID::DOWN) {
                if (g_systemState.calibTdsTarget >= TDS_CALIB_TARGET_STEP
                    + TDS_CALIB_TARGET_MIN) {
                    g_systemState.calibTdsTarget -= TDS_CALIB_TARGET_STEP;
                }
                g_systemState.displayDirty = true;
            } else if (isActivate && msg.id == ButtonID::OK) {
                float rawUncalib = 0.0f;
                if (g_sensorData.tdsStatus == SensorStatus::OK &&
                    g_calibParams.tdsKFactor > 0.001f) {
                    rawUncalib = g_sensorData.tdsFiltered / g_calibParams.tdsKFactor;
                }
                if (rawUncalib > 10.0f) {
                    g_calibParams.tdsKFactor =
                        static_cast<float>(g_systemState.calibTdsTarget) / rawUncalib;
                    if (g_calibParams.tdsKFactor < TDS_KFACTOR_MIN)
                        g_calibParams.tdsKFactor = TDS_KFACTOR_MIN;
                    if (g_calibParams.tdsKFactor > TDS_KFACTOR_MAX)
                        g_calibParams.tdsKFactor = TDS_KFACTOR_MAX;
                    storage_requestSave(g_calibParams);
                    g_systemState.calibSaving = true;
                    g_systemState.calibTdsError = false;
                    transitionToLocked(MenuState::CALIBRATION_TDS_MENU);
                } else {
                    // Sinyal TDS terlalu lemah — tampilkan error, jangan pindah halaman
                    g_systemState.calibTdsError = true;
                    g_systemState.displayDirty = true;
                }
            } else if (isActivate && msg.id == ButtonID::BACK) {
                transitionToLocked(MenuState::CALIBRATION_TDS_MENU);
            }
            break;

        case MenuState::TDS_MONITOR:
            if (isActivate && (msg.id == ButtonID::BACK || msg.id == ButtonID::OK)) {
                transitionToLocked(MenuState::CALIBRATION_TDS_MENU);
            }
            break;

        case MenuState::CALIBRATION_TURBIDITY_MENU:
            if (isRepeatable && msg.id == ButtonID::UP)
                moveCursorLocked(false, SENSOR_SUB_ITEM_COUNT);
            else if (isRepeatable && msg.id == ButtonID::DOWN)
                moveCursorLocked(true, SENSOR_SUB_ITEM_COUNT);
            else if (isActivate && msg.id == ButtonID::OK) {
                if (g_systemState.cursorIndex == 0) {
                    // Kalibrasi — masuk wizard
                    g_systemState.calibTurbidityStep = 0;
                    g_systemState.calibTurbidityVClear = 0.0f;
                    g_systemState.turbidityCalibFeedback = TurbidityCalibrationFeedback::NONE;
                    transitionToLocked(MenuState::CALIBRATION_TURBIDITY_WIZARD);
                } else {
                    // Live Monitor
                    transitionToLocked(MenuState::TURBIDITY_MONITOR);
                }
            } else if (isActivate && msg.id == ButtonID::BACK) {
                transitionToLocked(MenuState::CALIBRATION);
            }
            break;

        case MenuState::CALIBRATION_TURBIDITY_WIZARD:
            if (g_systemState.turbidityCalibFeedback == TurbidityCalibrationFeedback::SAVING ||
                g_systemState.turbidityCalibFeedback == TurbidityCalibrationFeedback::SUCCESS) {
                break;
            }
            if (s_viewState.calibTurbidityStep == 0 && isActivate && msg.id == ButtonID::OK) {
                float volt = g_sensorData.turbidityVoltage;
                if (g_sensorData.turbidityStatus != SensorStatus::OK) {
                    g_systemState.turbidityCalibFeedback = TurbidityCalibrationFeedback::SENSOR_ERROR;
                    g_systemState.displayDirty = true;
                } else if (volt > TURBIDITY_VCLEAR_MIN) {
                    g_systemState.calibTurbidityVClear = volt;
                    g_systemState.calibTurbidityStep = 1;
                    g_systemState.calibTurbidityTarget = static_cast<uint16_t>(g_calibParams.turbidityNtuStandard);
                    g_systemState.turbidityCalibFeedback = TurbidityCalibrationFeedback::NONE;
                    g_systemState.displayDirty = true;
                } else {
                    g_systemState.turbidityCalibFeedback = TurbidityCalibrationFeedback::VOLTAGE_TOO_LOW;
                    g_systemState.displayDirty = true;
                }
            } else if (s_viewState.calibTurbidityStep == 1 && isRepeatable && msg.id == ButtonID::UP) {
                if (g_systemState.calibTurbidityTarget + TURBIDITY_NTU_STANDARD_STEP <= TURBIDITY_NTU_STANDARD_MAX) {
                    g_systemState.calibTurbidityTarget += TURBIDITY_NTU_STANDARD_STEP;
                }
                g_systemState.turbidityCalibFeedback = TurbidityCalibrationFeedback::NONE;
                g_systemState.displayDirty = true;
            } else if (s_viewState.calibTurbidityStep == 1 && isRepeatable && msg.id == ButtonID::DOWN) {
                if (g_systemState.calibTurbidityTarget > TURBIDITY_NTU_STANDARD_MIN + TURBIDITY_NTU_STANDARD_STEP - 1) {
                    g_systemState.calibTurbidityTarget -= TURBIDITY_NTU_STANDARD_STEP;
                }
                g_systemState.turbidityCalibFeedback = TurbidityCalibrationFeedback::NONE;
                g_systemState.displayDirty = true;
            } else if (s_viewState.calibTurbidityStep == 1 && isActivate && msg.id == ButtonID::OK) {
                const float volt = g_sensorData.turbidityVoltage;
                const float deltaV = volt - g_systemState.calibTurbidityVClear;
                if (g_sensorData.turbidityStatus == SensorStatus::OK &&
                    fabsf(deltaV) >= TURBIDITY_MIN_CALIBRATION_DELTA_V) {
                    g_calibParams.turbidityVClear = g_systemState.calibTurbidityVClear;
                    g_calibParams.turbidityVStandard = volt;
                    g_calibParams.turbidityNtuStandard = static_cast<float>(g_systemState.calibTurbidityTarget);
                    storage_requestSave(g_calibParams);
                    g_systemState.calibSaving = true;
                    g_systemState.turbidityCalibFeedback = TurbidityCalibrationFeedback::SUCCESS;
                    g_systemState.turbidityCalibSuccessTick = millis();
                    g_systemState.displayDirty = true;
                } else if (g_sensorData.turbidityStatus != SensorStatus::OK) {
                    g_systemState.turbidityCalibFeedback = TurbidityCalibrationFeedback::SENSOR_ERROR;
                    g_systemState.displayDirty = true;
                } else {
                    g_systemState.turbidityCalibFeedback = TurbidityCalibrationFeedback::DELTA_V_TOO_SMALL;
                    g_systemState.displayDirty = true;
                }
            } else if (isActivate && msg.id == ButtonID::BACK) {
                g_systemState.turbidityCalibFeedback = TurbidityCalibrationFeedback::NONE;
                transitionToLocked(MenuState::CALIBRATION_TURBIDITY_MENU);
            }
            break;

        case MenuState::TURBIDITY_MONITOR:
            if (isActivate && (msg.id == ButtonID::BACK || msg.id == ButtonID::OK)) {
                transitionToLocked(MenuState::CALIBRATION_TURBIDITY_MENU);
            }
            break;

        case MenuState::CALIBRATION_TEMPERATURE_MENU:
            if (isRepeatable && msg.id == ButtonID::UP)
                moveCursorLocked(false, SENSOR_SUB_ITEM_COUNT);
            else if (isRepeatable && msg.id == ButtonID::DOWN)
                moveCursorLocked(true, SENSOR_SUB_ITEM_COUNT);
            else if (isActivate && msg.id == ButtonID::OK) {
                if (g_systemState.cursorIndex == 0) {
                    transitionToLocked(MenuState::CALIBRATION_TEMPERATURE_WIZARD);
                } else {
                    transitionToLocked(MenuState::TEMPERATURE_MONITOR);
                }
            } else if (isActivate && msg.id == ButtonID::BACK) {
                transitionToLocked(MenuState::CALIBRATION);
            }
            break;

        case MenuState::CALIBRATION_TEMPERATURE_WIZARD:
            if (isRepeatable && (msg.id == ButtonID::LEFT ||
                                 msg.id == ButtonID::RIGHT ||
                                 msg.id == ButtonID::UP ||
                                 msg.id == ButtonID::DOWN)) {
                float delta = (msg.id == ButtonID::RIGHT || msg.id == ButtonID::UP)
                                  ? TEMP_OFFSET_STEP : -TEMP_OFFSET_STEP;
                g_calibParams.tempOffset += delta;
                if (g_calibParams.tempOffset > TEMP_OFFSET_LIMIT)
                    g_calibParams.tempOffset = TEMP_OFFSET_LIMIT;
                if (g_calibParams.tempOffset < -TEMP_OFFSET_LIMIT)
                    g_calibParams.tempOffset = -TEMP_OFFSET_LIMIT;
                g_systemState.displayDirty = true;
            } else if (isActivate && msg.id == ButtonID::OK) {
                storage_requestSave(g_calibParams);
                g_systemState.calibSaving = true;
                transitionToLocked(MenuState::CALIBRATION_TEMPERATURE_MENU);
            } else if (isActivate && msg.id == ButtonID::BACK) {
                transitionToLocked(MenuState::CALIBRATION_TEMPERATURE_MENU);
            }
            break;

        case MenuState::TEMPERATURE_MONITOR:
            if (isActivate && (msg.id == ButtonID::BACK || msg.id == ButtonID::OK)) {
                transitionToLocked(MenuState::CALIBRATION_TEMPERATURE_MENU);
            }
            break;

        case MenuState::SETTINGS:
            if (!g_systemState.settingsAdjustMode) {
                if (isRepeatable && msg.id == ButtonID::UP)
                    moveCursorLocked(false, SETTINGS_ITEM_COUNT);
                else if (isRepeatable && msg.id == ButtonID::DOWN)
                    moveCursorLocked(true, SETTINGS_ITEM_COUNT);
                else if (isActivate && msg.id == ButtonID::OK) {
                    if (g_systemState.cursorIndex == SETTINGS_IDX_BRIGHTNESS ||
                        g_systemState.cursorIndex == SETTINGS_IDX_CONTRAST) {
                        g_systemState.settingsAdjustMode = true;
                        g_systemState.displayDirty = true;
                    } else if (g_systemState.cursorIndex == SETTINGS_IDX_RESET) {
                        g_systemState.settingsBrightness = DISPLAY_DEFAULT_BRIGHTNESS;
                        g_systemState.settingsContrast = DISPLAY_DEFAULT_CONTRAST;
                        g_calibParams.displayBrightness = DISPLAY_DEFAULT_BRIGHTNESS;
                        g_calibParams.displayContrast = DISPLAY_DEFAULT_CONTRAST;
                        storage_requestSave(g_calibParams);
                        g_systemState.calibSaving = true;
                        g_systemState.displayDirty = true;
                    } else if (g_systemState.cursorIndex == SETTINGS_IDX_INFO) {
                        transitionToLocked(MenuState::ABOUT);
                    }
                } else if (isActivate && msg.id == ButtonID::BACK) {
                    transitionToLocked(MenuState::HOME);
                }
            } else {
                if (isRepeatable && (msg.id == ButtonID::LEFT ||
                                     msg.id == ButtonID::RIGHT)) {
                    int16_t delta = (msg.id == ButtonID::RIGHT)
                                        ? DISPLAY_LEVEL_STEP : -DISPLAY_LEVEL_STEP;
                    uint8_t* target =
                        (g_systemState.cursorIndex == SETTINGS_IDX_BRIGHTNESS)
                            ? &g_systemState.settingsBrightness
                            : &g_systemState.settingsContrast;
                    int16_t newValue = static_cast<int16_t>(*target) + delta;
                    newValue = constrain(newValue, DISPLAY_MIN_LEVEL,
                                         DISPLAY_MAX_LEVEL);
                    *target = static_cast<uint8_t>(newValue);
                    g_systemState.displayDirty = true;
                } else if (isActivate && (msg.id == ButtonID::OK ||
                                          msg.id == ButtonID::BACK)) {
                    g_systemState.settingsAdjustMode = false;
                    g_calibParams.displayBrightness = g_systemState.settingsBrightness;
                    g_calibParams.displayContrast = g_systemState.settingsContrast;
                    storage_requestSave(g_calibParams);
                    g_systemState.calibSaving = true;
                    g_systemState.displayDirty = true;
                }
            }
            break;

        case MenuState::ABOUT:
            if (isActivate && (msg.id == ButtonID::DOWN || msg.id == ButtonID::OK)) {
                if (g_systemState.aboutSubPage == 0) {
                    g_systemState.aboutSubPage = 1;
                    g_systemState.displayDirty = true;
                }
            } else if (isActivate && msg.id == ButtonID::UP) {
                if (g_systemState.aboutSubPage == 1) {
                    g_systemState.aboutSubPage = 0;
                    g_systemState.displayDirty = true;
                }
            } else if (isActivate && msg.id == ButtonID::BACK) {
                transitionToLocked(g_systemState.previousMenu);
            }
            break;

        case MenuState::FACTORY_RESET_CONFIRM:
            if (isActivate && msg.id == ButtonID::OK) {
                storage_loadFactoryDefaults(g_calibParams);
                storage_requestSave(g_calibParams);
                g_systemState.settingsBrightness = g_calibParams.displayBrightness;
                g_systemState.settingsContrast   = g_calibParams.displayContrast;
                g_systemState.calibSaving = true;
                g_systemState.displayDirty = true;
                transitionToLocked(MenuState::CALIBRATION);
            } else if (isActivate && msg.id == ButtonID::BACK) {
                transitionToLocked(MenuState::CALIBRATION);
            }
            break;

        default:
            break;
    }

    xSemaphoreGive(g_dataMutex);
}

void gui_tick() {
    // 1. Transisi otomatis Boot Animation Air (5 detik)
    if (g_systemState.currentMenu == MenuState::BOOT_ANIMATION) {
        if ((millis() - s_bootAnimStartTick) >= BOOT_ANIMATION_MS) {
            if (xSemaphoreTake(g_dataMutex, DATA_MUTEX_TIMEOUT) == pdTRUE) {
                s_splashStartTick = millis();
                transitionToLocked(MenuState::SPLASH);
                xSemaphoreGive(g_dataMutex);
            }
        } else {
            // Picu rendering frame berikutnya secara kontinu selama animasi berlangsung
            if (xSemaphoreTake(g_dataMutex, DATA_MUTEX_TIMEOUT) == pdTRUE) {
                g_systemState.displayDirty = true;
                xSemaphoreGive(g_dataMutex);
            }
        }
    }
    // 2. Tampilkan bukti penyimpanan kalibrasi sebelum kembali ke sub-menu.
    else if (g_systemState.currentMenu == MenuState::CALIBRATION_TURBIDITY_WIZARD) {
        if (xSemaphoreTake(g_dataMutex, DATA_MUTEX_TIMEOUT) == pdTRUE) {
            if (g_systemState.turbidityCalibFeedback == TurbidityCalibrationFeedback::SUCCESS &&
                millis() - g_systemState.turbidityCalibSuccessTick >= TURBIDITY_CALIB_SUCCESS_DISPLAY_MS) {
                g_systemState.turbidityCalibFeedback = TurbidityCalibrationFeedback::NONE;
                transitionToLocked(MenuState::CALIBRATION_TURBIDITY_MENU);
            }
            xSemaphoreGive(g_dataMutex);
        }
    }
    // 2. Transisi otomatis Splash Screen (2 detik)
    else if (g_systemState.currentMenu == MenuState::SPLASH) {
        if ((millis() - s_splashStartTick) >= SPLASH_SCREEN_MS) {
            if (xSemaphoreTake(g_dataMutex, DATA_MUTEX_TIMEOUT) == pdTRUE) {
                transitionToLocked(MenuState::HOME);
                xSemaphoreGive(g_dataMutex);
            }
        }
    }
    // 3. Stabilisasi suhu probe setelah dicelupkan ke sampel.
    else if (g_systemState.currentMenu == MenuState::WAITING_SAMPLING) {
        if (xSemaphoreTake(g_dataMutex, DATA_MUTEX_TIMEOUT) == pdTRUE) {
            const uint32_t now = millis();
            if (!g_systemState.stabilizationTimedOut &&
                g_sensorData.temperatureStatus == SensorStatus::OK &&
                now - s_stabilitySampleTick >= TASK_PERIOD_TEMPERATURE_MS) {
                if (g_systemState.stabilizationCount == 0) {
                    s_stabilityMin = g_sensorData.temperature;
                    s_stabilityMax = g_sensorData.temperature;
                } else {
                    if (g_sensorData.temperature < s_stabilityMin) s_stabilityMin = g_sensorData.temperature;
                    if (g_sensorData.temperature > s_stabilityMax) s_stabilityMax = g_sensorData.temperature;
                }
                if (s_stabilityMax - s_stabilityMin <= TEMP_STABLE_DELTA_C) {
                    g_systemState.stabilizationCount++;
                } else {
                    g_systemState.stabilizationCount = 1;
                    s_stabilityMin = g_sensorData.temperature;
                    s_stabilityMax = g_sensorData.temperature;
                }
                s_stabilitySampleTick = now;
            }
            if (g_systemState.stabilizationCount >= TEMP_STABLE_REQUIRED_SAMPLES) {
                transitionToLocked(MenuState::MEASUREMENT);
            } else if (now - s_samplingStartTick >= TEMP_STABILIZATION_TIMEOUT_MS) {
                g_systemState.stabilizationTimedOut = true;
                g_systemState.displayDirty = true;
            } else {
                g_systemState.displayDirty = true;
            }
            xSemaphoreGive(g_dataMutex);
        }
    }
}

void gui_draw() {
    if (xSemaphoreTake(g_dataMutex, DATA_MUTEX_TIMEOUT) != pdTRUE) {
        return;
    }
    s_view      = g_sensorData;
    s_viewState = g_systemState;
    s_viewCalib = g_calibParams;
    xSemaphoreGive(g_dataMutex);

    applyDisplaySettings();

    uint8_t stateIndex = static_cast<uint8_t>(s_viewState.currentMenu);
    if (stateIndex >= static_cast<uint8_t>(MenuState::COUNT)) {
        stateIndex = static_cast<uint8_t>(MenuState::HOME);
    }

    g_u8g2.clearBuffer();
    s_drawTable[stateIndex]();
    g_u8g2.sendBuffer();
}
