/**
 * @file    gui.cpp
 * @brief   Implementasi GUI Manager berbasis Finite State Machine (FSM).
 *          Alur Workflow: Splash (FBN) -> Menu Utama (Pilih Mode) ->
 *          Screen Tunggu / Stabilisasi Sensor (5s) -> Dashboard Hasil (1/2) <->
 *          Detail Rekomendasi (2/2) -> BACK ke Menu Utama.
 *
 *          Pola snapshot thread-safe: gui_draw() mengambil salinan data di
 *          bawah g_dataMutex sebelum merender layar OLED SSD1306.
 */

#include "gui.h"
#include "display.h"
#include "config.h"
#include "storage.h"
#include <STM32FreeRTOS.h>
#include <stdio.h>

// =============================================================================
// KONSTANTA DAFTAR MENU
// =============================================================================
static const char* const HOME_ITEMS[] = {
    "Air Minum & Higiene",
    "Pemandian / Kolam",
    "Kalibrasi Sensor",
    "Pengaturan OLED"
};
static constexpr uint8_t HOME_ITEM_COUNT = 4;

static const char* const CALIBRATION_ITEMS[] = {
    "Kalibrasi TDS",
    "Kalibrasi Turbidity",
    "Kalibrasi Suhu",
    "Reset Pabrik"
};
static constexpr uint8_t CALIBRATION_ITEM_COUNT = 4;

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
static uint32_t s_splashStartTick   = 0;
static uint32_t s_samplingStartTick = 0;

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
    g_systemState.previousMenu = g_systemState.currentMenu;
    g_systemState.currentMenu = newState;
    g_systemState.cursorIndex = 0;
    g_systemState.measurementSubPage = 0;
    g_systemState.settingsAdjustMode = false;
    g_systemState.displayDirty = true;
}

static void moveCursorLocked(bool moveDown, uint8_t itemCount) {
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
    uint8_t textWidth = g_u8g2.getStrWidth(text);
    if (textWidth >= DISPLAY_WIDTH) return 0;
    return static_cast<uint8_t>((DISPLAY_WIDTH - textWidth) / 2);
}

/** @brief Viewport bergulir: 4 baris terlihat, kursor selalu dalam view. */
static void drawSimpleList(const char* title, const char* const* items,
                            uint8_t count, uint8_t cursor) {
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
        if (i == cursor) {
            g_u8g2.drawStr(2, y, ">");
        }
        g_u8g2.drawStr(12, y, items[i]);
    }

    // Indikator scroll
    if (firstVisible > 0) {
        g_u8g2.drawStr(120, MENU_FIRST_LINE_Y, "^");
    }
    if (count > firstVisible + MENU_VISIBLE_ROWS) {
        g_u8g2.drawStr(120, static_cast<uint8_t>(
            DISPLAY_HEIGHT - DISPLAY_STATUSBAR_H - 1), "v");
    }
}

/** @brief Splash Screen (Sesuai Note/INTRO: Physic Water Quality Index FBN) */
static void drawSplash() {
    static const char* const titleLine1 = "Physic Water";
    static const char* const titleLine2 = "Quality Index";
    static const char* const titleLine3 = "FBN";

    g_u8g2.setFont(u8g2_font_7x14B_tf);
    g_u8g2.drawStr(centeredX(titleLine1), 20, titleLine1);
    g_u8g2.drawStr(centeredX(titleLine2), 35, titleLine2);
    g_u8g2.drawStr(centeredX(titleLine3), 49, titleLine3);

    g_u8g2.setFont(u8g2_font_5x7_tf);
    char versionLine[24];
    snprintf(versionLine, sizeof(versionLine), "v%s", FIRMWARE_VERSION);
    g_u8g2.drawStr(centeredX(versionLine), 61, versionLine);
}

/** @brief Menu Utama (Pemilihan Objek Air & Fitur) */
static void drawHome() {
    drawSimpleList("Pilih Mode Uji Air", HOME_ITEMS, HOME_ITEM_COUNT, s_viewState.cursorIndex);
    display_drawStatusBar("UP/DN:Pilih", "OK:Masuk");
}

/** @brief Screen Tunggu / Stabilisasi Pembacaan Sensor (5 Detik) */
static void drawWaitingSampling() {
    const char* modeTitle = (s_viewState.activeParameter == WaterParameter::AIR_MINUM_HIGIENE)
                                ? "MODE: AIR MINUM"
                                : "MODE: PEMANDIAN/KOLAM";
    display_drawHeader(modeTitle);

    g_u8g2.setFont(u8g2_font_6x10_tf);
    g_u8g2.drawStr(12, 26, "Membaca Sensor...");

    // Progress bar dinamis (durasi SAMPLING_SCREEN_MS = 5000 ms)
    uint32_t elapsed = millis() - s_samplingStartTick;
    if (elapsed > SAMPLING_SCREEN_MS) elapsed = SAMPLING_SCREEN_MS;
    float progress = static_cast<float>(elapsed) / static_cast<float>(SAMPLING_SCREEN_MS);

    // Bingkai progress bar
    constexpr uint8_t barX = 14;
    constexpr uint8_t barY = 34;
    constexpr uint8_t barW = 100;
    constexpr uint8_t barH = 10;
    g_u8g2.drawFrame(barX, barY, barW, barH);

    // Isi progress bar
    uint8_t fillW = static_cast<uint8_t>(progress * (barW - 4));
    if (fillW > 0) {
        g_u8g2.drawBox(barX + 2, barY + 2, fillW, barH - 4);
    }

    display_drawStatusBar("Stabilisasi...", "BACK:Batal");
}

/** @brief Mencetak satu baris nilai sensor dengan label dan satuan. */
static void drawSensorLine(uint8_t y, const char* label, float value,
                            const char* unit, SensorStatus status) {
    g_u8g2.setFont(u8g2_font_6x10_tf);
    char line[32];
    if (status == SensorStatus::OK) {
        char valueStr[12];
        dtostrf(value, 4, 1, valueStr);
        char* p = valueStr;
        while (*p == ' ') p++;
        snprintf(line, sizeof(line), "%s %s %s", label, p, unit);
    } else {
        snprintf(line, sizeof(line), "%s ERROR", label);
    }
    g_u8g2.drawStr(2, y, line);
}

/** @brief Memecah teks panjang agar muat di lebar layar (maks 2 baris). */
static void drawWrappedText(uint8_t x, uint8_t y, const char* text) {
    const uint8_t maxWidth = static_cast<uint8_t>(DISPLAY_WIDTH - x - 2);
    char buf[64];
    strncpy(buf, text, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    uint8_t lineY = y;
    char* start = buf;
    for (uint8_t line = 0; line < 2 && *start != '\0'; line++) {
        if (lineY > DISPLAY_HEIGHT - DISPLAY_STATUSBAR_H) break;

        char* end = start;
        char* lastSpace = nullptr;

        while (*end != '\0') {
            char save = end[1];
            end[1] = '\0';
            bool fits = (g_u8g2.getStrWidth(start) <= maxWidth);
            end[1] = save;
            if (!fits) break;
            if (*end == ' ') lastSpace = end;
            end++;
        }

        if (end == start) break;

        if (lastSpace != nullptr && lastSpace > start) {
            *lastSpace = '\0';
            g_u8g2.drawStr(x, lineY, start);
            lineY += MENU_LINE_HEIGHT;
            start = lastSpace + 1;
        } else {
            char save = *end;
            *end = '\0';
            g_u8g2.drawStr(x, lineY, start);
            lineY += MENU_LINE_HEIGHT;
            if (save != '\0') { *end = save; start = end; }
            else start = end;
        }
    }
}

/** @brief Dashboard Hasil Pengukuran & Detail Rekomendasi (Dual-Page View) */
static void drawMeasurement() {
    if (s_viewState.measurementSubPage == 0) {
        // --- HALAMAN 1: DATA SENSOR + SKOR FUZZY (DASHBOARD) ---
        const char* pageHeader = (s_viewState.activeParameter == WaterParameter::AIR_MINUM_HIGIENE)
                                     ? "Air Minum (1/2)"
                                     : "Pemandian/Kolam (1/2)";
        display_drawHeader(pageHeader);
        uint8_t y = MENU_FIRST_LINE_Y;

        // Suhu + status suhu
        char tempBuf[32];
        if (s_view.temperatureStatus == SensorStatus::OK) {
            char tStr[8];
            dtostrf(s_view.temperature, 4, 1, tStr);
            char* p = tStr;
            while (*p == ' ') p++;
            snprintf(tempBuf, sizeof(tempBuf), "Suhu : %s C (%s)", p,
                     FuzzyKualitasAir_GetStatusSuhuStr(s_view.tempStatus));
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

    } else {
        // --- HALAMAN 2: DETAIL REKOMENDASI TINDAKAN ---
        display_drawHeader("Rekomendasi (2/2)");
        uint8_t y = MENU_FIRST_LINE_Y;
        g_u8g2.setFont(u8g2_font_6x10_tf);

        const char* qStr = (s_view.qualityStatus == STATUS_EXCELLENT)    ? "EXCELLENT" :
                           (s_view.qualityStatus == STATUS_GOOD)         ? "GOOD [Baik]" :
                           (s_view.qualityStatus == STATUS_POOR)         ? "POOR [Kurang]" :
                           (s_view.qualityStatus == STATUS_VERY_POOR)    ? "VERY POOR" : "TIDAK LAYAK";
        char lineBuf[32];
        snprintf(lineBuf, sizeof(lineBuf), "Mutu  : %s", qStr);
        g_u8g2.drawStr(2, y, lineBuf);
        y += MENU_LINE_HEIGHT;

        if (s_view.temperatureStatus == SensorStatus::OK) {
            char tStr[8];
            dtostrf(s_view.temperature, 4, 1, tStr);
            char* p = tStr;
            while (*p == ' ') p++;
            snprintf(lineBuf, sizeof(lineBuf), "Suhu  : %sC [%s]",
                     p, FuzzyKualitasAir_GetStatusSuhuStr(s_view.tempStatus));
        } else {
            snprintf(lineBuf, sizeof(lineBuf), "Suhu  : ERROR");
        }
        g_u8g2.drawStr(2, y, lineBuf);
        y += MENU_LINE_HEIGHT;

        g_u8g2.drawStr(2, y, "Saran :");
        y += MENU_LINE_HEIGHT;
        const char* pesan = FuzzyKualitasAir_GetPesan(s_view.qualityStatus);
        drawWrappedText(4, y, pesan);

        display_drawStatusBar("UP:Kembali", "BACK:Menu");
    }
}

static void drawCalibration() {
    drawSimpleList("Kalibrasi Sensor", CALIBRATION_ITEMS, CALIBRATION_ITEM_COUNT,
                   s_viewState.cursorIndex);
    if (s_viewState.calibSaving) {
        display_drawStatusBar("Menyimpan...", nullptr);
    } else {
        display_drawStatusBar("OK:Pilih", "BACK:Menu");
    }
}

/**
 * @brief Menggambar layar kalibrasi interaktif. Fungsi tunggal untuk ketiga
 *        sub-menu (TDS, Turbidity, Suhu) via switch pada currentMenu.
 */
static void drawCalibrationSub() {
    g_u8g2.setFont(u8g2_font_6x10_tf);
    uint8_t y = MENU_FIRST_LINE_Y;
    char lineBuf[40];

    switch (s_viewState.currentMenu) {
        case MenuState::CALIBRATION_TDS: {
            display_drawHeader("Kalibrasi TDS");

            snprintf(lineBuf, sizeof(lineBuf), "ADC Raw : %u",
                     s_view.tdsRaw);
            g_u8g2.drawStr(2, y, lineBuf); y += MENU_LINE_HEIGHT;

            snprintf(lineBuf, sizeof(lineBuf), "Target  : [ %u ppm ]",
                     s_viewState.calibTdsTarget);
            g_u8g2.drawStr(2, y, lineBuf); y += MENU_LINE_HEIGHT;

            if (s_viewState.calibSaving) {
                g_u8g2.drawStr(2, y, "Menyimpan...");
            } else {
                g_u8g2.drawStr(2, y, "UP/DN:Target OK:Simpan");
            }
            display_drawStatusBar("UP/DN:Ubah", "BACK:Batal");
            break;
        }

        case MenuState::CALIBRATION_TURBIDITY: {
            display_drawHeader("Kalibrasi Turbidity");

            char vStr[8];
            dtostrf(s_view.turbidityVoltage, 4, 2, vStr);
            char* p = vStr;
            while (*p == ' ') p++;
            snprintf(lineBuf, sizeof(lineBuf), "Volt   : %s V", p);
            g_u8g2.drawStr(2, y, lineBuf); y += MENU_LINE_HEIGHT;

            char vcStr[8];
            dtostrf(s_viewCalib.turbidityVClear, 4, 2, vcStr);
            p = vcStr;
            while (*p == ' ') p++;
            snprintf(lineBuf, sizeof(lineBuf), "V_Clear: %s V", p);
            g_u8g2.drawStr(2, y, lineBuf); y += MENU_LINE_HEIGHT;

            if (s_viewState.calibSaving) {
                g_u8g2.drawStr(2, y, "Menyimpan...");
            } else {
                g_u8g2.drawStr(2, y, "Tekan OK: Lock 0 NTU");
            }
            display_drawStatusBar("Air Aquades", "BACK:Batal");
            break;
        }

        case MenuState::CALIBRATION_TEMPERATURE: {
            display_drawHeader("Kalibrasi Suhu");

            char tStr[8];
            dtostrf(s_view.temperatureRaw, 4, 1, tStr);
            char* p = tStr;
            while (*p == ' ') p++;
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
                g_u8g2.drawStr(2, y, "LF/RT:Offset OK:Simpan");
            }
            display_drawStatusBar("LF/RT:Ubah", "BACK:Batal");
            break;
        }

        default:
            break;
    }
}

static void drawSettings() {
    display_drawHeader("Pengaturan OLED");
    g_u8g2.setFont(u8g2_font_6x10_tf);

    char valueBuf[8];
    for (uint8_t i = 0; i < SETTINGS_ITEM_COUNT; i++) {
        uint8_t y = MENU_FIRST_LINE_Y + static_cast<uint8_t>(i * MENU_LINE_HEIGHT);
        if (y > DISPLAY_HEIGHT - DISPLAY_STATUSBAR_H) break;

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
    if (s_viewState.settingsAdjustMode) {
        display_drawStatusBar("LF/RT:Ubah", "OK:Selesai");
    } else {
        display_drawStatusBar("OK:Atur", "BACK:Menu");
    }
}

static void drawAbout() {
    display_drawHeader("Tentang Alat");
    g_u8g2.setFont(u8g2_font_5x7_tf);

    char line[48];
    uint8_t y = MENU_FIRST_LINE_Y;

    snprintf(line, sizeof(line), "Alat: %s", FIRMWARE_NAME);
    g_u8g2.drawStr(2, y, line); y += 9;

    snprintf(line, sizeof(line), "FW  : v%s (FBN)", FIRMWARE_VERSION);
    g_u8g2.drawStr(2, y, line); y += 9;

    snprintf(line, sizeof(line), "HW  : %s", HARDWARE_VERSION);
    g_u8g2.drawStr(2, y, line); y += 9;

    snprintf(line, sizeof(line), "MCU : %s", MCU_NAME);
    g_u8g2.drawStr(2, y, line); y += 9;

    snprintf(line, sizeof(line), "RTOS: FreeRTOS Aktif");
    g_u8g2.drawStr(2, y, line); y += 9;

    snprintf(line, sizeof(line), "Heap: %lu B",
             static_cast<unsigned long>(xPortGetFreeHeapSize()));
    g_u8g2.drawStr(2, y, line);

    display_drawStatusBar("Info Sistem", "BACK:Menu");
}

// =============================================================================
// DISPATCH TABLE — indeks sesuai nilai enum MenuState
// =============================================================================
typedef void (*DrawFn)();

static DrawFn s_drawTable[static_cast<uint8_t>(MenuState::COUNT)] = {
    drawSplash,             // SPLASH
    drawHome,               // HOME
    drawWaitingSampling,    // WAITING_SAMPLING
    drawMeasurement,        // MEASUREMENT
    drawCalibration,        // CALIBRATION
    drawCalibrationSub,     // CALIBRATION_TDS
    drawCalibrationSub,     // CALIBRATION_TURBIDITY
    drawCalibrationSub,     // CALIBRATION_TEMPERATURE
    drawSettings,           // SETTINGS
    drawAbout               // ABOUT
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
    s_splashStartTick   = millis();
    s_samplingStartTick = 0;
    // Set langsung tanpa mutex: pre-scheduler, tidak ada task lain.
    g_systemState.previousMenu = MenuState::SPLASH;
    g_systemState.currentMenu = MenuState::SPLASH;
    g_systemState.cursorIndex = 0;
    g_systemState.measurementSubPage = 0;
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
        case MenuState::SPLASH:
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
                        s_samplingStartTick = millis();
                        transitionToLocked(MenuState::WAITING_SAMPLING);
                        break;
                    case 1:
                        g_systemState.activeParameter = WaterParameter::PEMANDIAN_KOLAM;
                        s_samplingStartTick = millis();
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

        case MenuState::WAITING_SAMPLING:
            // Tombol BACK membatalkan tunggu pembacaan dan kembali ke Menu Utama
            if (isActivate && msg.id == ButtonID::BACK) {
                transitionToLocked(MenuState::HOME);
            }
            break;

        case MenuState::MEASUREMENT:
            // Navigasi intuitif: DOWN pindah ke Rekomendasi, UP kembali ke Dashboard
            if (isActivate && (msg.id == ButtonID::DOWN || msg.id == ButtonID::OK)) {
                if (g_systemState.measurementSubPage == 0) {
                    g_systemState.measurementSubPage = 1;
                    g_systemState.displayDirty = true;
                }
            } else if (isActivate && (msg.id == ButtonID::UP)) {
                if (g_systemState.measurementSubPage == 1) {
                    g_systemState.measurementSubPage = 0;
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
                    transitionToLocked(MenuState::CALIBRATION_TDS);
                } else if (g_systemState.cursorIndex == 1) {
                    transitionToLocked(MenuState::CALIBRATION_TURBIDITY);
                } else if (g_systemState.cursorIndex == 2) {
                    transitionToLocked(MenuState::CALIBRATION_TEMPERATURE);
                } else if (g_systemState.cursorIndex == 3) {
                    // Reset Kalibrasi ke Nilai Pabrik
                    storage_loadFactoryDefaults(g_calibParams);
                    storage_requestSave(g_calibParams);
                    g_systemState.calibSaving = true;
                    g_systemState.displayDirty = true;
                }
            } else if (isActivate && msg.id == ButtonID::BACK) {
                transitionToLocked(MenuState::HOME);
            }
            break;

        case MenuState::CALIBRATION_TDS:
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
                }
                transitionToLocked(MenuState::CALIBRATION);
            } else if (isActivate && msg.id == ButtonID::BACK) {
                transitionToLocked(MenuState::CALIBRATION);
            }
            break;

        case MenuState::CALIBRATION_TURBIDITY:
            if (isActivate && msg.id == ButtonID::OK) {
                float volt = g_sensorData.turbidityVoltage;
                if (volt > TURBIDITY_VCLEAR_MIN) {
                    g_calibParams.turbidityVClear = volt;
                    storage_requestSave(g_calibParams);
                    g_systemState.calibSaving = true;
                }
                transitionToLocked(MenuState::CALIBRATION);
            } else if (isActivate && msg.id == ButtonID::BACK) {
                transitionToLocked(MenuState::CALIBRATION);
            }
            break;

        case MenuState::CALIBRATION_TEMPERATURE:
            if (isRepeatable && (msg.id == ButtonID::LEFT ||
                                 msg.id == ButtonID::RIGHT)) {
                float delta = (msg.id == ButtonID::RIGHT)
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
                transitionToLocked(MenuState::CALIBRATION);
            } else if (isActivate && msg.id == ButtonID::BACK) {
                transitionToLocked(MenuState::CALIBRATION);
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
                    g_systemState.displayDirty = true;
                }
            }
            break;

        case MenuState::ABOUT:
            if (isActivate && msg.id == ButtonID::BACK) {
                transitionToLocked(g_systemState.previousMenu);
            }
            break;

        default:
            break;
    }

    xSemaphoreGive(g_dataMutex);
}

void gui_tick() {
    // 1. Transisi otomatis Splash Screen (2 detik)
    if (g_systemState.currentMenu == MenuState::SPLASH) {
        if ((millis() - s_splashStartTick) >= SPLASH_SCREEN_MS) {
            if (xSemaphoreTake(g_dataMutex, DATA_MUTEX_TIMEOUT) == pdTRUE) {
                transitionToLocked(MenuState::HOME);
                xSemaphoreGive(g_dataMutex);
            }
        }
    }
    // 2. Transisi otomatis Screen Tunggu Pembacaan Sensor (5 detik)
    else if (g_systemState.currentMenu == MenuState::WAITING_SAMPLING) {
        if ((millis() - s_samplingStartTick) >= SAMPLING_SCREEN_MS) {
            if (xSemaphoreTake(g_dataMutex, DATA_MUTEX_TIMEOUT) == pdTRUE) {
                transitionToLocked(MenuState::MEASUREMENT);
                xSemaphoreGive(g_dataMutex);
            }
        } else {
            // Tandai display dirty agar animasi progress bar berjalan mulus
            if (xSemaphoreTake(g_dataMutex, DATA_MUTEX_TIMEOUT) == pdTRUE) {
                g_systemState.displayDirty = true;
                xSemaphoreGive(g_dataMutex);
            }
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
