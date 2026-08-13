/**
 * @file    gui.cpp
 * @brief   Implementasi GUI Manager. Pola snapshot: gui_draw() mengambil
 *          satu salinan SensorData + SystemState + CalibrationParams di
 *          bawah g_dataMutex, lalu seluruh fungsi draw*() membaca dari
 *          salinan tersebut. Ini mencegah torn-read pada nilai float yang
 *          sedang ditulis task sensor, dan menjamin seluruh elemen pada
 *          satu frame OLED berasal dari titik waktu yang sama.
 *
 *          Seluruh format float memakai dtostrf() karena STM32duino
 *          default-link dengan newlib nano tanpa -u _printf_float, sehingga
 *          snprintf("%.1f", ...) TIDAK mencetak angka di hardware.
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
    "Mulai Pengukuran", "Parameter", "Kalibrasi", "Pengaturan", "Tentang"
};
static constexpr uint8_t HOME_ITEM_COUNT = 5;

static const char* const PARAMETER_ITEMS[] = {
    "Higiene Sanitasi", "Air SPA", "Air Kolam Renang", "Pemandian Umum"
};
static constexpr uint8_t PARAMETER_ITEM_COUNT = 4;

static const char* const CALIBRATION_ITEMS[] = {
    "Kalibrasi TDS", "Kalibrasi Turbidity", "Kalibrasi Suhu"
};
static constexpr uint8_t CALIBRATION_ITEM_COUNT = 3;

static const char* const SETTINGS_ITEMS[] = {
    "Brightness", "Kontras", "Reset Pengaturan", "Informasi Firmware"
};
static constexpr uint8_t SETTINGS_ITEM_COUNT = 4;

static constexpr uint8_t SETTINGS_IDX_BRIGHTNESS = 0;
static constexpr uint8_t SETTINGS_IDX_CONTRAST   = 1;
static constexpr uint8_t SETTINGS_IDX_RESET      = 2;
static constexpr uint8_t SETTINGS_IDX_INFO       = 3;

// Waktu (ms) sejak boot; dipakai untuk mengukur durasi tampil Splash Screen.
static uint32_t s_splashStartTick = 0;

// =============================================================================
// SNAPSHOT — salinan lokal data global yang dibaca semua draw*().
// =============================================================================
static SensorData         s_view;
static SystemState        s_viewState;
static CalibrationParams  s_viewCalib;

// =============================================================================
// HELPERS BER-MUTEX (dipanggil dari gui_update, mutex sudah dipegang)
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

static void drawSplash() {
    static const char* const titleLine1 = "Water Quality";
    static const char* const titleLine2 = "Analyzer";

    g_u8g2.setFont(u8g2_font_7x14B_tf);
    g_u8g2.drawStr(centeredX(titleLine1), 26, titleLine1);
    g_u8g2.drawStr(centeredX(titleLine2), 42, titleLine2);

    g_u8g2.setFont(u8g2_font_6x10_tf);
    char versionLine[24];
    snprintf(versionLine, sizeof(versionLine), "v%s", FIRMWARE_VERSION);
    g_u8g2.drawStr(centeredX(versionLine), 56, versionLine);
}

static void drawHome() {
    drawSimpleList("Home", HOME_ITEMS, HOME_ITEM_COUNT, s_viewState.cursorIndex);
    display_drawStatusBar("Water Quality Analyzer", nullptr);
}

static void drawParameter() {
    drawSimpleList("Parameter", PARAMETER_ITEMS, PARAMETER_ITEM_COUNT,
                   s_viewState.cursorIndex);
    const char* activeName = PARAMETER_ITEMS[
        static_cast<uint8_t>(s_viewState.activeParameter)];
    display_drawStatusBar(activeName, nullptr);
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

static void drawMeasurement() {
    if (s_viewState.measurementSubPage == 0) {
        // --- HALAMAN 1: DATA SENSOR + SKOR FUZZY ---
        display_drawHeader("Pengukuran (1/2)");
        uint8_t y = MENU_FIRST_LINE_Y;

        // Suhu + status suhu
        char tempBuf[32];
        if (s_view.temperatureStatus == SensorStatus::OK) {
            char tStr[8];
            dtostrf(s_view.temperature, 4, 1, tStr);
            char* p = tStr;
            while (*p == ' ') p++;
            snprintf(tempBuf, sizeof(tempBuf), "Suhu : %s C (%s)", p,
                     (s_view.tempStatus == SUHU_NORMAL) ? "Normal" : "Abn");
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

        // Skor + badge
        const char* badge = (s_view.qualityStatus == STATUS_LAYAK) ? "LAYAK" :
                            (s_view.qualityStatus == STATUS_LTM)   ? "LTM" : "TL";
        char scoreStr[8];
        char skorBuf[32];
        dtostrf(s_view.fuzzyScore, 4, 1, scoreStr);
        char* p = scoreStr;
        while (*p == ' ') p++;
        snprintf(skorBuf, sizeof(skorBuf), "Skor : %s [%s]", p, badge);
        g_u8g2.drawStr(2, y, skorBuf);

        display_drawStatusBar("Tekan OK untuk detail", nullptr);

    } else {
        // --- HALAMAN 2: DETAIL FUZZY & PESAN REKOMENDASI ---
        display_drawHeader("Detail Fuzzy (2/2)");
        uint8_t y = MENU_FIRST_LINE_Y;
        g_u8g2.setFont(u8g2_font_6x10_tf);

        const char* qStr = (s_view.qualityStatus == STATUS_LAYAK) ? "LAYAK" :
                           (s_view.qualityStatus == STATUS_LTM)   ? "LTM (Layak TM)"
                                                                   : "TL (Tdk Layak)";
        char lineBuf[32];
        snprintf(lineBuf, sizeof(lineBuf), "Status: %s", qStr);
        g_u8g2.drawStr(2, y, lineBuf);
        y += MENU_LINE_HEIGHT;

        const char* tStr = (s_view.tempStatus == SUHU_NORMAL) ? "Normal" : "Abnormal";
        snprintf(lineBuf, sizeof(lineBuf), "Suhu  : %s", tStr);
        g_u8g2.drawStr(2, y, lineBuf);
        y += MENU_LINE_HEIGHT;

        g_u8g2.drawStr(2, y, "Pesan :");
        y += MENU_LINE_HEIGHT;
        const char* pesan = FuzzyKualitasAir_GetPesan(s_view.qualityStatus);
        drawWrappedText(4, y, pesan);

        display_drawStatusBar("Tekan OK ke data sensor", nullptr);
    }
}

static void drawCalibration() {
    drawSimpleList("Kalibrasi", CALIBRATION_ITEMS, CALIBRATION_ITEM_COUNT,
                   s_viewState.cursorIndex);
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
            display_drawStatusBar("Celupkan ke larutan standar", nullptr);
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
            display_drawStatusBar("Air Aquades (0 NTU)", nullptr);
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
            // Tambahkan tanda '+' manual bila non-negatif
            char sign = (s_viewCalib.tempOffset >= 0.0f) ? '+' : ' ';
            snprintf(lineBuf, sizeof(lineBuf), "Offset  : [ %c%s C ]", sign, p);
            g_u8g2.drawStr(2, y, lineBuf); y += MENU_LINE_HEIGHT;

            if (s_viewState.calibSaving) {
                g_u8g2.drawStr(2, y, "Menyimpan...");
            } else {
                g_u8g2.drawStr(2, y, "LF/RT:Offset OK:Simpan");
            }
            display_drawStatusBar("Samakan dgn termometer", nullptr);
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
}

static void drawAbout() {
    display_drawHeader("Tentang");
    g_u8g2.setFont(u8g2_font_5x7_tf);

    char line[48];
    uint8_t y = MENU_FIRST_LINE_Y;

    snprintf(line, sizeof(line), "Alat: %s", FIRMWARE_NAME);
    g_u8g2.drawStr(2, y, line); y += 9;

    snprintf(line, sizeof(line), "FW  : v%s", FIRMWARE_VERSION);
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
}

// =============================================================================
// DISPATCH TABLE — indeks sesuai nilai enum MenuState
// =============================================================================
typedef void (*DrawFn)();

static DrawFn s_drawTable[static_cast<uint8_t>(MenuState::COUNT)] = {
    drawSplash,             // SPLASH
    drawHome,               // HOME
    drawParameter,          // PARAMETER
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
    s_splashStartTick = millis();
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
                    case 0: transitionToLocked(MenuState::MEASUREMENT); break;
                    case 1: transitionToLocked(MenuState::PARAMETER);   break;
                    case 2: transitionToLocked(MenuState::CALIBRATION); break;
                    case 3: transitionToLocked(MenuState::SETTINGS);    break;
                    case 4: transitionToLocked(MenuState::ABOUT);       break;
                    default: break;
                }
            }
            break;

        case MenuState::PARAMETER:
            if (isRepeatable && msg.id == ButtonID::UP)
                moveCursorLocked(false, PARAMETER_ITEM_COUNT);
            else if (isRepeatable && msg.id == ButtonID::DOWN)
                moveCursorLocked(true, PARAMETER_ITEM_COUNT);
            else if (isActivate && msg.id == ButtonID::OK) {
                g_systemState.activeParameter = static_cast<WaterParameter>(
                    g_systemState.cursorIndex);
                transitionToLocked(MenuState::HOME);
            } else if (isActivate && msg.id == ButtonID::BACK)
                transitionToLocked(MenuState::HOME);
            break;

        case MenuState::MEASUREMENT:
            if (isActivate && msg.id == ButtonID::OK) {
                g_systemState.measurementSubPage =
                    static_cast<uint8_t>((g_systemState.measurementSubPage == 0) ? 1 : 0);
                g_systemState.displayDirty = true;
            } else if (isActivate && msg.id == ButtonID::BACK)
                transitionToLocked(MenuState::HOME);
            break;

        case MenuState::CALIBRATION:
            if (isRepeatable && msg.id == ButtonID::UP)
                moveCursorLocked(false, CALIBRATION_ITEM_COUNT);
            else if (isRepeatable && msg.id == ButtonID::DOWN)
                moveCursorLocked(true, CALIBRATION_ITEM_COUNT);
            else if (isActivate && msg.id == ButtonID::OK) {
                static const MenuState targets[CALIBRATION_ITEM_COUNT] = {
                    MenuState::CALIBRATION_TDS,
                    MenuState::CALIBRATION_TURBIDITY,
                    MenuState::CALIBRATION_TEMPERATURE
                };
                transitionToLocked(targets[g_systemState.cursorIndex]);
            } else if (isActivate && msg.id == ButtonID::BACK)
                transitionToLocked(MenuState::HOME);
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
            } else if (isActivate && msg.id == ButtonID::BACK)
                transitionToLocked(MenuState::CALIBRATION);
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
            } else if (isActivate && msg.id == ButtonID::BACK)
                transitionToLocked(MenuState::CALIBRATION);
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
            } else if (isActivate && msg.id == ButtonID::BACK)
                transitionToLocked(MenuState::CALIBRATION);
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
                } else if (isActivate && msg.id == ButtonID::BACK)
                    transitionToLocked(MenuState::HOME);
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
            if (isActivate && msg.id == ButtonID::BACK)
                transitionToLocked(g_systemState.previousMenu);
            break;

        default:
            break;
    }

    xSemaphoreGive(g_dataMutex);
}

void gui_tick() {
    if (g_systemState.currentMenu == MenuState::SPLASH) {
        if ((millis() - s_splashStartTick) >= SPLASH_SCREEN_MS) {
            if (xSemaphoreTake(g_dataMutex, DATA_MUTEX_TIMEOUT) == pdTRUE) {
                transitionToLocked(MenuState::HOME);
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
