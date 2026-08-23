/**
 * @file    display.cpp
 * @brief   Implementasi Display Manager untuk OLED 1.3" 128x64 via I2C pada
 *          pin custom PB8(SCL)/PB9(SDA), menggunakan library U8g2.
 * @details Controller default SH1106 (OLED 1.3"). Ukuran font header dan
 *          status bar dinaikkan satu tingkat dibanding versi 0.96" agar
 *          proporsi teks pada panel yang lebih besar tetap enak dibaca.
 */

#include "display.h"
#include "config.h"
#include <Wire.h>

// Full buffer mode (_F_) dipilih agar penggambaran header + konten +
// status bar dapat digabung dalam satu sendBuffer() tanpa kedip,
// mengorbankan ~1 KB RAM yang masih sangat aman untuk STM32F401 (64 KB SRAM).
OledDriver_t g_u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

/**
 * @brief Inisialisasi bus I2C dengan remap pin custom lalu OLED.
 */
void display_init() {
    Wire.setSCL(PIN_OLED_SCL);
    Wire.setSDA(PIN_OLED_SDA);
    Wire.begin();
    Wire.setClock(400000);  // SH1106/SSD1306 mendukung fast-mode I2C (400 kHz)

    g_u8g2.begin();
    g_u8g2.setContrast(DISPLAY_DEFAULT_CONTRAST);
    g_u8g2.setFontMode(1);
    g_u8g2.setBitmapMode(1);
}

/**
 * @brief Mengatur kontras OLED secara langsung.
 */
void display_setContrast(uint8_t level) {
    g_u8g2.setContrast(level);
}

/**
 * @brief Mengatur brightness. SH1106/SSD1306 tidak memiliki kontrol
 *        brightness terpisah dari contrast, sehingga nilai brightness
 *        dipetakan langsung ke register contrast agar perilaku tetap
 *        intuitif bagi pengguna di menu Pengaturan.
 */
void display_setBrightness(uint8_t level) {
    g_u8g2.setContrast(level);
}

/**
 * @brief Menggambar header berisi judul halaman dengan garis pemisah.
 *        Font 6x10 dipilih agar proporsional, rapi, dan tidak kebesaran.
 */
void display_drawHeader(const char* title) {
    g_u8g2.setFont(u8g2_font_6x10_tf);
    g_u8g2.drawStr(2, 9, title);
    g_u8g2.drawHLine(0, DISPLAY_HEADER_H - 1, DISPLAY_WIDTH);
}

/**
 * @brief Menggambar status bar bawah dengan teks kiri dan teks kanan
 *        (opsional). Menggunakan font 5x7 yang ringkas.
 */
void display_drawStatusBar(const char* leftText, const char* rightText) {
    const uint8_t barY = DISPLAY_HEIGHT - DISPLAY_STATUSBAR_H;
    g_u8g2.drawHLine(0, barY, DISPLAY_WIDTH);

    g_u8g2.setFont(u8g2_font_5x7_tf);
    const uint8_t textY = DISPLAY_HEIGHT - 1;

    if (rightText != nullptr && rightText[0] != '\0') {
        uint8_t rightWidth = g_u8g2.getStrWidth(rightText);
        uint8_t rightX = (DISPLAY_WIDTH > (rightWidth + 2))
                             ? (DISPLAY_WIDTH - rightWidth - 2) : 0;
        g_u8g2.drawStr(rightX, textY, rightText);

        if (leftText != nullptr) {
            uint8_t maxLeftWidth = (rightX > 6) ? (rightX - 6) : 0;
            char buf[32];
            strncpy(buf, leftText, sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            while (strlen(buf) > 0 && g_u8g2.getStrWidth(buf) > maxLeftWidth) {
                buf[strlen(buf) - 1] = '\0';
            }
            g_u8g2.drawStr(2, textY, buf);
        }
    } else if (leftText != nullptr) {
        g_u8g2.drawStr(2, textY, leftText);
    }
}
