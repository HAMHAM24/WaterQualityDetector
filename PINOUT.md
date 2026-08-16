# 📌 Daftar Pin Mapping Hardware — Water Quality Analyzer

Dokumen ini memuat daftar lengkap seluruh pin GPIO yang digunakan pada board **STM32F401CCU6 (Blackpill)** untuk sistem **Water Quality Analyzer**.

---

## 📋 Tabel Master Pinout (STM32F401CCU6 Blackpill)

| No | Nama Pin STM32 | Fungsi / Modul | Mode Pin / Periferal | Keterangan Wiring & Rangkaian |
|:---:|:---:|:---|:---:|:---|
| **1** | **`PA0`** | **Sensor TDS Analog** | `ADC1_IN0` (Analog 12-bit) | Terhubung ke pin **AOUT / Signal** modul TDS (0 - 3.3V) |
| **2** | **`PA1`** | **Sensor Turbidity Analog** | `ADC1_IN1` (Analog 12-bit) | Terhubung ke pin **AOUT / Signal** modul Turbidity (0 - 3.3V) |
| **3** | **`PA8`** | **Tombol DOWN** | `INPUT_PULLUP` (Active LOW) | Kaki 1 ke **PA8**, Kaki 2 ke **GND** |
| **4** | **`PA9`** | **Serial TX (UART1)** | `USART1_TX` (115200 bps) | Terhubung ke pin **RX** FTDI / USB-to-UART converter |
| **5** | **`PA10`** | **Serial RX (UART1)** | `USART1_RX` (115200 bps) | Terhubung ke pin **TX** FTDI / USB-to-UART converter |
| **6** | **`PB8`** | **OLED Display SCL** | `I2C1_SCL` (Clock 400kHz) | Terhubung ke pin **SCL** OLED SSD1306 128x64 |
| **7** | **`PB9`** | **OLED Display SDA** | `I2C1_SDA` (Data 400kHz) | Terhubung ke pin **SDA** OLED SSD1306 128x64 |
| **8** | **`PB10`** | **Sensor Suhu DS18B20** | `GPIO OneWire` | Terhubung ke pin **Data** DS18B20 *(Wajib resistor pull-up 4.7kΩ ke 3.3V)* |
| **9** | **`PB11`** | **Tombol BACK** | `INPUT_PULLUP` (Active LOW) | Kaki 1 ke **PB11**, Kaki 2 ke **GND** |
| **10** | **`PB12`** | **Tombol OK** | `INPUT_PULLUP` (Active LOW) | Kaki 1 ke **PB12**, Kaki 2 ke **GND** |
| **11** | **`PB13`** | **Tombol RIGHT** | `INPUT_PULLUP` (Active LOW) | Kaki 1 ke **PB13**, Kaki 2 ke **GND** |
| **12** | **`PB14`** | **Tombol UP** | `INPUT_PULLUP` (Active LOW) | Kaki 1 ke **PB14**, Kaki 2 ke **GND** |
| **13** | **`PB15`** | **Tombol LEFT** | `INPUT_PULLUP` (Active LOW) | Kaki 1 ke **PB15**, Kaki 2 ke **GND** |

---

## 🔘 Rincian Khusus: 6 Tombol Navigasi GUI

Semua tombol menggunakan mode **Active LOW** dengan memanfaatkan internal pull-up STM32 (tidak membutuhkan resistor eksternal):

| Tombol | Pin STM32 | Fungsi Navigasi |
|:---:|:---:|:---|
| ⬆️ **UP** | **`PB14`** | Geser kursor ke atas / Tambah target kalibrasi TDS / Kembali ke Dashboard |
| ⬇️ **DOWN** | **`PA8`** | Geser kursor ke bawah / Kurangi target kalibrasi TDS / Masuk ke Rekomendasi |
| ⬅️ **LEFT** | **`PB15`** | Kurangi offset suhu (-0.1°C) / Kurangi level kecerahan OLED |
| ➡️ **RIGHT** | **`PB13`** | Tambah offset suhu (+0.1°C) / Tambah level kecerahan OLED |
| 🟢 **OK** | **`PB12`** | Pilih menu / Kunci nilai kalibrasi / Konfirmasi aksi |
| 🔴 **BACK** | **`PB11`** | Batal proses stabilisasi / Kembali ke menu sebelumnya / Keluar |

---

## 🔌 Rincian Khusus: Sensor & Display

| Komponen | Pin STM32 | Catu Daya | Catatan Tambahan |
|:---|:---:|:---:|:---|
| **OLED SSD1306 (128x64)** | `PB8` (SCL), `PB9` (SDA) | `3.3V & GND` | Bus I2C Fast-Mode (400 kHz) |
| **Sensor Suhu DS18B20** | `PB10` (Data) | `3.3V & GND` | Pasang resistor pull-up 4.7 kΩ antara pin Data dan 3.3V |
| **Modul Sensor TDS** | `PA0` (Analog) | `3.3V / 5V & GND` | Rentang bacaan 0 - 2000 ppm (Kompensasi suhu otomatis) |
| **Modul Turbidity SEN0189**| `PA1` (Analog) | `3.3V / 5V & GND` | Rentang bacaan 0 - 30 NTU |
| **UART Telemetri Debug** | `PA9` (TX), `PA10` (RX) | `GND` bersama | Baudrate 115200 bps (Log telemetri sensor & validasi) |

---

## ⚡ Pin Catu Daya & Pemrograman (ST-Link)

| Pin Board | Fungsi |
|:---:|:---|
| **`3V3`** | Output tegangan 3.3V untuk sensor dan display OLED |
| **`GND`** | Common Ground seluruh komponen |
| **`5V / VIN`**| Input suplai daya utama board (dari USB atau regulator eksternal) |
| **`SWDIO` (`PA13`)** | Jalur Data Pemrograman ST-Link / SWD |
| **`SWCLK` (`PA14`)** | Jalur Clock Pemrograman ST-Link / SWD |
| **`NRST`** | Pin Reset STM32 |

---

*Terakhir diperbarui: Agustus 2026 (Firmware Rev-A Blackpill F401CCU6).*
