/**
 * @file    gui.h
 * @brief   GUI Manager — mengimplementasikan Finite State Machine (FSM)
 *          untuk navigasi menu OLED.
 * @details Prinsip pemisahan tanggung jawab yang ketat:
 *            - gui_update() HANYA mengubah state (dipanggil Task GUI).
 *            - gui_draw()   HANYA menggambar (dipanggil Task OLED).
 *
 *          Aturan penguncian data: gui_draw() mengambil SATU snapshot
 *          SensorData + SystemState di bawah g_dataMutex, lalu menggambar
 *          sepenuhnya dari salinan lokal tersebut. Ini mencegah torn read
 *          (nilai float 4 byte terbaca separuh saat Task Water Sensor
 *          sedang menulisnya) dan menjamin seluruh elemen pada satu frame
 *          berasal dari titik waktu yang sama.
 *
 *          Menambah halaman baru cukup: tambah nilai enum MenuState di
 *          globals.h, buat satu cabang pada gui_update() dan satu fungsi
 *          draw*(), lalu daftarkan pada dispatch table di gui.cpp.
 */

#ifndef GUI_H
#define GUI_H

#include "globals.h"

/**
 * @brief Inisialisasi state awal GUI (masuk ke halaman Splash).
 */
void gui_init();

/**
 * @brief Memproses satu event tombol sesuai halaman GUI yang sedang
 *        aktif dan memperbarui SystemState (state transition FSM).
 *        Fungsi ini TIDAK melakukan operasi gambar maupun penulisan flash.
 * @param msg Event tombol yang diterima dari g_buttonEventQueue.
 */
void gui_update(const ButtonEventMsg& msg);

/**
 * @brief Memproses transisi otomatis yang tidak bergantung tombol,
 *        misalnya durasi tampil Splash Screen. Dipanggil periodik
 *        oleh Task GUI di setiap siklusnya.
 */
void gui_tick();

/**
 * @brief Menggambar ulang seluruh konten OLED sesuai MenuState yang
 *        sedang aktif. Hanya dipanggil oleh Task OLED saat displayDirty
 *        bernilai true.
 */
void gui_draw();

#endif // GUI_H
