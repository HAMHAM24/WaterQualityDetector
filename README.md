# Water Quality Analyzer

Creator: I

Firmware berbasis Arduino/STM32 untuk alat analisis kualitas air dengan tampilan OLED, sensor suhu DS18B20, sensor TDS analog, dan sensor turbidity analog. Proyek ini dibuat dengan arsitektur multitask berbasis FreeRTOS dan berjalan pada board STM32F401CCU6 (Blackpill).

## 1. Deskripsi Proyek

Proyek ini adalah firmware untuk perangkat pengukur kualitas air yang menampilkan hasil pengukuran pada layar OLED 128x64. Sistem membaca data dari beberapa sensor secara periodik, menyimpannya dalam struktur data global, dan menampilkannya melalui antarmuka GUI.

Tujuan utama dari firmware ini:
- membaca suhu air menggunakan DS18B20,
- membaca nilai TDS analog,
- membaca nilai turbidity analog,
- menampilkan hasil secara real-time di OLED,
- menyediakan navigasi menu sederhana menggunakan tombol,
- mendukung kalibrasi dan pengaturan perangkat.

## 2. Spesifikasi Hardware

### Board utama
- STM32F401CCU6 (Blackpill)
- Framework: STM32duino (Arduino Core STM32)
- RTOS: STM32FreeRTOS

### Sensor yang didukung
- DS18B20 untuk pengukuran suhu
- Sensor TDS analog (DFRobot / analog)
- Sensor turbidity analog (SEN0189)

### Display
- OLED SSD1306 128x64 dengan komunikasi I2C

### Input
- 6 tombol navigasi:
  - UP
  - DOWN
  - LEFT
  - RIGHT
  - OK
  - BACK

## 3. Diagram Ringkas Sistem

```text
[STM32F401CCU6]
   |-- UART Serial1 debug
   |-- I2C -> OLED SSD1306
   |-- OneWire -> DS18B20
   |-- ADC CH0 -> TDS Analog
   |-- ADC CH1 -> Turbidity Analog
   |-- GPIO -> 6 tombol
```

## 4. Struktur File Proyek

- `main.ino` – entry point firmware, inisialisasi modul, lalu menjalankan scheduler FreeRTOS.
- `config.h` – konfigurasi pin, timing task, prioritas, stack size, dan parameter umum.
- `globals.h` / `globals.cpp` – variabel global, mutex, queue, dan struktur data sistem.
- `buttons.h` / `buttons.cpp` – pembacaan tombol, debounce, hold, repeat, dan event queue.
- `display.h` / `display.cpp` – driver OLED SSD1306 dan fungsi rendering header/status bar.
- `sensors.h` / `sensors.cpp` – driver sensor, filter moving average, dan pembacaan ADC.
- `gui.h` / `gui.cpp` – antarmuka GUI berbasis state machine.
- `tasks.h` / `tasks.cpp` – pembuatan seluruh task FreeRTOS.

## 5. Konfigurasi Pin

Pin mapping yang digunakan dalam proyek ini adalah:

| Fungsi | Pin |
|---|---|
| UART RX | PA_9 |
| UART TX | PA_10 |
| OLED SCL | PB_8 |
| OLED SDA | PB_9 |
| DS18B20 Data | PB_10 |
| TDS Analog | PA_0 |
| Turbidity Analog | PA_1 |
| BTN UP | PB_12 |
| BTN DOWN | PB_13 |
| BTN LEFT | PB_14 |
| BTN RIGHT | PB_15 |
| BTN OK | PA_8 |
| BTN BACK | PB_11 |

## 6. Library yang Diperlukan

Install library berikut melalui Arduino Library Manager atau dependency manager yang sesuai:

- `STM32duino FreeRTOS`
- `U8g2`
- `OneWire`
- `DallasTemperature`

## 7. Persiapan Pengembangan

### 7.1 Alat yang diperlukan
- Arduino IDE atau VS Code + Arduino extension
- Board package STM32duino
- Programmer / USB serial yang kompatibel dengan Blackpill
- Kabel FTDI atau koneksi UART untuk debugging serial

### 7.2 Board yang harus dipilih
Pada Arduino IDE / STM32duino, pilih board:

- `Generic STM32F4 series`
- `BlackPill F401CC`

### 7.3 Kecepatan serial
- Baud rate default: `115200`

## 8. Cara Build dan Upload

1. Buka folder proyek ini pada Arduino IDE atau editor yang mendukung proyek Arduino.
2. Pastikan board dan port serial sudah benar.
3. Pastikan semua library yang dibutuhkan sudah terpasang.
4. Compile proyek.
5. Upload firmware ke board STM32.
6. Setelah boot, perangkat akan menampilkan splash screen dan masuk ke menu GUI.

## 9. Arsitektur Firmware

Firmware ini memiliki pola multitasking yang jelas:

- `Button Task` membaca dan memproses tombol dengan debounce dan event repeat.
- `Temperature Task` menangani pembacaan suhu DS18B20.
- `Water Sensor Task` membaca TDS dan turbidity secara periodik.
- `GUI Task` memproses state menu dan event tombol.
- `OLED Task` menggambar layar sesuai state terbaru.
- `Serial Debug Task` menyiapkan output debug melalui UART.

Setiap task berkomunikasi melalui mutex dan queue untuk menjaga data tetap konsisten.

## 10. Fitur Utama

- Tampilan OLED 128x64 dengan header dan status bar.
- GUI berbasis FSM (Finite State Machine).
- Sensor suhu DS18B20 dengan konversi non-blocking.
- Pembacaan analog TDS dan turbidity dengan moving average filter.
- Deteksi status sensor: `OK` atau `ERROR`.
- Menu pengaturan brightness/contrast.
- Mode kalibrasi untuk sensor.
- Navigasi tombol yang responsif.

## 11. Cara Penggunaan

### Menu utama
- Gunakan tombol navigasi untuk berpindah halaman.
- Tombol `OK` biasanya digunakan untuk konfirmasi atau masuk ke submenu.
- Tombol `BACK` digunakan untuk kembali ke halaman sebelumnya.

### Pengukuran
- Setelah boot, perangkat akan menampilkan layar utama.
- Firmware membaca sensor secara periodik dan memperbarui data layar.

### Kalibrasi
- Akses menu kalibrasi melalui menu GUI.
- Lakukan kalibrasi sensor sesuai kebutuhan pengujian air.
- Pastikan sensor dalam kondisi stabil saat kalibrasi.

## 12. Catatan Teknis

### Filter moving average
TDS dan turbidity menggunakan buffer circular moving average dengan jumlah sampel tetap. Ini digunakan untuk meredam noise ADC dan menghasilkan pembacaan yang lebih stabil.

### Keamanan thread / data consistency
Semua akses terhadap data sensor dan state sistem dilindungi oleh mutex `g_dataMutex` agar tidak terjadi race condition antar task.

### Scheduler FreeRTOS
Setelah `vTaskStartScheduler()` dipanggil, `loop()` tidak lagi dipakai. Semua eksekusi berjalan di dalam task FreeRTOS.

## 13. Troubleshooting

### OLED tidak tampil
- Periksa koneksi I2C.
- Pastikan pin `PB_8` dan `PB_9` terhubung dengan benar.
- Cek apakah library `U8g2` sudah terinstall.

### Sensor suhu tidak terbaca
- Periksa kabel OneWire.
- Pastikan pin `PB_10` terhubung dengan benar.
- Cek pull-up dan alamat sensor DS18B20.

### Nilai TDS/Turbidity tidak stabil
- Pastikan sensor analog terhubung dengan referensi power dan ground yang benar.
- Gunakan filter waktu yang sudah disediakan.
- Cek apakah pin ADC dan konfigurasi resolusi benar.

### Firmware tidak bisa upload
- Pastikan board dan port yang dipilih sesuai.
- Periksa apakah bootloader / wiring upload sudah benar.
- Gunakan kabel upload dan mode reset yang sesuai untuk STM32.

## 14. Catatan Pengembangan

Proyek ini dirancang dengan pendekatan modular:
- satu file untuk konfigurasi,
- satu file untuk antarmuka hardware,
- satu file untuk GUI,
- satu file untuk task scheduler.

Hal ini memudahkan pengembangan lanjutan, penambahan sensor baru, atau perubahan tampilan tanpa mengubah struktur inti firmware.

## 15. Status Proyek

Proyek ini merupakan firmware embedded yang masih dapat dikembangkan lebih lanjut, terutama untuk:
- peningkatan algoritma kalibrasi,
- pengolahan data kualitas air yang lebih presisi,
- penambahan menu diagnostik,
- integrasi komunikasi ke aplikasi atau server.

## 16. Penutup

Dokumentasi ini dibuat untuk mempermudah pemahaman, pengembangan, dan penggunaan firmware Water Quality Analyzer. Jika Anda ingin melanjutkan proyek ini, pastikan semua pengaturan hardware dan library selalu konsisten dengan file `config.h`.
