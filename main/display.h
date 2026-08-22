/**
 * @file    display.h
 * @brief   Display Manager — mengelola inisialisasi hardware OLED 1.3"
 *          (controller SH1106, I2C), pengaturan brightness/contrast, dan
 *          primitif tampilan umum (header, status bar) yang dipakai oleh
 *          seluruh halaman GUI.
 * @details Modul ini TIDAK berisi logika navigasi ataupun keputusan
 *          halaman mana yang aktif. Itu adalah tanggung jawab gui.cpp.
 *
 *          Controller dipilih lewat makro OLED_USE_SH1106 di config.h:
 *            1 = SH1106  (OLED 1.3", default)
 *            0 = SSD1306 (OLED 0.96")
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "config.h"

// Alias tipe agar seluruh kode tidak perlu tahu controller mana yang dipakai.
#if OLED_USE_SH1106
  typedef U8G2_SH1106_128X64_NONAME_F_HW_I2C OledDriver_t;
#else
  typedef U8G2_SSD1306_128X64_NONAME_F_HW_I2C OledDriver_t;
#endif

// Instance U8g2 global (full frame buffer, hardware I2C).
// Dipakai langsung oleh gui.cpp untuk menggambar konten tiap halaman.
extern OledDriver_t g_u8g2;

/**
 * @brief Inisialisasi bus I2C pada pin PB8(SCL)/PB9(SDA) dan OLED SSD1306.
 */
void display_init();

/**
 * @brief Mengatur tingkat kontras OLED.
 * @param level Nilai 0-255.
 */
void display_setContrast(uint8_t level);

/**
 * @brief Mengatur tingkat brightness (mapping sederhana ke contrast
 *        karena SSD1306 tidak memiliki register brightness terpisah).
 * @param level Nilai 0-255.
 */
void display_setBrightness(uint8_t level);

/**
 * @brief Menggambar area header (judul halaman) pada bagian atas layar.
 * @param title Teks judul yang ditampilkan.
 */
void display_drawHeader(const char* title);

/**
 * @brief Menggambar status bar pada bagian bawah layar berisi indikator
 *        status sistem singkat (mis. parameter aktif atau status error).
 * @param leftText  Teks rata kiri.
 * @param rightText Teks rata kanan (boleh nullptr jika tidak dipakai).
 */
void display_drawStatusBar(const char* leftText, const char* rightText);

#endif // DISPLAY_H
