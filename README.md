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

### Kalibrasi Turbidity Dua Titik
Sensor turbidity memakai kalibrasi dua titik agar slope NTU sesuai karakteristik
sensor yang dipakai. Langkah dari menu **Kalibrasi Turbidity**:
1. Celupkan sensor ke air jernih (0 NTU), lalu tekan `OK` untuk mengambil titik pertama.
2. Celupkan sensor ke larutan standar. Atur nilai custom `1-3000 NTU` dengan `UP/DOWN`.
3. Tekan `OK` untuk menyimpan kedua titik ke EEPROM Flash.

Rumus yang digunakan:

```text
slope = NTU_standar / (V_jernih - V_standar)
NTU   = (V_jernih - V_ukur) x slope
```

Jika titik kedua belum dikalibrasi, firmware memakai slope fallback `30 NTU/V`.
Untuk sensor yang disuplai 3.3V, `TURBIDITY_INPUT_DIVIDER` tetap `1.0`.

---

## 9. Antarmuka GUI & Katalog Tampilan Layar OLED 1.3" (128x64)

Panel **OLED 1.3 inci SH1106 / SSD1306** memiliki resolusi 128x64 piksel. Layout layar
dibagi menjadi 3 zona: **Header (12 px)**, **Konten Utama (44 px)**, **Status Bar (8 px)**.

### 9.1. Splash Screen (Tampilan Booting)
Tampil otomatis selama **2 detik** saat perangkat baru dinyalakan.
```text
+---------------------------------------------------+
|                                                   |
|                   Physic Water                    |
|                  Quality Index                    |
|                       FBN                         |
|                      v2.1.0                       |
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
|                       60%                         |
|---------------------------------------------------|
| Stabilisasi...                         BACK:Batal |
+---------------------------------------------------+
```

---

### 9.4. Layar Hasil Pengukuran - Three-Page View
Menampilkan hasil pengolahan data sensor secara bertahap melalui **3 Halaman Navigasi**:

#### Mode 1: Air Minum & Higiene Sanitasi (Fuzzy Sugeno)

**Halaman 1/3: Dashboard Hasil Sensor & Skor Mutu**
```text
+---------------------------------------------------+
| Air Minum (1/3)                                   |
|---------------------------------------------------|
| Suhu : 27.5 C (Normal)                            |
| TDS  : 280.0 ppm                                  |
| Turb : 1.5 NTU                                    |
| Skor : 1.00 [S.LAYAK]                             |
|---------------------------------------------------|
| DN:Detail                               BACK:Menu |
+---------------------------------------------------+
```

**Halaman 2/3: Diagnosis Parameter Penyebab**
```text
+---------------------------------------------------+
| Diagnosis (2/3)                                   |
|---------------------------------------------------|
| Mutu: S.LAYAK                                     |
| Suhu: Normal                                      |
| TDS : Ideal                                       |
| Turb: Jernih                                      |
|---------------------------------------------------|
| DN:Saran                                BACK:Menu |
+---------------------------------------------------+
```

**Halaman 3/3: Saran Tindakan Spesifik**
```text
+---------------------------------------------------+
| Saran (3/3)                                       |
|---------------------------------------------------|
| Air layak digunakan                               |
| Pantau berkala                                    |
|                                                   |
|                                                   |
|---------------------------------------------------|
| UP:Diagnosis                            BACK:Menu |
+---------------------------------------------------+
```
*(Bila ada parameter bermasalah, halaman 3 langsung menyarankan tindakan tepat, misal: `TDS : RO/ganti air`, `Turb: filter total`, `Suhu: hangatkan air`, `Uji ulang air`).*

---

#### Mode 2: Pemandian / Kolam (Non-Fuzzy Threshold Checker)

**Halaman 1/3: Dashboard Per-Parameter**
```text
+---------------------------------------------------+
| Pemandian (1/3)                                   |
|---------------------------------------------------|
| Suhu: 28.5 C [LAYAK]                              |
| Turb: 0.3 NTU [LAYAK]                             |
| TDS : 280 ppm [BYP]                               |
| Status: LAYAK                                     |
|---------------------------------------------------|
| DN:Detail                               BACK:Menu |
+---------------------------------------------------+
```

**Halaman 2/3: Diagnosis Batas Regulasi**
```text
+---------------------------------------------------+
| Diagnosis (2/3)                                   |
|---------------------------------------------------|
| Batas Pemandian/Kolam                             |
| Suhu: 16-35C [LAYAK]                              |
| Turb: <0.5NTU [LAYAK]                             |
| TDS : BYPASS                                      |
|---------------------------------------------------|
| DN:Saran                                BACK:Menu |
+---------------------------------------------------+
```

**Halaman 3/3: Saran Tindakan**
```text
+---------------------------------------------------+
| Saran (3/3)                                       |
|---------------------------------------------------|
| Air layak digunakan                               |
| Jaga air tetap jernih                             |
|                                                   |
|                                                   |
|---------------------------------------------------|
| UP:Diagnosis                            BACK:Menu |
+---------------------------------------------------+
```
*(Bila ada parameter tidak aman, halaman 3 menampilkan: `Suhu: atur suhu air`, `Turb: jernihkan air`, `Uji ulang air`).*

---

### 9.5. Menu Kalibrasi Sensor & Sub-menu (CALIBRATION)
Mengelola kalibrasi probe TDS, Turbidity (2-titik), Suhu, dan Reset Pabrik.

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

#### 1. Kalibrasi TDS:
Celupkan probe ke larutan standar (misal `707 ppm`). Gunakan `UP`/`DOWN` untuk menyelaraskan nilai target acuan (presisi `+-1 ppm`), lalu tekan `OK` untuk menyimpan.
```text
+---------------------------------------------------+
| Kalibrasi TDS                                     |
|---------------------------------------------------|
| ADC Raw : 1245                                    |
| Target  : [ 707 ppm ]                             |
| UP/DN:Atur  OK:Simpan                             |
|                                                   |
|---------------------------------------------------|
| Celup larutan                          BACK:Batal |
+---------------------------------------------------+
```

#### 2. Kalibrasi Turbidity Dua Titik:
- **Langkah 1/2**: Celupkan sensor ke air jernih (0 NTU), tekan `OK` untuk merekam tegangan $V_{\text{clear}}$.
- **Langkah 2/2**: Celupkan ke larutan standar. Atur nilai NTU custom (`1-3000 NTU`, step `+-5 NTU`) dengan `UP`/`DOWN`, lalu tekan `OK` untuk mengunci slope dan menyimpan ke EEPROM.

```text
+---------------------------------------------------+
| Turbidity (1/2)                                   |
|---------------------------------------------------|
| Volt   : 3.25 V                                   |
| Air jernih: OK ambil                              |
|                                                   |
|---------------------------------------------------|
| Air jernih 0 NTU                       BACK:Batal |
+---------------------------------------------------+
```
```text
+---------------------------------------------------+
| Turbidity (2/2)                                   |
|---------------------------------------------------|
| Volt   : 2.10 V                                   |
| Std: 1000 NTU                                     |
| V0 : 3.25 V                                       |
| UP/DN atur OK simpan                              |
|---------------------------------------------------|
| Larutan standar                        BACK:Batal |
+---------------------------------------------------+
```

#### 3. Kalibrasi Suhu:
Bandingkan suhu raw dengan termometer presisi. Gunakan `LEFT`/`RIGHT` untuk mengatur offset ($\pm 0.1\text{ }^\circ\text{C}$), lalu tekan `OK`.
```text
+---------------------------------------------------+
| Kalibrasi Suhu                                    |
|---------------------------------------------------|
| Suhu Raw: 27.4 C                                  |
| Offset  : [ +0.1 C ]                              |
| LF/RT:Atur  OK:Simpan                             |
|                                                   |
|---------------------------------------------------|
| Cek termometer                         BACK:Batal |
+---------------------------------------------------+
```

#### 4. Konfirmasi Reset Pabrik:
Mencegah data kalibrasi dan pengaturan OLED terhapus secara tidak sengaja.
```text
+---------------------------------------------------+
| Reset Pabrik?                                     |
|---------------------------------------------------|
| Semua kalibrasi &                                 |
| OLED akan direset!                                |
| OK:Ya      BACK:Batal                             |
|                                                   |
|---------------------------------------------------|
| Yakin reset?                           BACK:Batal |
+---------------------------------------------------+
```

---

### 9.6. Menu Pengaturan OLED (Tersimpan di EEPROM)
Mengatur tingkat Kecerahan dan Kontras OLED secara *real-time*. Nilai otomatis tersimpan permanen ke EEPROM Flash saat selesai diatur.

```text
+---------------------------------------------------+
| Pengaturan OLED                                   |
|---------------------------------------------------|
| > Brightness                           200        |
|   Kontras                              128        |
|   Reset Pengaturan                                |
|   Informasi Firmware                              |
|---------------------------------------------------|
| OK:Pilih                                BACK:Menu |
+---------------------------------------------------+
```

#### Adjust Mode Brightness / Kontras
Tekan `OK` pada `Brightness` atau `Kontras` untuk masuk mode edit (`*`).
Gunakan `LEFT`/`RIGHT` untuk mengubah nilai. Tekan `OK` atau `BACK` untuk
selesai dan menjadwalkan penyimpanan EEPROM.
```text
+---------------------------------------------------+
| Pengaturan OLED                                   |
|---------------------------------------------------|
| * Brightness                           215        |
|   Kontras                              128        |
|   Reset Pengaturan                                |
|   Informasi Firmware                              |
|---------------------------------------------------|
| OK:Pilih                                BACK:Menu |
+---------------------------------------------------+
```

#### Reset Pengaturan OLED
Pilih `Reset Pengaturan`, lalu tekan `OK`. Brightness dan kontras kembali ke
nilai default serta disimpan ke EEPROM. Selama write Flash tertunda, status bar
menampilkan `Menyimpan...`.
```text
+---------------------------------------------------+
| Pengaturan OLED                                   |
|---------------------------------------------------|
| > Brightness                           200        |
|   Kontras                              128        |
|   Reset Pengaturan                                |
|   Informasi Firmware                              |
|---------------------------------------------------|
| Menyimpan...                            BACK:Menu |
+---------------------------------------------------+
```

---

### 9.7. Layar Informasi Sistem (ABOUT — Dual-Page View)
Menampilkan identitas firmware, tipe hardware, dan status FreeRTOS yang dipisah menjadi **2 Halaman**:

#### Halaman 1/2: Software & Metodologi
```text
+---------------------------------------------------+
| Tentang (1/2)                                     |
|---------------------------------------------------|
| Alat: WQ Analyzer                                 |
| FW  : v2.1.0                                      |
| Reg : Permenkes 2023                              |
| RTOS: FreeRTOS Aktif                              |
|---------------------------------------------------|
| DN:Hardware                             BACK:Menu |
+---------------------------------------------------+
```

#### Halaman 2/2: Hardware & Memori
```text
+---------------------------------------------------+
| Tentang (2/2)                                     |
|---------------------------------------------------|
| HW  : Blackpill F401                              |
| MCU : STM32F401CC                                 |
| OLED: 1.3 SH1106                                  |
| Heap: 48240 B                                     |
|---------------------------------------------------|
| UP:Firmware                             BACK:Menu |
+---------------------------------------------------+
```

---

## 10. Pengujian & Validasi Baseline

Saat perangkat dinyalakan, fungsi `setup()` di `main.ino` akan mengeksekusi uji coba validasi otomatis 1x pada `Serial`:

```text
========================================
    VALIDASI AUTOMATIS FIRMWARE         
========================================
Air Minum (Ideal)     : 1.00 [S.LAYAK]
Air Minum (1 Batas)   : 0.75 [LAYAK]
Pemandian (28C, 0.3NTU): [LAYAK]
Pemandian (28C, 2.5NTU): [TDK LAYAK]
Turb 2 titik (2.50V): 50.0 NTU
========================================
```

---

## 11. Panduan Kalibrasi Sensor

### A. Kalibrasi Suhu (DS18B20 Offset) — Lakukan Pertama
1. Tempatkan probe DS18B20 bersama termometer laboratorium presisi di dalam wadah air yang sama.
2. Masuk ke **Menu Utama** $\rightarrow$ **Kalibrasi Sensor** $\rightarrow$ **Kalibrasi Suhu**.
3. Bandingkan nilai **Suhu Raw** pada layar dengan termometer acuan.
4. Tekan tombol `LEFT` atau `RIGHT` untuk menyelaraskan nilai **Offset** ($\pm 0.1\text{ }^\circ\text{C}$).
5. Tekan tombol **OK** untuk menyimpan nilai offset ke EEPROM Flash.

### B. Kalibrasi TDS (1-Point Solution)
1. Siapkan larutan standar TDS acuan (misal **707 ppm**).
2. Celupkan probe TDS ke dalam larutan dan tunggu pembacaan stabil.
3. Masuk ke **Menu Utama** $\rightarrow$ **Kalibrasi Sensor** $\rightarrow$ **Kalibrasi TDS**.
4. Tekan tombol `UP` atau `DOWN` untuk menyelaraskan nilai **Target** di layar hingga sama dengan larutan standar (`707 ppm`) dengan ketelitian **$\pm 1\text{ ppm}$**.
5. Tekan tombol **OK** untuk menghitung $K$-Factor baru dan menyimpannya ke EEPROM Flash.

### C. Kalibrasi Turbidity (2-Point Custom Wizard)
1. Masuk ke **Menu Utama** $\rightarrow$ **Kalibrasi Sensor** $\rightarrow$ **Kalibrasi Turbidity**.
2. **Titik 1 (0 NTU)**: Celupkan sensor ke air aquades murni. Amati nilai voltase hingga stabil, lalu tekan **OK**.
3. **Titik 2 (Standar Custom)**: Celupkan sensor ke larutan standar (misal **100 NTU** atau **1000 NTU**). Tekan `UP`/`DOWN` untuk menyelaraskan angka standar di layar dengan larutan yang dipakai (kenaikan **$\pm 5\text{ NTU}$**).
4. Tekan tombol **OK**. Firmware akan mengunci kedua titik, menghitung slope akurat sensor, dan menyimpannya ke EEPROM Flash.

---

## 12. Cara Build dan Upload

1. Buka folder proyek ini pada **Arduino IDE** atau **VS Code + STM32duino**.
2. Pilih Board: **Generic STM32F4 series** $\rightarrow$ **BlackPill F401CC**.
3. Pastikan library pendukung terinstall: `STM32duino FreeRTOS`, `U8g2`, `OneWire`, `DallasTemperature`.
4. Compile dan upload firmware ke STM32 Blackpill.
5. Buka Serial Monitor pada baud rate `115200` untuk mengamati log startup validasi dan telemetri debug.
