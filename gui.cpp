/**
 * @file    gui.cpp
 * @brief   Implementasi GUI Manager. Logika navigasi FSM dipusatkan pada
 *          gui_update() (satu switch atas MenuState, bukan if-else
 *          bertingkat), sedangkan tampilan tiap halaman diimplementasikan
 *          pada fungsi draw*() masing-masing dan didaftarkan ke dispatch
 *          table agar penambahan halaman baru tidak mengubah struktur inti.
 */

#include "gui.h"
#include "display.h"
#include "config.h"
#include <STM32FreeRTOS.h>

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
// UTILITAS INTERNAL
// =============================================================================

/**
 * @brief Berpindah ke halaman GUI baru secara thread-safe: menyimpan
 *        halaman sebelumnya, mereset kursor, dan menandai layar dirty.
 */
static void transitionTo(MenuState newState) {
    if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        g_systemState.previousMenu = g_systemState.currentMenu;
        g_systemState.currentMenu = newState;
        g_systemState.cursorIndex = 0;
        g_systemState.measurementSubPage = 0;
        g_systemState.settingsAdjustMode = false;
        g_systemState.displayDirty = true;
        xSemaphoreGive(g_dataMutex);
    }
}

/**
 * @brief Mengubah cursorIndex secara thread-safe (naik/turun melingkar).
 */
static void moveCursor(bool moveDown, uint8_t itemCount) {
    if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        if (moveDown) {
            g_systemState.cursorIndex = static_cast<uint8_t>((g_systemState.cursorIndex + 1) % itemCount);
        } else {
            g_systemState.cursorIndex = static_cast<uint8_t>(
                (g_systemState.cursorIndex == 0) ? (itemCount - 1) : (g_systemState.cursorIndex - 1));
        }
        g_systemState.displayDirty = true;
        xSemaphoreGive(g_dataMutex);
    }
}

// =============================================================================
// FUNGSI DRAW (TAMPILAN — TIDAK MENGUBAH STATE APAPUN)
// =============================================================================

/** @brief Menggambar satu daftar menu bergaya list dengan kursor panah. */
static void drawSimpleList(const char* title, const char* const* items,
                            uint8_t count, uint8_t cursor) {
    display_drawHeader(title);
    g_u8g2.setFont(u8g2_font_6x10_tf);

    constexpr uint8_t lineHeight = 11;
    constexpr uint8_t firstLineY = DISPLAY_HEADER_H + 9;

    for (uint8_t i = 0; i < count; i++) {
        uint8_t y = firstLineY + (i * lineHeight);
        if (y > DISPLAY_HEIGHT - DISPLAY_STATUSBAR_H) {
            break; // di luar area konten yang terlihat
        }
        if (i == cursor) {
            g_u8g2.drawStr(2, y, ">");
        }
        g_u8g2.drawStr(12, y, items[i]);
    }
}

/** @brief Menghitung koordinat X agar teks berada di tengah layar (aman dari nilai negatif). */
static uint8_t centeredX(const char* text) {
    uint8_t textWidth = g_u8g2.getStrWidth(text);
    if (textWidth >= DISPLAY_WIDTH) {
        return 0; // teks lebih lebar dari layar: rata kiri saja, tidak dipaksakan center
    }
    return static_cast<uint8_t>((DISPLAY_WIDTH - textWidth) / 2);
}

static void drawSplash() {
    // Judul dipecah manual jadi 2 baris agar tidak melebihi lebar layar 128px
    // (font bold satu baris untuk "Water Quality Analyzer" terlalu lebar).
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
    drawSimpleList("Home", HOME_ITEMS, HOME_ITEM_COUNT, g_systemState.cursorIndex);
    display_drawStatusBar("Water Quality Analyzer", nullptr);
}

static void drawParameter() {
    drawSimpleList("Parameter", PARAMETER_ITEMS, PARAMETER_ITEM_COUNT, g_systemState.cursorIndex);
    const char* activeName = PARAMETER_ITEMS[static_cast<uint8_t>(g_systemState.activeParameter)];
    display_drawStatusBar(activeName, nullptr);
}

/** @brief Mencetak satu baris nilai sensor dengan status error bila perlu. */
static void drawSensorLine(uint8_t y, const char* label, float value,
                            const char* unit, SensorStatus status) {
    g_u8g2.setFont(u8g2_font_6x10_tf);
    char line[32];
    if (status == SensorStatus::OK) {
        char valueStr[12];
        dtostrf(value, 4, 1, valueStr);
        snprintf(line, sizeof(line), "%s: %s %s", label, valueStr, unit);
    } else {
        snprintf(line, sizeof(line), "%s: ERROR", label);
    }
    g_u8g2.drawStr(4, y, line);
}

static void drawMeasurement() {
    g_u8g2.setFont(u8g2_font_6x10_tf);
    constexpr uint8_t lineHeight = 11;

    if (g_systemState.measurementSubPage == 0) {
        // --- HALAMAN 1: DATA SENSOR + SKOR FUZZY ---
        display_drawHeader("Pengukuran (1/2)");
        uint8_t y = DISPLAY_HEADER_H + 9;

        // Baris 1: Suhu + Status Suhu
        char tempBuf[32];
        snprintf(tempBuf, sizeof(tempBuf), "Suhu : %.1f C (%s)",
                 g_sensorData.temperature,
                 (g_sensorData.tempStatus == SUHU_NORMAL) ? "Normal" : "Abn");
        g_u8g2.drawStr(2, y, tempBuf);
        y += lineHeight;

        // Baris 2: TDS Kompensasi Suhu
        char tdsBuf[32];
        snprintf(tdsBuf, sizeof(tdsBuf), "TDS  : %.1f ppm", g_sensorData.tdsCompensated);
        g_u8g2.drawStr(2, y, tdsBuf);
        y += lineHeight;

        // Baris 3: Turbidity Filtered
        char turbBuf[32];
        snprintf(turbBuf, sizeof(turbBuf), "Turb : %.1f NTU", g_sensorData.turbidityFiltered);
        g_u8g2.drawStr(2, y, turbBuf);
        y += lineHeight;

        // Baris 4: Skor Fuzzy + Badge Status
        const char* badge = (g_sensorData.qualityStatus == STATUS_LAYAK) ? "LAYAK" :
                            (g_sensorData.qualityStatus == STATUS_LTM)   ? "LTM" : "TL";
        char skorBuf[32];
        snprintf(skorBuf, sizeof(skorBuf), "Skor : %.1f [%s]", g_sensorData.fuzzyScore, badge);
        g_u8g2.drawStr(2, y, skorBuf);

        display_drawStatusBar("Tekan OK untuk detail", nullptr);

    } else {
        // --- HALAMAN 2: DETAIL FUZZY & PESAN REKOMENDASI ---
        display_drawHeader("Detail Fuzzy (2/2)");
        uint8_t y = DISPLAY_HEADER_H + 9;

        // Status Kualitas Air
        const char* qStr = (g_sensorData.qualityStatus == STATUS_LAYAK) ? "LAYAK" :
                           (g_sensorData.qualityStatus == STATUS_LTM)   ? "LTM (Layak TM)" : "TL (Tidak Layak)";
        char line1[32];
        snprintf(line1, sizeof(line1), "Status: %s", qStr);
        g_u8g2.drawStr(2, y, line1);
        y += lineHeight;

        // Status Suhu Air
        const char* tStr = (g_sensorData.tempStatus == SUHU_NORMAL) ? "Normal" : "Abnormal";
        char line2[32];
        snprintf(line2, sizeof(line2), "Suhu  : %s", tStr);
        g_u8g2.drawStr(2, y, line2);
        y += lineHeight;

        // Teks Rekomendasi
        g_u8g2.drawStr(2, y, "Pesan :");
        y += lineHeight;
        const char* pesan = FuzzyKualitasAir_GetPesan(g_sensorData.qualityStatus);
        g_u8g2.drawStr(6, y, pesan);

        display_drawStatusBar("Tekan OK ke data sensor", nullptr);
    }
}

static void drawCalibration() {
    drawSimpleList("Kalibrasi", CALIBRATION_ITEMS, CALIBRATION_ITEM_COUNT, g_systemState.cursorIndex);
}

static uint16_t s_targetTdsRef = 707; // Default larutan acuan 707 ppm

static void drawCalibrationSub() {
    g_u8g2.setFont(u8g2_font_6x10_tf);
    constexpr uint8_t lineHeight = 11;
    uint8_t y = DISPLAY_HEADER_H + 9;

    char lineBuf[32];

    switch (g_systemState.currentMenu) {
        case MenuState::CALIBRATION_TDS: {
            display_drawHeader("Kalibrasi TDS");

            snprintf(lineBuf, sizeof(lineBuf), "ADC Raw : %u", g_sensorData.tdsRaw);
            g_u8g2.drawStr(2, y, lineBuf); y += lineHeight;

            snprintf(lineBuf, sizeof(lineBuf), "Target  : [ %u ppm ]", s_targetTdsRef);
            g_u8g2.drawStr(2, y, lineBuf); y += lineHeight;

            g_u8g2.drawStr(2, y, "UP/DN:Target OK:Simpan");
            display_drawStatusBar("Celupkan ke larutan 707ppm", nullptr);
            break;
        }

        case MenuState::CALIBRATION_TURBIDITY: {
            display_drawHeader("Kalibrasi Turbidity");

            float volt = (g_sensorData.turbidityRaw / static_cast<float>(ADC_MAX_VALUE)) * ADC_REFERENCE_VOLTAGE;
            snprintf(lineBuf, sizeof(lineBuf), "ADC Raw : %u (%.2fV)", g_sensorData.turbidityRaw, volt);
            g_u8g2.drawStr(2, y, lineBuf); y += lineHeight;

            snprintf(lineBuf, sizeof(lineBuf), "V_Clear : %.2f V", g_calibParams.turbidityVClear);
            g_u8g2.drawStr(2, y, lineBuf); y += lineHeight;

            g_u8g2.drawStr(2, y, "Tekan OK: Lock 0 NTU");
            display_drawStatusBar("Air Aquades (0 NTU)", nullptr);
            break;
        }

        case MenuState::CALIBRATION_TEMPERATURE: {
            display_drawHeader("Kalibrasi Suhu");

            snprintf(lineBuf, sizeof(lineBuf), "Suhu Raw: %.1f C", g_sensorData.temperature - g_calibParams.tempOffset);
            g_u8g2.drawStr(2, y, lineBuf); y += lineHeight;

            snprintf(lineBuf, sizeof(lineBuf), "Offset  : [ %+.1f C ]", g_calibParams.tempOffset);
            g_u8g2.drawStr(2, y, lineBuf); y += lineHeight;

            g_u8g2.drawStr(2, y, "LF/RT:Offset OK:Simpan");
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

    constexpr uint8_t lineHeight = 11;
    constexpr uint8_t firstLineY = DISPLAY_HEADER_H + 9;

    char valueBuf[8];
    for (uint8_t i = 0; i < SETTINGS_ITEM_COUNT; i++) {
        uint8_t y = firstLineY + (i * lineHeight);
        if (i == g_systemState.cursorIndex) {
            g_u8g2.drawStr(2, y, g_systemState.settingsAdjustMode ? "*" : ">");
        }
        g_u8g2.drawStr(12, y, SETTINGS_ITEMS[i]);

        if (i == SETTINGS_IDX_BRIGHTNESS) {
            snprintf(valueBuf, sizeof(valueBuf), "%3u", g_systemState.settingsBrightness);
            g_u8g2.drawStr(100, y, valueBuf);
        } else if (i == SETTINGS_IDX_CONTRAST) {
            snprintf(valueBuf, sizeof(valueBuf), "%3u", g_systemState.settingsContrast);
            g_u8g2.drawStr(100, y, valueBuf);
        }
    }
}

static void drawAbout() {
    display_drawHeader("Tentang");
    g_u8g2.setFont(u8g2_font_5x7_tf);

    char line[32];
    uint8_t y = DISPLAY_HEADER_H + 9;
    constexpr uint8_t lineHeight = 9;

    snprintf(line, sizeof(line), "Alat: %s", FIRMWARE_NAME);
    g_u8g2.drawStr(2, y, line); y += lineHeight;

    snprintf(line, sizeof(line), "FW  : v%s", FIRMWARE_VERSION);
    g_u8g2.drawStr(2, y, line); y += lineHeight;

    snprintf(line, sizeof(line), "HW  : %s", HARDWARE_VERSION);
    g_u8g2.drawStr(2, y, line); y += lineHeight;

    snprintf(line, sizeof(line), "MCU : %s", MCU_NAME);
    g_u8g2.drawStr(2, y, line); y += lineHeight;

    snprintf(line, sizeof(line), "RTOS: FreeRTOS Aktif");
    g_u8g2.drawStr(2, y, line); y += lineHeight;

    snprintf(line, sizeof(line), "Heap: %lu B", static_cast<unsigned long>(xPortGetFreeHeapSize()));
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
// API PUBLIK
// =============================================================================

void gui_init() {
    s_splashStartTick = millis();
    transitionTo(MenuState::SPLASH);
}

/**
 * @brief Memetakan pasangan MenuState + arah navigasi generik (UP/DOWN)
 *        ke perubahan cursorIndex, dan OK/BACK ke perpindahan halaman.
 *        Diimplementasikan sebagai satu fungsi terpusat (bukan banyak
 *        if-else per halaman) yang memanggil helper generik di atas.
 */
void gui_update(const ButtonEventMsg& msg) {
    const bool isActivate = (msg.event == ButtonEvent::PRESSED);
    const bool isRepeatable = (msg.event == ButtonEvent::PRESSED || msg.event == ButtonEvent::REPEAT);
    const MenuState state = g_systemState.currentMenu;

    switch (state) {
        case MenuState::SPLASH:
            // Tidak bereaksi terhadap tombol; hanya berbasis waktu (gui_tick).
            break;

        case MenuState::HOME:
            if (isRepeatable && msg.id == ButtonID::UP)   moveCursor(false, HOME_ITEM_COUNT);
            if (isRepeatable && msg.id == ButtonID::DOWN) moveCursor(true, HOME_ITEM_COUNT);
            if (isActivate && msg.id == ButtonID::OK) {
                switch (g_systemState.cursorIndex) {
                    case 0: transitionTo(MenuState::MEASUREMENT); break;
                    case 1: transitionTo(MenuState::PARAMETER);   break;
                    case 2: transitionTo(MenuState::CALIBRATION); break;
                    case 3: transitionTo(MenuState::SETTINGS);    break;
                    case 4: transitionTo(MenuState::ABOUT);       break;
                    default: break;
                }
            }
            break;

        case MenuState::PARAMETER:
            if (isRepeatable && msg.id == ButtonID::UP)   moveCursor(false, PARAMETER_ITEM_COUNT);
            if (isRepeatable && msg.id == ButtonID::DOWN) moveCursor(true, PARAMETER_ITEM_COUNT);
            if (isActivate && msg.id == ButtonID::OK) {
                if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
                    g_systemState.activeParameter = static_cast<WaterParameter>(g_systemState.cursorIndex);
                    xSemaphoreGive(g_dataMutex);
                }
                transitionTo(MenuState::HOME);
            }
            if (isActivate && msg.id == ButtonID::BACK) transitionTo(MenuState::HOME);
            break;

        case MenuState::MEASUREMENT:
            if (isActivate && msg.id == ButtonID::OK) {
                if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
                    g_systemState.measurementSubPage = (g_systemState.measurementSubPage == 0) ? 1 : 0;
                    g_systemState.displayDirty = true;
                    xSemaphoreGive(g_dataMutex);
                }
            }
            if (isActivate && msg.id == ButtonID::BACK) transitionTo(MenuState::HOME);
            break;

        case MenuState::CALIBRATION:
            if (isRepeatable && msg.id == ButtonID::UP)   moveCursor(false, CALIBRATION_ITEM_COUNT);
            if (isRepeatable && msg.id == ButtonID::DOWN) moveCursor(true, CALIBRATION_ITEM_COUNT);
            if (isActivate && msg.id == ButtonID::OK) {
                static const MenuState targets[CALIBRATION_ITEM_COUNT] = {
                    MenuState::CALIBRATION_TDS, MenuState::CALIBRATION_TURBIDITY, MenuState::CALIBRATION_TEMPERATURE
                };
                transitionTo(targets[g_systemState.cursorIndex]);
            }
            if (isActivate && msg.id == ButtonID::BACK) transitionTo(MenuState::HOME);
            break;

        case MenuState::CALIBRATION_TDS:
            if (isRepeatable && msg.id == ButtonID::UP) {
                s_targetTdsRef += 5;
                g_systemState.displayDirty = true;
            }
            if (isRepeatable && msg.id == ButtonID::DOWN) {
                if (s_targetTdsRef >= 5) s_targetTdsRef -= 5;
                g_systemState.displayDirty = true;
            }
            if (isActivate && msg.id == ButtonID::OK) {
                // Hitung K-Factor baru
                float rawUncalibratedPpm = (g_calibParams.tdsKFactor > 0.001f)
                    ? (g_sensorData.tdsFiltered / g_calibParams.tdsKFactor)
                    : g_sensorData.tdsFiltered;
                if (rawUncalibratedPpm > 10.0f) {
                    g_calibParams.tdsKFactor = static_cast<float>(s_targetTdsRef) / rawUncalibratedPpm;
                    storage_saveCalibration();
                }
                transitionTo(MenuState::CALIBRATION);
            }
            if (isActivate && msg.id == ButtonID::BACK) transitionTo(MenuState::CALIBRATION);
            break;

        case MenuState::CALIBRATION_TURBIDITY:
            if (isActivate && msg.id == ButtonID::OK) {
                // Lock tegangan air murni 0 NTU
                float volt = (g_sensorData.turbidityRaw / static_cast<float>(ADC_MAX_VALUE)) * ADC_REFERENCE_VOLTAGE;
                if (volt > 0.5f) {
                    g_calibParams.turbidityVClear = volt;
                    storage_saveCalibration();
                }
                transitionTo(MenuState::CALIBRATION);
            }
            if (isActivate && msg.id == ButtonID::BACK) transitionTo(MenuState::CALIBRATION);
            break;

        case MenuState::CALIBRATION_TEMPERATURE:
            if (isRepeatable && (msg.id == ButtonID::LEFT || msg.id == ButtonID::RIGHT)) {
                float delta = (msg.id == ButtonID::RIGHT) ? 0.1f : -0.1f;
                g_calibParams.tempOffset += delta;
                g_systemState.displayDirty = true;
            }
            if (isActivate && msg.id == ButtonID::OK) {
                storage_saveCalibration();
                transitionTo(MenuState::CALIBRATION);
            }
            if (isActivate && msg.id == ButtonID::BACK) transitionTo(MenuState::CALIBRATION);
            break;

        case MenuState::SETTINGS:
            if (!g_systemState.settingsAdjustMode) {
                if (isRepeatable && msg.id == ButtonID::UP)   moveCursor(false, SETTINGS_ITEM_COUNT);
                if (isRepeatable && msg.id == ButtonID::DOWN) moveCursor(true, SETTINGS_ITEM_COUNT);
                if (isActivate && msg.id == ButtonID::OK) {
                    if (g_systemState.cursorIndex == SETTINGS_IDX_BRIGHTNESS ||
                        g_systemState.cursorIndex == SETTINGS_IDX_CONTRAST) {
                        if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
                            g_systemState.settingsAdjustMode = true;
                            g_systemState.displayDirty = true;
                            xSemaphoreGive(g_dataMutex);
                        }
                    } else if (g_systemState.cursorIndex == SETTINGS_IDX_RESET) {
                        if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
                            g_systemState.settingsBrightness = DISPLAY_DEFAULT_BRIGHTNESS;
                            g_systemState.settingsContrast = DISPLAY_DEFAULT_CONTRAST;
                            g_systemState.displayDirty = true;
                            xSemaphoreGive(g_dataMutex);
                        }
                        display_setBrightness(DISPLAY_DEFAULT_BRIGHTNESS);
                        display_setContrast(DISPLAY_DEFAULT_CONTRAST);
                    } else if (g_systemState.cursorIndex == SETTINGS_IDX_INFO) {
                        transitionTo(MenuState::ABOUT);
                    }
                }
                if (isActivate && msg.id == ButtonID::BACK) transitionTo(MenuState::HOME);
            } else {
                // Mode penyesuaian nilai: LEFT/RIGHT mengubah nilai (mendukung tahan/repeat).
                if (isRepeatable && (msg.id == ButtonID::LEFT || msg.id == ButtonID::RIGHT)) {
                    const int8_t delta = (msg.id == ButtonID::RIGHT) ? DISPLAY_LEVEL_STEP : -DISPLAY_LEVEL_STEP;
                    if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
                        uint8_t* target = (g_systemState.cursorIndex == SETTINGS_IDX_BRIGHTNESS)
                                               ? &g_systemState.settingsBrightness
                                               : &g_systemState.settingsContrast;
                        int16_t newValue = static_cast<int16_t>(*target) + delta;
                        newValue = constrain(newValue, DISPLAY_MIN_LEVEL, DISPLAY_MAX_LEVEL);
                        *target = static_cast<uint8_t>(newValue);
                        g_systemState.displayDirty = true;
                        xSemaphoreGive(g_dataMutex);
                    }
                    if (g_systemState.cursorIndex == SETTINGS_IDX_BRIGHTNESS) {
                        display_setBrightness(g_systemState.settingsBrightness);
                    } else {
                        display_setContrast(g_systemState.settingsContrast);
                    }
                }
                if (isActivate && (msg.id == ButtonID::OK || msg.id == ButtonID::BACK)) {
                    if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
                        g_systemState.settingsAdjustMode = false;
                        g_systemState.displayDirty = true;
                        xSemaphoreGive(g_dataMutex);
                    }
                }
            }
            break;

        case MenuState::ABOUT:
            if (isActivate && msg.id == ButtonID::BACK) transitionTo(g_systemState.previousMenu);
            break;

        default:
            break;
    }
}

void gui_tick() {
    if (g_systemState.currentMenu == MenuState::SPLASH) {
        if ((millis() - s_splashStartTick) >= SPLASH_SCREEN_MS) {
            transitionTo(MenuState::HOME);
        }
    }
}

void gui_draw() {
    uint8_t stateIndex = static_cast<uint8_t>(g_systemState.currentMenu);
    g_u8g2.clearBuffer();
    s_drawTable[stateIndex]();
    g_u8g2.sendBuffer();
}