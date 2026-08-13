/**
 * @file    display.cpp
 * @brief   Implementasi Display Manager untuk OLED SSD1306 128x64 via I2C
 *          pada pin custom PB8(SCL)/PB9(SDA), menggunakan library U8g2.
 */

#include "display.h"
#include "config.h"
#include <Wire.h>

// Full buffer mode (_F_) dipilih agar penggambaran header + konten +
// status bar dapat digabung dalam satu sendBuffer() tanpa kedip,
// mengorbankan ~1 KB RAM yang masih sangat aman untuk STM32F401 (64 KB SRAM).
U8G2_SSD1306_128X64_NONAME_F_HW_I2C g_u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

/**
 * @brief Inisialisasi bus I2C dengan remap pin custom lalu OLED.
 */
void display_init() {
    Wire.setSCL(PIN_OLED_SCL);
    Wire.setSDA(PIN_OLED_SDA);
    Wire.begin();
    Wire.setClock(400000);  // SSD1306 mendukung fast-mode I2C (400 kHz)

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
 * @brief Mengatur brightness. SSD1306 tidak memiliki kontrol brightness
 *        terpisah dari contrast, sehingga nilai brightness dipetakan
 *        langsung ke register contrast agar perilaku tetap intuitif
 *        bagi pengguna di menu Pengaturan.
 */
void display_setBrightness(uint8_t level) {
    g_u8g2.setContrast(level);
}

/**
 * @brief Menggambar header berisi judul halaman dengan garis pemisah.
 */
void display_drawHeader(const char* title) {
    g_u8g2.setFont(u8g2_font_6x10_tf);
    g_u8g2.drawStr(2, 9, title);
    g_u8g2.drawHLine(0, DISPLAY_HEADER_H - 1, DISPLAY_WIDTH);
}

/**
 * @brief Menggambar status bar bawah dengan teks kiri dan kanan (opsional).
 */
void display_drawStatusBar(const char* leftText, const char* rightText) {
    const uint8_t barY = DISPLAY_HEIGHT - DISPLAY_STATUSBAR_H;
    g_u8g2.drawHLine(0, barY, DISPLAY_WIDTH);

    g_u8g2.setFont(u8g2_font_5x7_tf);
    g_u8g2.drawStr(2, DISPLAY_HEIGHT - 1, leftText);

    if (rightText != nullptr) {
        uint8_t textWidth = g_u8g2.getStrWidth(rightText);
        g_u8g2.drawStr(DISPLAY_WIDTH - textWidth - 2, DISPLAY_HEIGHT - 1, rightText);
    }
}
