   # Water Quality Analyzer

Creator: I

Firmware berbasis Arduino/STM32 untuk alat analisis kualitas air terpadu dengan tampilan OLED 128x64, sensor suhu DS18B20, sensor TDS analog, sensor turbidity analog, serta **Sistem Klasifikasi Kualitas Air berbasis Fuzzy Logic Sugeno Order-0** dan **Penyimpanan Kalibrasi EEPROM Flash STM32**. Proyek ini dibuat dengan arsitektur *multitask preemptive* berbasis **STM32FreeRTOS** dan berjalan pada board **STM32F401CCU6 (Blackpill)**.

---

## 1. Deskripsi Proyek

Proyek ini adalah firmware embedded untuk perangkat pengukur kualitas air multi-parameter yang menampilkan hasil pengukuran dan evaluasi kualitas air secara *real-time* pada layar OLED 128x64. Sistem membaca data dari beberapa sensor secara periodik, melakukan kompensasi suhu terhadap pembacaan TDS, mengolah data melalui **Mesin Evaluasi Fuzzy Sugeno**, menyimpannya secara *thread-safe* di dalam struktur data global, dan menampilkannya melalui antarmuka GUI **Dual-Page View**.

### Tujuan Utama Firmware:
- Membaca suhu air menggunakan sensor digital **DS18B20** (OneWire).
- Membaca nilai **TDS analog** dan menerapkan **kompensasi suhu** (referensi 25 °C).
- Membaca nilai **turbidity analog** (kekeruhan air) dengan filter *Circular Moving Average* (20 sampel).
- Menghitung **Skor Kualitas Air (0–100)** dan menetapkan label status (`LAYAK`, `LTM`, `TL`) serta pesan rekomendasi menggunakan **Fuzzy Logic Sugeno Order-0**.
- Menampilkan hasil secara *real-time* di OLED 128x64 dengan fitur **Dual-Page View** pada layar Pengukuran.
- Menyediakan **Fitur Kalibrasi Sensor Interaktif** (TDS, Turbidity, Suhu) dengan penyimpanan permanen pada memori Flash EEPROM STM32.
- Menyediakan navigasi menu yang intuitif dan aman menggunakan 6 tombol fisik.
- Mendukung pengaturan kecerahan/kontras layar.

---

## 2. Spesifikasi Hardware & Software

### Board Utama
- **MCU**: STM32F401CCU6 (Blackpill ARM Cortex-M4, 84 MHz, 64 KB SRAM, 256 KB Flash)
- **Framework**: STM32duino (Arduino Core STM32)
- **RTOS**: STM32FreeRTOS

### Sensor yang Didukung
- **Suhu**: DS18B20 Digital Waterproof (OneWire Bus pada `PB_10`, resolusi 12-bit)
- **TDS**: Sensor TDS Analog DFRobot (ADC CH0 pada `PA_0`, 12-bit)
- **Turbidity**: Sensor Turbidity Analog SEN0189 (ADC CH1 pada `PA_1`, 12-bit)

### Display & Input
- **Display**: OLED SSD1306 128x64 dengan komunikasi Hardware I2C (`PB_8` SCL, `PB_9` SDA)
- **Input**: 6 Tombol Navigasi (`UP`, `DOWN`, `LEFT`, `RIGHT`, `OK`, `BACK`)

---

## 3. Diagram Ringkas Sistem

```text
[STM32F401CCU6 (Blackpill)]
   |-- EEPROM Flash Memory -> Penyimpanan Permanen Parameter Kalibrasi
   |-- UART Serial (PA_9 TX / PA_10 RX) -> Telemetri & Log Validasi (115200 baud)
   |-- I2C (PB_8 SCL / PB_9 SDA) -> OLED SSD1306 128x64
   |-- OneWire (PB_10) -> DS18B20 Temp Sensor
   |-- ADC CH0 (PA_0) -> TDS Analog Sensor
   |-- ADC CH1 (PA_1) -> Turbidity Analog Sensor
   |-- GPIO Input -> 6 Tombol Navigasi (Active LOW, Internal Pull-Up)
```

---

## 4. Struktur File Proyek

- `main.ino` – Entry point firmware, inisialisasi hardware/software, pengujian otomatis validasi baseline MATLAB, dan inisiasi FreeRTOS scheduler.
- `config.h` – Single source of truth untuk konfigurasi pin, timing task, prioritas, stack size, konstanta konversi sensor (polinomial TDS, slope NTU, divider input), dan tabel profil baku mutu per peruntukan air.
- `globals.h` / `globals.cpp` – Variabel state global, mutex data (`g_dataMutex`), queue tombol (`g_buttonEventQueue`), dan struct `SensorData` & `SystemState`.
- `storage.h` / `storage.cpp` – Modul penyimpanan non-volatile Flash **EEPROM Emulation** untuk parameter kalibrasi ($K$-factor TDS, $V_{\text{clear}}$ Turbidity, Offset Suhu).
- `fuzzy_kualitas_air.h` / `fuzzy_kualitas_air.c` – Engine evaluasi **Fuzzy Logic Sugeno Order-0**, kompensasi suhu TDS, 9 rule base, dan klasifikasi status kualitas air.
- `buttons.h` / `buttons.cpp` – Driver tombol dengan software debounce (30ms), hold (600ms), repeat (150ms), dan event queue.
- `sensors.h` / `sensors.cpp` – Driver sensor DS18B20 non-blocking, pembacaan ADC, filter moving average, penerapan kalibrasi, dan pemrosesan fuzzy thread-safe.
- `display.h` / `display.cpp` – Driver OLED SSD1306 U8g2 full frame buffer dan primitif header/status bar.
- `gui.h` / `gui.cpp` – Antarmuka GUI berbasis Finite State Machine (FSM) dengan fitur **Dual-Page View** pada layar Pengukuran dan menu kalibrasi interaktif.
- `tasks.h` / `tasks.cpp` – Pembuatan dan penanganan 6 task FreeRTOS.
- `kualitas_air.fis` – File ekspor konfigurasi Fuzzy Inference System dari MATLAB.

---

## 5. Konfigurasi Pin Mapping

| Fungsi | Pin MCU | Catatan Hardware |
|---|---|---|
| **UART TX** | `PA_9` | MCU TX -> FTDI RX |
| **UART RX** | `PA_10` | MCU RX <- FTDI TX |
| **OLED SCL** | `PB_8` | Hardware I2C SCL |
| **OLED SDA** | `PB_9` | Hardware I2C SDA |
| **DS18B20 Data** | `PB_10` | OneWire Data (Pull-up 4.7kΩ) |
| **TDS Analog** | `PA_0` | ADC Channel 0 (12-bit) |
| **Turbidity Analog** | `PA_1` | ADC Channel 1 (12-bit) |
| **BTN UP** | `PB_14` | Active LOW (`INPUT_PULLUP`) |
| **BTN DOWN** | `PA_8` | Active LOW (`INPUT_PULLUP`) |
| **BTN LEFT** | `PB_0` | Active LOW (`INPUT_PULLUP`) |
| **BTN RIGHT** | `PB_13` | Active LOW (`INPUT_PULLUP`) |
| **BTN OK** | `PB_12` | Active LOW (`INPUT_PULLUP`) |
| **BTN BACK** | `PB_11` | Active LOW (`INPUT_PULLUP`) |

---

## 6. Library yang Diperlukan

Pasang library berikut melalui **Arduino Library Manager**:

- `STM32duino FreeRTOS`
- `U8g2`
- `OneWire`
- `DallasTemperature`

---

## 7. Arsitektur Multitasking FreeRTOS

Firmware menggunakan arsitektur *preemptive multitasking* dengan 6 task independen:

1. **TaskButton** (Periode: 15 ms | Prioritas: 4 | Stack: 160w): Scan GPIO tombol, debounce, dan melempar event ke `g_buttonEventQueue`.
2. **TaskGui** (Periode: 50 ms | Prioritas: 3 | Stack: 320w): Mengonsumsi event tombol dan meng-update FSM state GUI. **Tidak melakukan penulisan flash** — hanya menjadwalkan penyimpanan kalibrasi via `storage_requestSave()`.
3. **TaskOled** (Periode: 100 ms | Prioritas: 2 | Stack: 512w): Menggambar ulang layar OLED via U8g2 hanya saat `displayDirty == true`. Seluruh format float memakai `dtostrf()`.
4. **TaskWater** (Periode: 200 ms | Prioritas: 2 | Stack: 256w): Sampling ADC, konversi rantai lengkap ke satuan fisik (ADC→Volt→ppm TDS, Volt→NTU), filter moving average, eksekusi penulisan flash yang tertunda (`storage_processPendingSave()`), dan evaluasi Fuzzy Logic Engine.
5. **TaskTemp** (Periode: 1000 ms | Prioritas: 1 | Stack: 192w): Konversi DS18B20 non-blocking (menunggu 750ms secara kooperatif).
6. **TaskDebug** (Periode: 1000 ms | Prioritas: 1 | Stack: 352w): Mengirim log telemetri sistem, pembacaan sensor, dan **stack high water mark** seluruh task ke UART `Serial` (115200 baud).

**Catatan keamanan penulisan Flash:** STM32F401CCU6 TIDAK memiliki EEPROM sejati.
Penulisan kalibrasi menggunakan API buffer EEPROM emulation agar satu siklus
hapus-sektor cukup satu kali untuk seluruh struct, bukan per byte. Operasi
ini dijalankan dari **TaskWater** (bukan TaskGui) dengan
`vTaskSuspendAll()` agar scheduler tidak memproses task lain saat bus flash
terhenti.

---

## 8. Mode Evaluasi Kualitas Air

Firmware memiliki 2 mode evaluasi utama yang disesuaikan dengan regulasi Permenkes RI No. 2 Tahun 2023:

### Mode 1: Air Minum & Higiene Sanitasi (Fuzzy Logic Sugeno Orde-0)
Menggunakan **Fuzzy Inference System (FIS) Sugeno Orde-0** dengan **3 input, 27 aturan**, kurva trapesium (`trapmf`) dan segitiga (`trimf`):
1. **TDS Kompensasi (ppm / mg/L)**: Ideal ($\le 250$), Batas ($150-450$), Tinggi ($>450$)
2. **Turbidity (NTU)**: Jernih ($\le 2.5$), Sedang ($1.5-5.0$), Keruh ($\ge 6.0$)
3. **Suhu Air (Celsius)**: Dingin ($\le 24^\circ\text{C}$), Normal ($24-32^\circ\text{C}$), Panas ($\ge 32^\circ\text{C}$)

- **Output Skor (0.00 – 1.00)** dengan 5 level status:
  - **Skor $\ge 0.875$** $\rightarrow$ **S.LAYAK** (`"Air Sangat Layak"`)
  - **$0.625 \le \text{Skor} < 0.875$** $\rightarrow$ **LAYAK** (`"Layak, Saring Ringan"`)
  - **$0.375 \le \text{Skor} < 0.625$** $\rightarrow$ **CUKUP** (`"Cukup, Perlu Olah Air"`)
  - **$0.125 \le \text{Skor} < 0.375$** $\rightarrow$ **KURANG** (`"Kurang, Butuh Saring Total"`)
  - **Skor $< 0.125$** $\rightarrow$ **T.LAYAK** (`"Tidak Layak / Dilarang"`)

---

### Mode 2: Pemandian / Kolam (Non-Fuzzy Threshold Checker)
Berdasarkan Permenkes No. 2/2023 Tabel 10 (Pemandian Umum) dan Tabel Kolam Renang, evaluasi dilakukan secara **langsung tanpa fuzzy (crisp threshold)** dengan standar gabungan konservatif:
- **Suhu**: `16.0 – 35.0 °C` $\rightarrow$ `[LAYAK]`, di luar itu $\rightarrow$ `[TDK]`
- **Turbidity**: `< 0.5 NTU` $\rightarrow$ `[LAYAK]`, di atas itu $\rightarrow$ `[TDK]`
- **TDS**: **Bypass / Tidak Diatur** (nilai sensor tetap ditampilkan dengan tag `[BYP]`)
- **Status Akhir**: **`LAYAK`** jika kedua parameter lolos, atau **`TIDAK LAYAK`** jika ada parameter yang gagal.

---

## 9. Antarmuka GUI & Katalog Tampilan Layar OLED 1.3" (128x64)

Panel **OLED 1.3 inci SH1106** memiliki resolusi 128x64 piksel (sama dengan 0.96"),
namun piksel fisik lebih besar sehingga proporsi teks lebih proporsional. Layout layar
dibagi menjadi 3 zona: **Header (13 px)**, **Konten Utama (41 px)**, **Status Bar (10 px)**.

### 9.1. Splash Screen (Tampilan Booting)
Tampil otomatis selama **2 detik** saat perangkat baru dinyalakan.
```text
+---------------------------------------------------+
|                                                   |
|                   Physic Water                    |
|                  Quality Index                    |
|                       FBN                         |
|                      v1.0.0                       |
|                                                   |
+---------------------------------------------------+
```

---

### 9.2. Menu Utama (Pemilihan Objek Air & Fitur)
Navigasi tombol `UP`/`DOWN` menggeser kursor `>`. Tekan `OK` untuk memilih mode.
```text
+---------------------------------------------------+
| Pilih Mode Uji Air                                |
|---------------------------------------------------|
| > Air Minum & Higiene                             |
|   Pemandian / Kolam                               |
|   Kalibrasi Sensor                                |
|   Pengaturan OLED                                 |
|---------------------------------------------------|
| UP/DN:Pilih                              OK:Masuk |
+---------------------------------------------------+
```

---

### 9.3. Screen Tunggu (Stabilisasi Sensor - 5 Detik)
Tampil selama **5 detik** setelah memilih mode air untuk memberikan waktu
stabilisasi pembacaan filter *Circular Moving Average* (20 sampel).
```text
+---------------------------------------------------+
| MODE: AIR MINUM                                   |
|---------------------------------------------------|
|                                                   |
|   Membaca Sensor...                               |
|   [||||||||||||||||||................]            |
|                                                   |
|---------------------------------------------------|
| Stabilisasi...                         BACK:Batal |
+---------------------------------------------------+
```

---

### 9.4. Layar Hasil Pengukuran - Dual-Page View
Menampilkan hasil pengolahan data sensor dan evaluasi Fuzzy Sugeno ($0.00 - 1.00$) secara *real-time*.

#### Page 1: Dashboard Data Sensor & Skor (Default)
Gunakan tombol `DOWN` $\downarrow$ untuk melihat detail rekomendasi, atau `BACK` untuk kembali ke Menu Utama.
```text
+---------------------------------------------------+
| Air Minum (1/2)                                   |
|---------------------------------------------------|
| Suhu : 27.5 C (Normal)                            |
| TDS  : 343.1 ppm                                  |
| Turb : 10.0 NTU                                   |
| Skor : 0.50 [POOR]                                |
|---------------------------------------------------|
| DN:Detail                               BACK:Menu |
+---------------------------------------------------+
```

#### Page 2: Detail Rekomendasi Tindakan
Gunakan tombol `UP` $\uparrow$ untuk kembali ke dashboard nilai, atau `BACK` untuk kembali ke Menu Utama.
```text
+---------------------------------------------------+
| Rekomendasi (2/2)                                 |
|---------------------------------------------------|
| Mutu  : POOR [Kurang]                             |
| Suhu  : 27.5C [Normal]                            |
| Saran :                                           |
| Perlu Filtrasi Ringan                             |
|---------------------------------------------------|
| UP:Kembali                              BACK:Menu |
+---------------------------------------------------+
```

---

### 9.5. Menu Kalibrasi Sensor & Sub-menu (CALIBRATION)
Digunakan untuk mengkalibrasi sensor TDS, Turbidity, Suhu, atau mereset ke standar pabrik secara interaktif.

#### Menu Pilihan Kalibrasi:
```text
+---------------------------------------------------+
| Kalibrasi Sensor                                  |
|---------------------------------------------------|
| > Kalibrasi TDS                                   |
|   Kalibrasi Turbidity                             |
|   Kalibrasi Suhu                                  |
|   Reset Pabrik                                    |
|---------------------------------------------------|
| OK:Pilih                                BACK:Menu |
+---------------------------------------------------+
```

#### 1. Layar Interaktif Kalibrasi TDS:
Gunakan tombol `UP`/`DOWN` untuk menyelaraskan nilai Target Acuan (misal `707 ppm`), lalu tekan `OK` untuk menghitung $K$-factor baru dan menyimpannya ke EEPROM Flash.
```text
+---------------------------------------------------+
| Kalibrasi TDS                                     |
|---------------------------------------------------|
| ADC Raw : 1245                                    |
| Target  : [ 707 ppm ]                             |
| UP/DN:Target OK:Simpan                            |
|                                                   |
|---------------------------------------------------|
| UP/DN:Ubah                             BACK:Batal |
+---------------------------------------------------+
```

#### 2. Layar Interaktif Kalibrasi Turbidity:
Celupkan sensor ke air murni jernih (aquades 0 NTU), lalu tekan tombol `OK` untuk mengunci tegangan $V_{\text{clear}}$ dan menyimpannya ke EEPROM Flash.
```text
+---------------------------------------------------+
| Kalibrasi Turbidity                               |
|---------------------------------------------------|
| Volt   : 3.10 V                                   |
| V_Clear: 3.10 V                                   |
| Tekan OK: Lock 0 NTU                              |
|                                                   |
|---------------------------------------------------|
| Air Aquades                            BACK:Batal |
+---------------------------------------------------+
```

#### 3. Layar Interaktif Kalibrasi Suhu:
Gunakan tombol `LEFT`/`RIGHT` untuk mengatur offset koreksi suhu ($\pm 0.1\text{ }^\circ\text{C}$), lalu tekan tombol `OK` untuk menyimpannya ke EEPROM Flash.
```text
+---------------------------------------------------+
| Kalibrasi Suhu                                    |
|---------------------------------------------------|
| Suhu Raw: 27.4 C                                  |
| Offset  : [ +0.1 C ]                              |
| LF/RT:Offset OK:Simpan                            |
|                                                   |
|---------------------------------------------------|
| LF/RT:Ubah                             BACK:Batal |
+---------------------------------------------------+
```

---

### 9.6. Menu Pengaturan OLED & Adjust Mode (SETTINGS)
Mengatur tingkat Kecerahan dan Kontras OLED secara *real-time*.

```text
+---------------------------------------------------+
| Pengaturan OLED                                   |
|---------------------------------------------------|
| > Brightness                           200        |
|   Kontras                              128        |
|   Reset Pengaturan                                |
|   Informasi Firmware                              |
|---------------------------------------------------|
| OK:Atur                                 BACK:Menu |
+---------------------------------------------------+
```

#### Tampilan Adjust Mode (`*`):
Tekan `OK` pada `Brightness`/`Kontras` hingga kursor berubah menjadi `*`. Gunakan tombol `LEFT`/`RIGHT` untuk mengubah nilai, tekan `OK`/`BACK` untuk menyimpan.
```text
+---------------------------------------------------+
| Pengaturan OLED                                   |
|---------------------------------------------------|
| * Brightness                           215        |
|   Kontras                              128        |
|   Reset Pengaturan                                |
|   Informasi Firmware                              |
|---------------------------------------------------|
| LF/RT:Ubah                             OK:Selesai |
+---------------------------------------------------+
```

---

### 9.7. Layar Informasi System (ABOUT)
Menampilkan spesifikasi firmware, MCU, dan penggunaan RAM secara *real-time*.
```text
+---------------------------------------------------+
| Tentang Alat                                      |
|---------------------------------------------------|
| Alat: Water Quality Analyzer                      |
| FW  : v1.0.0 (FBN)                                |
| HW  : Rev-A (Blackpill F401CCU6)                  |
| MCU : STM32F401CCU6                               |
| RTOS: FreeRTOS Aktif                              |
| Heap: 48240 B                                     |
|---------------------------------------------------|
| Info Sistem                             BACK:Menu |
+---------------------------------------------------+
```

---

## 10. Pengujian & Validasi Baseline

Saat perangkat dinyalakan, fungsi `setup()` di `main.ino` akan mengeksekusi uji coba validasi otomatis 1x pada `Serial`:

```text
========================================
    VALIDASI AUTOMATIS FUZZY LOGIC    
========================================
Input Test  : TDS = 350.0 ppm, Turbidity = 10.0 NTU
Hasil Skor  : 0.50
Status Badge: POOR
Status Pesan: Perlu Filtrasi Ringan
========================================
Air Jernih (50 ppm, 0.5 NTU): 1.00 [EXCELLENT]
Air Buruk (1100 ppm, 28 NTU): 0.00 [NOT-SUIT]
========================================
```

Target skor $0.50$ (`POOR`) ini memverifikasi bahwa perhitungan Fuzzy Sugeno pada MCU STM32 100% konsisten dengan rancangan FIS dan simulasi MATLAB.

---

## 11. Panduan Cara Kalibrasi Sensor

### A. Kalibrasi TDS (1-Point Buffer Solution)
1. Siapkan larutan standar TDS acuan (misal **707 ppm**).
2. Celupkan probe TDS ke dalam larutan standar dan tunggu hingga nilai pembacaan stabil.
3. Masuk ke **Menu Utama** $\rightarrow$ **Kalibrasi** $\rightarrow$ **Kalibrasi TDS**.
4. Tekan tombol `UP` atau `DOWN` untuk menyelaraskan nilai **Target** di layar hingga sama dengan larutan standar (`707 ppm`).
5. Tekan tombol **OK**. Perangkat akan menghitung faktor pengali $K$-Factor baru secara otomatis dan menyimpannya ke memori EEPROM Flash.

### B. Kalibrasi Turbidity (Air Murni 0 NTU)
1. Siapkan air murni aquades (0 NTU).
2. Celupkan sensor Turbidity SEN0189 ke dalam air aquades.
3. Masuk ke **Menu Utama** $\rightarrow$ **Kalibrasi** $\rightarrow$ **Kalibrasi Turbidity**.
4. Amati nilai ADC Raw dan tegangan pada layar hingga stabil.
5. Tekan tombol **OK**. Perangkat akan mengunci tegangan air jernih $V_{\text{clear}}$ sebagai referensi 0 NTU dan menyimpannya ke memori EEPROM Flash.

### C. Kalibrasi Suhu (DS18B20 Offset)
1. Tempatkan DS18B20 bersama termometer laboratorium presisi di dalam wadah air yang sama.
2. Masuk ke **Menu Utama** $\rightarrow$ **Kalibrasi** $\rightarrow$ **Kalibrasi Suhu**.
3. Bandingkan nilai **Suhu Raw** pada layar dengan pembacaan termometer laboratorium.
4. Tekan tombol `LEFT` atau `RIGHT` untuk menambah atau mengurangi nilai **Offset** hingga hasil akhir sesuai dengan termometer laboratorium.
5. Tekan tombol **OK** untuk menyimpan nilai offset ke memori EEPROM Flash.

---

## 12. Cara Build dan Upload

1. Buka folder proyek ini pada **Arduino IDE** atau **VS Code + STM32duino**.
2. Pilih Board: **Generic STM32F4 series** $\rightarrow$ **BlackPill F401CC**.
3. Pastikan semua library pendukung sudah terinstall.
4. Compile dan upload firmware ke STM32 Blackpill.
5. Buka Serial Monitor pada baud rate `115200` untuk mengamati log startup validasi dan telemetri debug.
