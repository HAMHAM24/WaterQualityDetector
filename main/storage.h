/**
 * @file    storage.h
 * @brief   Modul Storage EEPROM — Mengelola penyimpanan permanen kalibrasi
 *          sensor dan pengaturan tampilan OLED.
 *          pada memori Flash non-volatile STM32.
 * @details PENTING soal keamanan waktu (timing): STM32F401 tidak memiliki
 *          EEPROM sejati. Library EEPROM STM32duino mengemulasikannya dengan
 *          cara MENGHAPUS SATU SEKTOR FLASH lalu menulis ulang seluruh
 *          isinya. Selama proses tersebut bus flash terhenti dan interupsi
 *          dinonaktifkan hingga ratusan milidetik — cukup lama untuk merusak
 *          timing bit-banging OneWire DS18B20 dan transaksi I2C OLED yang
 *          sedang berjalan.
 *
 *          Karena itu penyimpanan TIDAK BOLEH dipanggil langsung dari dalam
 *          task GUI saat tombol ditekan. Alurnya:
 *            1. Task GUI memanggil storage_requestSave() dengan nilai yang
 *               baru diubah. Fungsi ini hanya menyalin 16 byte ke buffer
 *               tertunda, jadi sangat cepat dan aman dari task mana pun.
 *            2. Task Water Sensor memanggil storage_processPendingSave() di
 *               awal siklusnya — titik ketika tidak ada konversi DS18B20
 *               maupun penggambaran OLED yang berlangsung.
 */

#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>

/**
 * @struct CalibrationParams
 * @brief  Struktur data parameter kalibrasi sensor yang disimpan di EEPROM.
 */
struct CalibrationParams {
    uint32_t magicHeader;      // Marker untuk memvalidasi format data EEPROM
    float    tdsKFactor;       // Faktor pengali kalibrasi TDS
    float    turbidityVClear;  // Tegangan sensor saat air murni/jernih 0 NTU
    float    tempOffset;       // Offset koreksi suhu °C
    float    turbidityVStandard;   // Tegangan titik standar turbidity (> 0 NTU)
    float    turbidityNtuStandard; // Nilai custom titik standar (1-3000 NTU)
    uint8_t  displayBrightness;    // Pengaturan brightness OLED
    uint8_t  displayContrast;      // Pengaturan contrast OLED
};

/**
 * Salinan aktif parameter kalibrasi yang dipakai saat menghitung nilai
 * sensor. Penulis tunggalnya adalah task GUI (di bawah g_dataMutex);
 * modul sensor hanya membaca satu field float sekaligus, yang bersifat
 * atomik pada ARM Cortex-M4 sehingga aman tanpa penguncian tambahan.
 */
extern CalibrationParams g_calibParams;

/**
 * @brief Inisialisasi memori EEPROM dan membaca data kalibrasi. Jika EEPROM
 *        belum pernah diinisialisasi (magicHeader salah) atau isinya di luar
 *        batas wajar, otomatis menggunakan nilai standar pabrik.
 *        Dipanggil dari setup(), sebelum scheduler berjalan.
 */
void storage_init();

/**
 * @brief Menyalin parameter ke buffer tertunda dan menandainya untuk
 *        disimpan ke flash. Tidak melakukan operasi flash apa pun,
 *        sehingga aman dipanggil saat merespons penekanan tombol.
 * @param params Nilai kalibrasi yang ingin disimpan.
 */
void storage_requestSave(const CalibrationParams& params);

/**
 * @brief Mengeksekusi penyimpanan yang tertunda, bila ada. Penulisan
 *        dilewati apabila isi flash sudah identik, sehingga umur flash
 *        tidak terbuang sia-sia.
 * @return true jika penulisan flash benar-benar dilakukan pada pemanggilan ini.
 */
bool storage_processPendingSave();

/**
 * @brief Mengisi struct dengan nilai standar pabrik.
 */
void storage_loadFactoryDefaults(CalibrationParams& params);

/**
 * @brief Menjepit seluruh field ke rentang aman agar data flash yang rusak
 *        tidak membuat pembacaan sensor menjadi tidak masuk akal.
 */
void storage_clampParams(CalibrationParams& params);

#endif // STORAGE_H
