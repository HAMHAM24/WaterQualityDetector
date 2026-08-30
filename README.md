# Water Quality Analyzer & Physic Quality Index (FBN)
### Firmware Alat Uji Mutu Fisik Air Berbasis STM32F401CCU6 (BlackPill) & STM32FreeRTOS

Firmware embedded canggih untuk alat analisis mutu fisik air terpadu dengan tampilan **OLED 1.3" SH1106 / SSD1306 (128x64 piksel)**, sensor suhu digital **DS18B20**, sensor **TDS analog**, sensor **turbidity analog (SEN0189)**, **Sistem Inferensi Fuzzy Sugeno Orde-0 (3 Input, 64 Aturan, Worst-Parameter Wins)** berdasar standar **Permenkes RI No. 2 Tahun 2023**, serta **Penyimpanan Parameter Kalibrasi & Display pada EEPROM Flash Emulation STM32**.

---

## 1. Deskripsi Proyek & Fitur Utama

Proyek ini dirancang sebagai instrumen uji kualitas fisik air lapangan portabel yang bekerja secara mandiri (*standalone*), cepat, dan andal dengan fitur-fitur utama:

- **Pengukuran Suhu Digital Presisi (DS18B20)**: Membaca suhu air secara non-blocking melalui bus OneWire (`PB_10`).
- **Kompensasi Termal TDS Pasca-Polinomial**: Mengoreksi efek fluktuasi suhu terhadap konduktivitas ion menggunakan formula standar $2\%\text{ per }^\circ\text{C}$ pada suhu referensi $25.0^\circ\text{C}$.
- **Filter Circular Moving Average (20 Sampel)**: Menghaluskan pembacaan ADC TDS dan Turbidity dari noise listrik frekuensi tinggi.
- **Workflow-Driven GUI dengan Input Suhu Udara Manual**: Mengizinkan pengguna memasukkan suhu lingkungan ($T_{\text{udara}}$) secara manual untuk menghitung deviasi suhu air ($\Delta T = |T_{\text{air}} - T_{\text{udara}}|$) tanpa menunggu inersia termal probe *stainless steel*.
- **Deteksi Stabilisasi Suhu Air Otomatis**: Memastikan kestabilan probe sensor ($3\times$ pembacaan beruntun variasi $\le 0.2^\circ\text{C}$, timeout $60\text{ s}$) sebelum mengunci data.
- **Mesin Evaluasi Fuzzy Sugeno Orde-0 (64 Rule)**: Menghitung skor mutu air ($0.00 - 1.00$) menggunakan logika keparahan dominan (*Worst Parameter Wins*) dan 4 tingkat status mutu (`S.LAYAK`, `P.SED`, `P.INT`, `T.LOLOS`).
- **Compliance Hard Gate (Pengunci Regulasi Permenkes)**: Mengunci status ke `T.LOLOS` ($Z=0.00$) secara mutlak jika $\text{TDS} \ge 300\text{ mg/L}$, $\text{Turbidity} \ge 3.0\text{ NTU}$, atau $\Delta T > 3.0^\circ\text{C}$.
- **Mode Pemandian / Kolam (Non-Fuzzy Threshold Checker)**: Pengecekan ambang batas langsung untuk air rekreasi ($16.0–35.0^\circ\text{C}$, $<0.5\text{ NTU}$, TDS bypass).
- **Antarmuka Three-Page View**:
  - `Halaman 1/3`: Dashboard hasil pengukuran live & skor mutu.
  - `Halaman 2/3`: Diagnosis parameter penyebab (menunjukkan status mutu per-sensor).
  - `Halaman 3/3`: Saran tindakan spesifik dan terarah tanpa teks terpotong.
- **Kalibrasi Interaktif & Permanen di Flash EEPROM**:
  - TDS 1-Titik dengan ketelitian step $\pm 1\text{ ppm}$.
  - Turbidity Wizard 2-Titik (Titik 1: Air Aquades 0 NTU, Titik 2: Larutan Standar Custom $1–3000\text{ NTU}$ step $\pm 5\text{ NTU}$).
  - Suhu Offset DS18B20 step $\pm 0.1^\circ\text{C}$.
  - Kecerahan (*Brightness*) & Kontras OLED otomatis tersimpan di Flash.
  - Halaman pengaman **Konfirmasi Reset Pabrik** (*Anti-Accidental Reset*).
- **Navigasi 6 Tombol Ergonomis**: `UP`, `DOWN`, `LEFT`, `RIGHT`, `OK`, `BACK`.

---

## 2. Spesifikasi Perangkat Keras & Lunak

### Board Utama
- **MCU**: STM32F401CCU6 (BlackPill ARM Cortex-M4, 84 MHz, 64 KB SRAM, 256 KB Flash)
- **Framework**: STM32duino (Arduino Core STM32 resmi STMicroelectronics)
- **RTOS**: STM32FreeRTOS (Kernel FreeRTOS V10.x Preemptive)

### Sensor yang Didukung
- **Suhu Air**: DS18B20 Digital Waterproof Probe (OneWire Bus pada `PB_10`, resolusi 12-bit)
- **TDS**: DFRobot Analog TDS Meter / SEN0244 (ADC Channel 0 pada `PA_0`, 12-bit $0–4095$)
- **Turbidity**: DFRobot SEN0189 Analog Turbidity Sensor (ADC Channel 1 pada `PA_1`, 12-bit $0–4095$, disuplai 3.3V)

### Display & Antarmuka
- **Display**: OLED 1.3" SH1106 / SSD1306 128x64 I2C Hardware (`PB_8` SCL, `PB_9` SDA, clock 400 kHz)
- **Input Tombol**: 6 Push Button dengan internal pull-up aktif LOW (`PB_14`, `PA_8`, `PB_0`, `PB_13`, `PB_12`, `PB_11`)
- **Komunikasi Data / Telemetri**: USART1 Serial (`PA_9` TX, `PA_10` RX) baud rate 115200

---

## 3. Diagram Blok Sistem

```text
                                [ STM32F401CCU6 (BlackPill) ]
                                              │
    ┌──────────────────┬──────────────────────┼──────────────────────┬──────────────────┐
    ▼                  ▼                      ▼                      ▼                  ▼
[OneWire PB_10]   [ADC0 PA_0]            [ADC1 PA_1]            [I2C PB_8/PB_9]   [USART1 PA_9/10]
DS18B20 Suhu      TDS Analog          Turbidity SEN0189       OLED 1.3" SH1106    Serial Telemetri
Digital Sensor    (0-3.3V ADC)           (0-3.3V ADC)         (128x64 400 kHz)      (115200 baud)
                                              │
                         ┌────────────────────┴────────────────────┐
                         ▼                                         ▼
                 [6 Tombol Navigasi]                    [Emulasi Flash EEPROM]
             UP/DOWN/LEFT/RIGHT/OK/BACK                 TDS K, Vclear, Vstd,
             (Internal Pull-Up Active LOW)              Offset, Brightness/Contrast
```

---

## 4. Struktur File Proyek

- `main.ino` – Entry point firmware, inisialisasi hardware, self-test validasi baseline startup, dan start scheduler FreeRTOS.
- `config.h` – Single source of truth untuk pin mapping, konstanta konversi sensor, konstanta timing/stack task, dan batas baku mutu.
- `globals.h` / `globals.cpp` – Variabel state global, handle mutex `g_dataMutex`, queue tombol `g_buttonEventQueue`, dan struct `SensorData` / `SystemState`.
- `storage.h` / `storage.cpp` – Modul penyimpanan non-volatile Flash EEPROM Emulation (buffered write 1-cycle erase).
- `fuzzy_kualitas_air.h` / `fuzzy_kualitas_air.c` – Mesin inferensi Fuzzy Sugeno 64 aturan (*worst-parameter wins*), fungsi membership trapesium/segitiga, dan evaluasi threshold Pemandian.
- `buttons.h` / `buttons.cpp` – Driver 6 tombol navigasi dengan software debounce (30 ms), hold (600 ms), repeat (150 ms), dan event queue.
- `sensors.h` / `sensors.cpp` – Driver sensor non-blocking (DS18B20, ADC TDS & Turbidity, filter circular moving average, kompensasi suhu TDS).
- `display.h` / `display.cpp` – Driver OLED SH1106 / SSD1306 berbasis U8g2 full frame buffer dan primitif header/status bar.
- `gui.h` / `gui.cpp` – Finite State Machine (FSM) GUI, guided workflow, input suhu udara manual, stabilisasi suhu, three-page view, dan kalibrasi interaktif.
- `tasks.h` / `tasks.cpp` – Orkestrasi 6 task multitasking FreeRTOS preemptive.
- `kualitas_air.fis` – File ekspor model Fuzzy Inference System Sugeno 64 rule untuk MATLAB.
- `Note/Laporan_Lengkap_Fuzzy_Sugeno_dan_Implementasi.md` – Laporan lengkap metodologi dan dasar teori untuk naskah Skripsi/TA.

---

## 5. Konfigurasi Pin Mapping

| Nama Port / Pin | Fungsi Hardware | Catatan Rangkaian |
|---|---|---|
| `PA_9` | **UART TX** | STM32 TX $\rightarrow$ FTDI / Serial Monitor RX |
| `PA_10` | **UART RX** | STM32 RX $\leftarrow$ FTDI / Serial Monitor TX |
| `PB_8` | **OLED SCL** | Hardware I2C1 SCL (Pull-up internal/modul) |
| `PB_9` | **OLED SDA** | Hardware I2C1 SDA (Pull-up internal/modul) |
| `PB_10` | **DS18B20 Data** | OneWire Data Bus (Pull-up eksternal $4.7\text{ k}\Omega$ ke 3.3V) |
| `PA_0` | **TDS Analog** | ADC1 Channel 0 ($0–3.3\text{ V}$, resolusi 12-bit) |
| `PA_1` | **Turbidity Analog** | ADC1 Channel 1 ($0–3.3\text{ V}$, resolusi 12-bit) |
| `PB_14` | **Tombol UP** | Active LOW (`INPUT_PULLUP`, ke GND saat ditekan) |
| `PA_8` | **Tombol DOWN** | Active LOW (`INPUT_PULLUP`, ke GND saat ditekan) |
| `PB_0` | **Tombol LEFT** | Active LOW (`INPUT_PULLUP`, ke GND saat ditekan) |
| `PB_13` | **Tombol RIGHT** | Active LOW (`INPUT_PULLUP`, ke GND saat ditekan) |
| `PB_12` | **Tombol OK** | Active LOW (`INPUT_PULLUP`, ke GND saat ditekan) |
| `PB_11` | **Tombol BACK** | Active LOW (`INPUT_PULLUP`, ke GND saat ditekan) |
| `PB_15` | **Cadangan** | Bebas / Tidak terpakai (*Reserved*) |

---

## 6. Library Arduino yang Diperlukan

Pasang library berikut melalui **Arduino Library Manager**:
1. **`STM32duino FreeRTOS`** (oleh STMicroelectronics)
2. **`U8g2`** (oleh olikraus)
3. **`OneWire`** (oleh Paul Stoffregen)
4. **`DallasTemperature`** (oleh Miles Burton)

---

## 7. Arsitektur Multitasking FreeRTOS

Firmware beroperasi menggunakan arsitektur *preemptive multitasking* dengan 6 task independen yang saling berkomunikasi secara aman melalui mutex dan queue:

| Nama Task | Periode | Prioritas | Ukuran Stack | Tanggung Jawab Utama |
|---|:---:|:---:|:---:|---|
| **TaskButton** | $15\text{ ms}$ | `4` (Tinggi) | 160 word | Scan GPIO tombol, debounce, deteksi hold/repeat, kirim event ke queue |
| **TaskGui** | $50\text{ ms}$ | `3` | 320 word | Konsumsi event tombol, proses state transition FSM, jadwal simpan EEPROM |
| **TaskOled** | $100\text{ ms}$ | `2` | 512 word | Ambil snapshot data di bawah mutex, render tampilan OLED via U8g2 bila dirty |
| **TaskWater** | $200\text{ ms}$ | `2` | 256 word | Sampling ADC TDS & Turbidity, moving average, eksekusi flash write tertunda, inferensi fuzzy |
| **TaskTemp** | $1000\text{ ms}$ | `1` | 192 word | Konversi DS18B20 non-blocking (tunggu 750 ms kooperatif via vTaskDelay) |
| **TaskDebug** | $1000\text{ ms}$ | `1` (Rendah) | 352 word | Kirim log telemetri sensor dan laporan stack high water mark ke Serial |

---

## 8. Metodologi Fuzzy Sugeno & Mode Evaluasi Kualitas Air

### 8.1. Mode 1: Air Minum & Higiene Sanitasi (Fuzzy Sugeno 64 Rule)
Sistem inferensi menggunakan **Fuzzy Sugeno Orde-0** dengan 3 variabel input yang masing-masing dibagi ke dalam **4 himpunan keanggotaan**:

#### Parameter Membership Functions (Fuzzifikasi):
1. **TDS Terkompensasi ($0 - 600\text{ mg/L}$)**:
   - `SL` : `trapmf [0, 0, 150, 225]`
   - `PS` : `trimf  [150, 225, 300]`
   - `PI` : `trimf  [225, 300, 450]`
   - `TL` : `trapmf [300, 450, 600, 600]`

$$\mu_{\text{TDS, SL}}(x) = \begin{cases} 1, & x \le 150 \\ \frac{225 - x}{225 - 150}, & 150 < x < 225 \\ 0, & x \ge 225 \end{cases} \quad
\mu_{\text{TDS, PS}}(x) = \begin{cases} 0, & x \le 150 \text{ atau } x \ge 300 \\ \frac{x - 150}{225 - 150}, & 150 < x \le 225 \\ \frac{300 - x}{300 - 225}, & 225 < x < 300 \end{cases}$$

$$\mu_{\text{TDS, PI}}(x) = \begin{cases} 0, & x \le 225 \text{ atau } x \ge 450 \\ \frac{x - 225}{300 - 225}, & 225 < x \le 300 \\ \frac{450 - x}{450 - 300}, & 300 < x < 450 \end{cases} \quad
\mu_{\text{TDS, TL}}(x) = \begin{cases} 0, & x \le 300 \\ \frac{x - 300}{450 - 300}, & 300 < x < 450 \\ 1, & x \ge 450 \end{cases}$$

2. **Kekeruhan / Turbidity ($0 - 25\text{ NTU}$)**:
   - `SL` : `trapmf [0, 0, 1.5, 2.25]`
   - `PS` : `trimf  [1.5, 2.25, 3.0]`
   - `PI` : `trimf  [2.25, 3.0, 4.5]`
   - `TL` : `trapmf [3.0, 4.5, 25.0, 25.0]`

$$\mu_{\text{Turb, SL}}(x) = \begin{cases} 1, & x \le 1.5 \\ \frac{2.25 - x}{2.25 - 1.5}, & 1.5 < x < 2.25 \\ 0, & x \ge 2.25 \end{cases} \quad
\mu_{\text{Turb, PS}}(x) = \begin{cases} 0, & x \le 1.5 \text{ atau } x \ge 3.0 \\ \frac{x - 1.5}{2.25 - 1.5}, & 1.5 < x \le 2.25 \\ \frac{3.0 - x}{3.0 - 2.25}, & 2.25 < x < 3.0 \end{cases}$$

$$\mu_{\text{Turb, PI}}(x) = \begin{cases} 0, & x \le 2.25 \text{ atau } x \ge 4.5 \\ \frac{x - 2.25}{3.0 - 2.25}, & 2.25 < x \le 3.0 \\ \frac{4.5 - x}{4.5 - 3.0}, & 3.0 < x < 4.5 \end{cases} \quad
\mu_{\text{Turb, TL}}(x) = \begin{cases} 0, & x \le 3.0 \\ \frac{x - 3.0}{4.5 - 3.0}, & 3.0 < x < 4.5 \\ 1, & x \ge 4.5 \end{cases}$$

3. **Deviasi Suhu $\Delta T = |T_{\text{air}} - T_{\text{udara}}| \ (0 - 10^\circ\text{C}$)**:
   - `SL` : `trapmf [0, 0, 1.0, 1.5]`
   - `PS` : `trimf  [1.0, 1.75, 2.5]`
   - `PI` : `trimf  [2.0, 2.75, 3.5]`
   - `TL` : `trapmf [3.0, 4.0, 10.0, 10.0]`

$$\mu_{\Delta T\text{, SL}}(x) = \begin{cases} 1, & x \le 1.0 \\ \frac{1.5 - x}{1.5 - 1.0}, & 1.0 < x < 1.5 \\ 0, & x \ge 1.5 \end{cases} \quad
\mu_{\Delta T\text{, PS}}(x) = \begin{cases} 0, & x \le 1.0 \text{ atau } x \ge 2.5 \\ \frac{x - 1.0}{1.75 - 1.0}, & 1.0 < x \le 1.75 \\ \frac{2.5 - x}{2.5 - 1.75}, & 1.75 < x < 2.5 \end{cases}$$

$$\mu_{\Delta T\text{, PI}}(x) = \begin{cases} 0, & x \le 2.0 \text{ atau } x \ge 3.5 \\ \frac{x - 2.0}{2.75 - 2.0}, & 2.0 < x \le 2.75 \\ \frac{3.5 - x}{3.5 - 2.75}, & 2.75 < x < 3.5 \end{cases} \quad
\mu_{\Delta T\text{, TL}}(x) = \begin{cases} 0, & x \le 3.0 \\ \frac{x - 3.0}{4.0 - 3.0}, & 3.0 < x < 4.0 \\ 1, & x \ge 4.0 \end{cases}$$

---

#### C. Logika Rule Base "Worst Parameter Wins" (MAX Severity) & Singleton $z$

Prinsip dasar pengawasan kualitas air konsumsi adalah **kepatuhan gabungan (conjunctive compliance)**: parameter terburuk menentukan status akhir air.

$$\text{Severity Output} = \max(\text{Severity}_{\text{Suhu}}, \text{Severity}_{\text{TDS}}, \text{Severity}_{\text{Turbidity}})$$
$$z = 1.0 - \frac{\text{Severity}}{3}$$

- **Severity 0 (SL)** $\rightarrow z_1 = 1.00$ (*Sangat Layak*)
- **Severity 1 (PS)** $\rightarrow z_2 = 0.67$ (*Perlu Proses Sedang*)
- **Severity 2 (PI)** $\rightarrow z_3 = 0.33$ (*Perlu Proses Intensif*)
- **Severity 3 (TL)** $\rightarrow z_4 = 0.00$ (*Tidak Lolos*)

---

#### D. Tabel Lengkap 64 Aturan Inferensi Fuzzy Sugeno

Kombinasi $4 \times 4 \times 4 = 64$ aturan inferensi yang diterapkan pada `kualitas_air.fis` dan `fuzzy_kualitas_air.c`:

| No | Suhu ($\Delta T$) | TDS | Turbidity | Output Kualitas Air | Singleton $z$ |
|:---:|:---:|:---:|:---:|:---|:---:|
| **R1** | SL | SL | SL | Sangat Layak | $1.00$ |
| **R2** | SL | SL | PS | Perlu Proses Sedang | $0.67$ |
| **R3** | SL | SL | PI | Perlu Proses Intensif | $0.33$ |
| **R4** | SL | SL | TL | Tidak Lolos | $0.00$ |
| **R5** | SL | PS | SL | Perlu Proses Sedang | $0.67$ |
| **R6** | SL | PS | PS | Perlu Proses Sedang | $0.67$ |
| **R7** | SL | PS | PI | Perlu Proses Intensif | $0.33$ |
| **R8** | SL | PS | TL | Tidak Lolos | $0.00$ |
| **R9** | SL | PI | SL | Perlu Proses Intensif | $0.33$ |
| **R10** | SL | PI | PS | Perlu Proses Intensif | $0.33$ |
| **R11** | SL | PI | PI | Perlu Proses Intensif | $0.33$ |
| **R12** | SL | PI | TL | Tidak Lolos | $0.00$ |
| **R13** | SL | TL | SL | Tidak Lolos | $0.00$ |
| **R14** | SL | TL | PS | Tidak Lolos | $0.00$ |
| **R15** | SL | TL | PI | Tidak Lolos | $0.00$ |
| **R16** | SL | TL | TL | Tidak Lolos | $0.00$ |
| **R17** | PS | SL | SL | Perlu Proses Sedang | $0.67$ |
| **R18** | PS | SL | PS | Perlu Proses Sedang | $0.67$ |
| **R19** | PS | SL | PI | Perlu Proses Intensif | $0.33$ |
| **R20** | PS | SL | TL | Tidak Lolos | $0.00$ |
| **R21** | PS | PS | SL | Perlu Proses Sedang | $0.67$ |
| **R22** | PS | PS | PS | Perlu Proses Sedang | $0.67$ |
| **R23** | PS | PS | PI | Perlu Proses Intensif | $0.33$ |
| **R24** | PS | PS | TL | Tidak Lolos | $0.00$ |
| **R25** | PS | PI | SL | Perlu Proses Intensif | $0.33$ |
| **R26** | PS | PI | PS | Perlu Proses Intensif | $0.33$ |
| **R27** | PS | PI | PI | Perlu Proses Intensif | $0.33$ |
| **R28** | PS | PI | TL | Tidak Lolos | $0.00$ |
| **R29** | PS | TL | SL | Tidak Lolos | $0.00$ |
| **R30** | PS | TL | PS | Tidak Lolos | $0.00$ |
| **R31** | PS | TL | PI | Tidak Lolos | $0.00$ |
| **R32** | PS | TL | TL | Tidak Lolos | $0.00$ |
| **R33** | PI | SL | SL | Perlu Proses Intensif | $0.33$ |
| **R34** | PI | SL | PS | Perlu Proses Intensif | $0.33$ |
| **R35** | PI | SL | PI | Perlu Proses Intensif | $0.33$ |
| **R36** | PI | SL | TL | Tidak Lolos | $0.00$ |
| **R37** | PI | PS | SL | Perlu Proses Intensif | $0.33$ |
| **R38** | PI | PS | PS | Perlu Proses Intensif | $0.33$ |
| **R39** | PI | PS | PI | Perlu Proses Intensif | $0.33$ |
| **R40** | PI | PS | TL | Tidak Lolos | $0.00$ |
| **R41** | PI | PI | SL | Perlu Proses Intensif | $0.33$ |
| **R42** | PI | PI | PS | Perlu Proses Intensif | $0.33$ |
| **R43** | PI | PI | PI | Perlu Proses Intensif | $0.33$ |
| **R44** | PI | PI | TL | Tidak Lolos | $0.00$ |
| **R45** | PI | TL | SL | Tidak Lolos | $0.00$ |
| **R46** | PI | TL | PS | Tidak Lolos | $0.00$ |
| **R47** | PI | TL | PI | Tidak Lolos | $0.00$ |
| **R48** | PI | TL | TL | Tidak Lolos | $0.00$ |
| **R49** | TL | SL | SL | Tidak Lolos | $0.00$ |
| **R50** | TL | SL | PS | Tidak Lolos | $0.00$ |
| **R51** | TL | SL | PI | Tidak Lolos | $0.00$ |
| **R52** | TL | SL | TL | Tidak Lolos | $0.00$ |
| **R53** | TL | PS | SL | Tidak Lolos | $0.00$ |
| **R54** | TL | PS | PS | Tidak Lolos | $0.00$ |
| **R55** | TL | PS | PI | Tidak Lolos | $0.00$ |
| **R56** | TL | PS | TL | Tidak Lolos | $0.00$ |
| **R57** | TL | PI | SL | Tidak Lolos | $0.00$ |
| **R58** | TL | PI | PS | Tidak Lolos | $0.00$ |
| **R59** | TL | PI | PI | Tidak Lolos | $0.00$ |
| **R60** | TL | PI | TL | Tidak Lolos | $0.00$ |
| **R61** | TL | TL | SL | Tidak Lolos | $0.00$ |
| **R62** | TL | TL | PS | Tidak Lolos | $0.00$ |
| **R63** | TL | TL | PI | Tidak Lolos | $0.00$ |
| **R64** | TL | TL | TL | Tidak Lolos | $0.00$ |

---

#### E. Defuzzifikasi Weighted Average & Ambang Batas Klasifikasi

Nilai keluaran tegas (*crisp output*) $Z$ dihitung dengan metode *Weighted Average*:
$$Z = \frac{\sum_{i=1}^{64} w_i \times z_i}{\sum_{i=1}^{64} w_i}, \quad w_i = \min(\mu_{\text{TDS}, i}, \mu_{\text{Turb}, i}, \mu_{\Delta T, i})$$

Skor akhir $Z$ ($0.00 - 1.00$) diklasifikasikan ke dalam 4 tingkatan status mutu:

| Rentang Skor Defuzzifikasi $Z$ | Kategori Mutu Air | Badge OLED | Pesan Tindakan |
|:---:|---|:---:|---|
| **$Z \ge 0.83$** | **Sangat Layak** | `S.LAYAK` | Air Sangat Layak |
| **$0.50 \le Z < 0.83$** | **Perlu Proses Sedang** | `P.SED` | Perlu proses sedang |
| **$0.17 \le Z < 0.50$** | **Perlu Proses Intensif** | `P.INT` | Perlu proses intensif |
| **$Z < 0.17$** | **Tidak Lolos** | `T.LOLOS` | Tidak layak digunakan |

#### F. Compliance Hard Gate (Pengunci Permenkes No. 2/2023):
$$\text{Jika } (\text{TDS} \ge 300\text{ mg/L}) \lor (\text{Turbidity} \ge 3.0\text{ NTU}) \lor (\Delta T > 3.0^\circ\text{C}) \implies \text{Status} = \text{T.LOLOS}, \ Z = 0.00$$

---

#### G. Contoh Kasus Perhitungan Numerik Langkah-Demi-Langkah

Misal sampel air diuji dengan kondisi:
- Suhu Udara Manual: $T_{\text{udara}} = 28.0^\circ\text{C}$, Suhu Air Terukur: $T_{\text{air}} = 29.5^\circ\text{C} \implies \Delta T = 1.5^\circ\text{C}$
- TDS Mentah: $218.0\text{ ppm} \implies \text{TDS}_{\text{terkompensasi}} = \frac{218.0}{1.0 + 0.02(29.5 - 25.0)} = 200.0\text{ mg/L}$
- Turbidity: $2.0\text{ NTU}$

**Langkah 1: Fuzzifikasi**
- $\text{TDS} = 200.0$: $\mu_{\text{SL}} = \frac{225 - 200}{225 - 150} = 0.333, \quad \mu_{\text{PS}} = \frac{200 - 150}{225 - 150} = 0.667, \quad \mu_{\text{PI}} = 0, \quad \mu_{\text{TL}} = 0$
- $\text{Turbidity} = 2.0$: $\mu_{\text{SL}} = \frac{2.25 - 2.0}{2.25 - 1.5} = 0.333, \quad \mu_{\text{PS}} = \frac{2.0 - 1.5}{2.25 - 1.5} = 0.667, \quad \mu_{\text{PI}} = 0, \quad \mu_{\text{TL}} = 0$
- $\Delta T = 1.5^\circ\text{C}$: $\mu_{\text{SL}} = 0, \quad \mu_{\text{PS}} = \frac{1.5 - 1.0}{1.75 - 1.0} = 0.667, \quad \mu_{\text{PI}} = 0, \quad \mu_{\text{TL}} = 0$

**Langkah 2: Evaluasi Aturan Aktif**
- **R21** (PS Suhu, PS TDS, SL Turb) $\rightarrow w_{21} = \min(0.667, 0.667, 0.333) = 0.333 \implies z_{21} = 0.67$
- **R22** (PS Suhu, PS TDS, PS Turb) $\rightarrow w_{22} = \min(0.667, 0.667, 0.667) = 0.667 \implies z_{22} = 0.67$

**Langkah 3: Defuzzifikasi**
$$Z = \frac{(0.333 \times 0.67) + (0.667 \times 0.67)}{0.333 + 0.667} = \frac{0.223 + 0.447}{1.000} = 0.67$$

**Langkah 4: Keputusan Akhir**
- $Z = 0.67$ berada pada rentang $0.50 \le Z < 0.83 \implies$ **`P.SED` (Perlu Proses Sedang)**.
- Seluruh parameter berada di bawah batas hard gate ($\text{TDS} < 300, \text{Turb} < 3.0, \Delta T \le 3.0$), sehingga skor $Z=0.67$ sah berlaku.

---

### 8.2. Mode 2: Pemandian / Kolam (Non-Fuzzy Threshold Checker)
Berdasarkan Permenkes No. 2/2023 Tabel 10 dan standar kolam renang:
- **Suhu Air**: $16.0 - 35.0^\circ\text{C}$ $\rightarrow$ `[LAYAK]`, di luar itu $\rightarrow$ `[TDK]`
- **Kekeruhan**: $< 0.5\text{ NTU}$ $\rightarrow$ `[LAYAK]`, di atas itu $\rightarrow$ `[TDK]`
- **TDS**: **Bypass / Tidak Diatur** (nilai sensor tetap ditampilkan dengan tag `[BYP]`)
- **Status Akhir**: `LAYAK` jika kedua parameter lolos, `TIDAK LAYAK` jika salah satu gagal.

---

### 8.3. Kalibrasi Turbidity Dua Titik
Rumus yang digunakan untuk menghitung slope linier akurat:
$$\text{Slope} = \frac{\text{NTU}_{\text{standar}}}{V_{\text{jernih}} - V_{\text{standar}}}$$
$$\text{NTU} = (V_{\text{jernih}} - V_{\text{ukur}}) \times \text{Slope}$$

---

## 9. Antarmuka GUI & Katalog Lengkap Layar OLED 1.3" (128x64)

Layout layar dibagi menjadi 3 zona: **Header (12 px)**, **Konten Utama (44 px)**, **Status Bar (8 px)**.

### 9.1. Splash Screen (Tampilan Booting - 2 Detik)
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

### 9.2. Menu Utama (Pemilihan Mode Uji Air)
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

### 9.3. Input Suhu Udara Manual (Khusus Mode 1)
Pengguna mengatur suhu lingkungan sekitar sebelum mencelupkan sensor ($10.0 - 45.0^\circ\text{C}$).
```text
+---------------------------------------------------+
| Input Suhu Udara                                  |
|---------------------------------------------------|
| Udara: [ 28.0 C ]                                 |
| UP/DN: +/-0.1 C                                   |
| LF/RT: +/-1.0 C                                   |
|                                                   |
|---------------------------------------------------|
| OK:Mulai                               BACK:Batal |
+---------------------------------------------------+
```

---

### 9.4. Screen Tunggu & Stabilisasi Suhu Probe
Sistem memverifikasi kestabilan suhu probe DS18B20 ($3\times$ sampel variasi $\le 0.2^\circ\text{C}$).

**Tampilan Stabilisasi Normal:**
```text
+---------------------------------------------------+
| MODE: AIR MINUM                                   |
|---------------------------------------------------|
|                                                   |
|   Membaca Sensor...                               |
|   [||||||||||||||||||................]            |
|   Stabil: 2/3                             60%     |
|                                                   |
|---------------------------------------------------|
| Tunggu stabil.                         BACK:Batal |
+---------------------------------------------------+
```

**Tampilan Bila Timeout (60 Detik):**
```text
+---------------------------------------------------+
| MODE: AIR MINUM                                   |
|---------------------------------------------------|
| Suhu belum stabil                                 |
| OK: lanjut manual                                 |
|                                                   |
|                                                   |
|---------------------------------------------------|
| OK:Lanjut                              BACK:Batal |
+---------------------------------------------------+
```

---

### 9.5. Layar Pengukuran Mode 1: Air Minum & Higiene (Three-Page View)

**Halaman 1/3: Dashboard Hasil Sensor & Skor Mutu**
```text
+---------------------------------------------------+
| Air Minum (1/3)                                   |
|---------------------------------------------------|
| Air:31.0C dT:3.0                                  |
| TDS  : 280.0 ppm                                  |
| Turb : 1.5 NTU                                    |
| Skor : 0.67 [P.SED]                               |
|---------------------------------------------------|
| DN:Detail                               BACK:Menu |
+---------------------------------------------------+
```

**Halaman 2/3: Diagnosis Tiap Parameter**
```text
+---------------------------------------------------+
| Diagnosis (2/3)                                   |
|---------------------------------------------------|
| Mutu: P.SED                                       |
| dT  : P.Sed                                       |
| TDS : S.Layak                                     |
| Turb: S.Layak                                     |
|---------------------------------------------------|
| DN:Saran                                BACK:Menu |
+---------------------------------------------------+
```

**Halaman 3/3: Saran Tindakan Spesifik**
```text
+---------------------------------------------------+
| Saran (3/3)                                       |
|---------------------------------------------------|
| Suhu: sesuaikan suhu                              |
| Uji ulang air                                     |
|                                                   |
|                                                   |
|---------------------------------------------------|
| UP:Diagnosis                            BACK:Menu |
+---------------------------------------------------+
```

---

### 9.6. Layar Pengukuran Mode 2: Pemandian / Kolam (Three-Page View)

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

---

### 9.7. Menu Kalibrasi Sensor, Sub-Menu, & Live Monitor

Sistem memisahkan secara tegas antara **Live Monitor** dan **Kalibrasi**:
- **Live Monitor**: Digunakan untuk mengecek sinyal sensor mentah (*raw data*) secara real-time sebelum proses kalibrasi atau konversi matematika (`ADC`, `Volt`, `Raw`, `Offset`, `Status`). Halaman ini tidak menampilkan nilai hasil konversi (ppm / NTU) karena pembacaan belum tentu valid sebelum kalibrasi selesai.
- **Kalibrasi**: Memakai larutan acuan untuk menghitung rumus/faktor konversi baru dan menyimpannya secara permanen ke Flash EEPROM.

#### Glosarium & Arti Singkatan:
- **`ADC`** : *Analog-to-Digital Converter* (nilai digital mentah 12-bit ADC STM32, rentang 0–4095).
- **`Volt`** : Tegangan sensor hasil konversi ADC (0.00–3.30 V).
- **`Raw`** : Pembacaan sensor mentah sebelum kompensasi suhu atau offset kalibrasi.
- **`Offset`** : Koreksi suhu (°C) yang ditambahkan ke pembacaan mentah DS18B20.
- **`TDS`** : *Total Dissolved Solids* (kandungan zat padat terlarut).
- **`Turb`** : *Turbidity* (kekeruhan cairan).
- **`ppm`** : *Parts per million* (satuan konsentrasi TDS, setara mg/L).
- **`NTU`** : *Nephelometric Turbidity Unit* (satuan kekeruhan air).
- **`V0`** : Tegangan keluaran sensor kekeruhan pada air jernih / 0 NTU ($V_{\text{jernih}}$).
- **`VStd`** : Tegangan keluaran sensor kekeruhan pada larutan standar ($V_{\text{standar}}$).
- **`Std`** : Nilai kekeruhan acuan larutan standar custom (NTU).
- **`dT`** : Deviasi/selisih suhu ($|\text{Suhu Air} - \text{Suhu Udara}|$).
- **`SL`** : Sangat Layak (kategori mutu air tertinggi).
- **`PS`** : Perlu Proses Sedang.
- **`PI`** : Perlu Proses Intensif.
- **`TL`** : Tidak Lolos (tidak memenuhi standar regulasi).

---

#### A. Struktur Menu Utama Kalibrasi Sensor
```text
+---------------------------------------------------+
| Kalibrasi Sensor                                  |
|---------------------------------------------------|
| > TDS                                             |
|   Turbidity                                       |
|   Suhu                                            |
|   Reset Pabrik                                    |
|---------------------------------------------------|
| OK:Pilih                                BACK:Menu |
+---------------------------------------------------+
```

#### B. Sub-Menu Tiap Sensor (TDS / Turbidity / Suhu)
Setiap parameter sensor memiliki sub-menu tersendiri:
```text
+---------------------------------------------------+
| TDS / Turbidity / Suhu                            |
|---------------------------------------------------|
| > Kalibrasi                                       |
|   Live Monitor                                    |
|                                                   |
|---------------------------------------------------|
| OK:Pilih                                BACK:Menu |
+---------------------------------------------------+
```

---

#### C. Halaman Live Monitor Raw Data (1 Layar Murni)

**1. Live Monitor TDS:**
```text
+---------------------------------------------------+
| Live TDS                                          |
|---------------------------------------------------|
| ADC   : 1245                                      |
| Volt  : 1.00 V                                    |
| Suhu  : 27.5 C                                    |
| Status: OK                                        |
|---------------------------------------------------|
|                                         BACK:Menu |
+---------------------------------------------------+
```

**2. Live Monitor Turbidity:**
```text
+---------------------------------------------------+
| Live Turb                                         |
|---------------------------------------------------|
| ADC   : 3210                                      |
| Volt  : 2.59 V                                    |
| Suhu  : 27.5 C                                    |
| Status: OK                                        |
|---------------------------------------------------|
|                                         BACK:Menu |
+---------------------------------------------------+
```

**3. Live Monitor Suhu:**
```text
+---------------------------------------------------+
| Live Suhu                                         |
|---------------------------------------------------|
| Raw   : 27.4 C                                    |
| Offset: +0.1 C                                    |
| Air   : 27.5 C                                    |
| Status: OK                                        |
|---------------------------------------------------|
|                                         BACK:Menu |
+---------------------------------------------------+
```

---

#### D. Halaman Wizard Kalibrasi Interaktif

**1. Kalibrasi TDS ($\pm 1\text{ ppm}$ step):**
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

**2. Kalibrasi Turbidity 2-Titik Wizard ($\pm 5\text{ NTU}$ custom step):**
*Langkah 1 (Air Jernih 0 NTU):*
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
*Langkah 2 (Larutan Standar Custom):*
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

**3. Kalibrasi Suhu Offset ($\pm 0.1^\circ\text{C}$ step):**
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

**4. Konfirmasi Pengaman Reset Pabrik:**
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

### 9.8. Menu Pengaturan OLED (Tersimpan Permanen di EEPROM)

**Daftar Pengaturan OLED:**
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

**Mode Edit Nilai Kecerahan/Kontras (`*`):**
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

### 9.9. Layar Informasi Sistem (ABOUT — 2 Halaman)

**Halaman 1/2: Software & Regulasi:**
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

**Halaman 2/2: Hardware & Memori:**
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

Saat perangkat dinyalakan, fungsi `setup()` di `main.ino` mengeksekusi uji coba validasi otomatis 1x pada `Serial` (115200 baud):

```text
========================================
    VALIDASI AUTOMATIS FIRMWARE         
========================================
Air Minum (Ideal)     : 1.00 [S.LAYAK]
Air Minum (1 Batas)   : 0.67 [P.SED]
Pemandian (28C, 0.3NTU): [LAYAK]
Pemandian (28C, 2.5NTU): [TDK LAYAK]
Turb 2 titik (2.50V): 50.0 NTU
========================================
```

---

## 11. Panduan Kalibrasi Sensor

*Tips: Anda dapat memeriksa sinyal sensor terlebih dahulu melalui **Live Monitor** sebelum melakukan kalibrasi.*

### A. Kalibrasi Suhu (DS18B20 Offset) — Lakukan Pertama
1. Tempatkan probe DS18B20 bersama termometer laboratorium presisi di dalam wadah air yang sama.
2. Masuk ke **Menu Utama** $\rightarrow$ **Kalibrasi Sensor** $\rightarrow$ **Suhu** $\rightarrow$ **Kalibrasi**.
3. Bandingkan nilai **Suhu Raw** pada layar dengan termometer acuan.
4. Tekan tombol `LEFT` atau `RIGHT` (atau `UP`/`DOWN`) untuk menyelaraskan nilai **Offset** ($\pm 0.1^\circ\text{C}$).
5. Tekan tombol **OK** untuk menyimpan nilai offset ke Flash EEPROM.

### B. Kalibrasi TDS (1-Point Solution)
1. Siapkan larutan standar TDS acuan (misal **707 ppm**).
2. Celupkan probe TDS ke dalam larutan dan tunggu pembacaan stabil.
3. Masuk ke **Menu Utama** $\rightarrow$ **Kalibrasi Sensor** $\rightarrow$ **TDS** $\rightarrow$ **Kalibrasi**.
4. Tekan tombol `UP` atau `DOWN` untuk menyelaraskan nilai **Target** di layar hingga sama dengan larutan standar (`707 ppm`) dengan ketelitian **$\pm 1\text{ ppm}$**.
5. Tekan tombol **OK** untuk menghitung $K$-Factor baru dan menyimpannya ke Flash EEPROM.

### C. Kalibrasi Turbidity (2-Point Custom Wizard)
1. Masuk ke **Menu Utama** $\rightarrow$ **Kalibrasi Sensor** $\rightarrow$ **Turbidity** $\rightarrow$ **Kalibrasi**.
2. **Titik 1 (0 NTU)**: Celupkan sensor ke air aquades murni. Amati nilai voltase hingga stabil, lalu tekan **OK**.
3. **Titik 2 (Standar Custom)**: Bilas sensor, lalu celupkan ke satu larutan standar yang dimiliki (misal **100 NTU**, **500 NTU**, atau **1000 NTU**). Tekan `UP`/`DOWN` untuk menyelaraskan angka `Std` di layar dengan label nilai larutan (kenaikan **$\pm 5\text{ NTU}$**).
4. Tekan tombol **OK**. Firmware akan mengunci kedua titik, menghitung slope akurat sensor ($\text{Slope} = \frac{\text{Std}}{V_0 - V_{\text{Std}}}$), dan menyimpannya ke Flash EEPROM.

---

## 12. Cara Build dan Upload

1. Buka folder `main` proyek ini pada **Arduino IDE** atau **VS Code + STM32duino**.
2. Pilih Board: **Generic STM32F4 series** $\rightarrow$ **BlackPill F401CC**.
3. Pastikan library pendukung terinstall: `STM32duino FreeRTOS`, `U8g2`, `OneWire`, `DallasTemperature`.
4. Compile dan upload firmware ke STM32 BlackPill via ST-Link V2 atau DFU USB.
5. Buka Serial Monitor pada baud rate `115200` untuk mengamati log startup validasi dan telemetri debug.
