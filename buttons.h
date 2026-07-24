/**
 * @file    buttons.h
 * @brief   Driver tombol dengan debounce, deteksi hold, dan repeat.
 * @details Modul ini adalah satu-satunya pemilik state debounce internal.
 *          Task lain (GUI) TIDAK membaca GPIO secara langsung, melainkan
 *          hanya menerima event lewat g_buttonEventQueue. Field
 *          g_buttonStates[] disediakan hanya untuk kebutuhan tampilan
 *          debug (Task Serial Debug).
 */

#ifndef BUTTONS_H
#define BUTTONS_H

#include <Arduino.h>

/**
 * @brief Inisialisasi GPIO seluruh tombol sebagai INPUT_PULLUP
 *        (active LOW, satu kaki ke GND).
 */
void buttons_init();

/**
 * @brief Melakukan satu siklus scan debounce untuk seluruh tombol.
 *        Jika terjadi perubahan state (pressed/released/hold/repeat),
 *        event dikirim ke g_buttonEventQueue dan g_buttonStates[]
 *        diperbarui. Dipanggil periodik oleh Task Button.
 */
void buttons_update();

#endif // BUTTONS_H
