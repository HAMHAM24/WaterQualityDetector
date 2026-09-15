# Kalibrasi Turbidity Berbasis Regresi Linear ADC STM32

## 1. Tujuan

Dokumen ini menjelaskan cara nilai ADC dari sensor turbidity dikonversi menjadi NTU pada alat berbasis STM32F401CCU6 BlackPill. Dokumen ini dapat dipakai untuk menjelaskan metode kalibrasi kepada client dan sebagai dasar jawaban saat sidang.

Kalibrasi memakai pembacaan **ADC alat sendiri** sebagai input dan **Turbidity Lab Bante** sebagai nilai referensi. Nilai `Turbidity (Alatku)` yang lama tidak digunakan untuk membentuk rumus, karena tujuan kalibrasi adalah menyesuaikan alat sendiri dengan alat referensi.

## 2. Konfigurasi Hardware


| Parameter                  |                    Nilai |
| ---------------------------- | -------------------------: |
| Mikrokontroler             |  STM32F401CCU6 BlackPill |
| Catu dan referensi ADC     |                    3,3 V |
| Resolusi ADC               |                   12-bit |
| Rentang ADC                |                   0-4095 |
| Pin input sensor turbidity |                      PA1 |
| Filter firmware            | Moving average 20 sampel |

ADC 12-bit menghasilkan angka digital dari tegangan analog sensor. Secara teori:

```text
tegangan ADC = ADC / 4095 x 3,3 V
```

Namun, firmware sengaja memakai ADC mentah untuk rumus kalibrasi. Hal ini menghindari galat pembulatan nilai volt dan membuat rumus sesuai langsung dengan data yang direkam saat pengujian.

## 3. Data Referensi yang Digunakan

Berikut delapan pasangan data dari `sampel_baru_turbidity.md` yang dipakai dalam regresi.


| No. | ADC alat (x) | Turbidity Lab Bante, NTU (y) |
| ----: | -------------: | -----------------------------: |
|   1 |          710 |                         0,00 |
|   2 |          718 |                         9,44 |
|   3 |          726 |                        19,47 |
|   4 |          732 |                        28,97 |
|   5 |          742 |                        38,72 |
|   6 |          748 |                        47,45 |
|   7 |          852 |                       177,30 |
|   8 |         1084 |                       468,00 |

Kondisi sensor tidak dicelup air (`ADC 637`) tidak digunakan karena bukan sampel air dengan nilai referensi Lab Bante. Data tersebut hanya berguna sebagai indikasi diagnostik sensor kosong atau tidak tercelup.

## 4. Metode: Regresi Linear Kuadrat Terkecil

Metode yang dipakai adalah **regresi linear sederhana** dengan metode **kuadrat terkecil** (*ordinary least squares*). Metode ini mencari satu garis lurus yang paling mendekati seluruh titik data.

Bentuk umum garis:

```text
y = m x + b
```

Dalam alat ini:

```text
y = NTU hasil referensi Lab Bante
x = ADC mentah STM32 yang sudah difilter
m = slope atau kemiringan garis
b = intercept atau titik potong terhadap sumbu y
```

Setiap titik memiliki residual atau galat:

```text
residual_i = y_referensi_i - y_prediksi_i
```

Kuadrat terkecil memilih `m` dan `b` yang meminimalkan jumlah kuadrat residual:

```text
SSE = jumlah (y_referensi_i - (m x_i + b))^2
```

Residual dikuadratkan agar galat positif dan negatif tidak saling menghapus, serta galat besar diberi penalti lebih besar.

### 4.1 Rumus Slope dan Intercept

Untuk `n` titik data, rata-rata ADC dan rata-rata NTU dihitung terlebih dahulu:

```text
x_bar = jumlah x_i / n
y_bar = jumlah y_i / n
```

Kemudian slope dihitung dengan:

```text
m = jumlah ((x_i - x_bar)(y_i - y_bar)) / jumlah ((x_i - x_bar)^2)
```

Setelah slope diketahui, intercept dihitung dengan:

```text
b = y_bar - m x_bar
```

### 4.2 Perhitungan Detail dari Data Pengujian

Jumlah data adalah `n = 8`. Dari tabel pada Bagian 3 diperoleh jumlah seluruh nilai ADC dan NTU referensi:

```text
sum x_i = 710 + 718 + 726 + 732 + 742 + 748 + 852 + 1084
        = 6312

sum y_i = 0,00 + 9,44 + 19,47 + 28,97 + 38,72 + 47,45 + 177,30 + 468,00
        = 789,35
```

Rata-rata kedua variabel adalah:

```text
x_bar = sum x_i / n
      = 6312 / 8
      = 789

y_bar = sum y_i / n
      = 789,35 / 8
      = 98,66875
```

Untuk menghitung slope, setiap data dikurangi rata-ratanya. Dua jumlah yang diperlukan adalah:

```text
Sxy = sum ((x_i - x_bar)(y_i - y_bar))
    = 141916,47

Sxx = sum ((x_i - x_bar)^2)
    = 113384
```

Contoh kontribusi titik pertama, yaitu `ADC = 710` dan `NTU = 0,00`:

```text
x_1 - x_bar = 710 - 789 = -79
y_1 - y_bar = 0,00 - 98,66875 = -98,66875

(x_1 - x_bar)(y_1 - y_bar) = (-79)(-98,66875) = 7794,83125
(x_1 - x_bar)^2 = (-79)^2 = 6241
```

Perhitungan yang sama dilakukan untuk seluruh delapan titik, kemudian semua kontribusinya dijumlahkan menjadi `Sxy` dan `Sxx` di atas. Slope diperoleh dengan substitusi:

```text
m = Sxy / Sxx
  = 141916,47 / 113384
  = 1,2516445883017
```

Nilai ini dibulatkan menjadi:

```text
m = 1,251644588
```

Kemudian intercept dihitung menggunakan rata-rata dan slope yang belum dibulatkan:

```text
b = y_bar - m x_bar
  = 98,66875 - (1,2516445883017 x 789)
  = 98,66875 - 987,547580170042
  = -888,878830170042
```

Nilai ini dibulatkan menjadi:

```text
b = -888,878830170
```

Dengan demikian, persamaan regresi sebelum pembulatan firmware adalah:

```text
NTU = (1,2516445883017 x ADC) - 888,878830170042
```

Firmware memakai enam angka di belakang koma agar ringkas, tetapi selisih pembulatan ini sangat kecil:

```text
m = 1,251644588...
b = -888,878830170...
```

Firmware membulatkan angka secara aman ke enam angka di belakang koma:

```cpp
constexpr float TURBIDITY_SLOPE     = 1.251645f;
constexpr float TURBIDITY_INTERCEPT = -888.878830f;
```

Huruf `f` berarti literal tersebut bertipe `float`. STM32 menggunakan `float` 32-bit untuk efisiensi memori dan waktu komputasi.

Jadi persamaan yang dipakai firmware adalah:

```text
NTU = (1,251645 x ADC) - 888,878830
```

Nilai intercept negatif bukan berarti turbidity dapat bernilai negatif. Intercept adalah konsekuensi posisi garis regresi pada sumbu matematis. Firmware menjepit hasil negatif menjadi `0 NTU` karena NTU negatif tidak mempunyai makna fisik.

## 5. Contoh Perhitungan Manual

Misalnya ADC hasil moving average adalah `852`.

```text
NTU = (1,251645 x 852) - 888,878830
NTU = 1066,401540 - 888,878830
NTU = 177,522710 NTU
```

Prediksi titik tersebut adalah `177,52 NTU`. Nilai Lab Bante pada titik ini adalah `177,30 NTU`, sehingga galatnya sekitar `-0,22 NTU`.

Contoh titik air jernih dengan ADC `710`:

```text
NTU = (1,251645 x 710) - 888,878830
NTU = -0,210880 NTU
```

Hasil tersebut dijepit firmware menjadi `0 NTU`.

## 6. Evaluasi Hasil Kalibrasi


| Metrik                  |     Hasil | Arti                                                                                    |
| ------------------------- | ----------: | ----------------------------------------------------------------------------------------- |
| R kuadrat (R^2)         |  0,999976 | Hampir seluruh variasi NTU referensi pada data ini dapat dijelaskan oleh perubahan ADC. |
| RMSE                    | 0,735 NTU | Besar galat prediksi tipikal pada delapan titik data.                                   |
| Galat terbesar          | 1,645 NTU | Terjadi pada titik ADC 732 dan referensi 28,97 NTU.                                     |
| Rentang tervalidasi     | 0-468 NTU | Rentang nilai Lab Bante yang benar-benar diuji.                                         |
| Rentang ADC tervalidasi |  710-1084 | Rentang ADC yang mempunyai data pembanding.                                             |

Nilai `R^2` yang mendekati 1 menunjukkan garis linear cocok sangat baik terhadap delapan titik yang tersedia. Ini bukan berarti sensor pasti akurat pada semua kondisi atau semua NTU. Pengulangan pengukuran dan titik tambahan tetap diperlukan untuk mengukur kestabilan alat.

## 7. Implementasi Firmware STM32

Alur program adalah:

```text
Sensor turbidity analog
        |
        v
PA1 STM32 membaca ADC 12-bit
        |
        v
Moving average 20 sampel
        |
        v
NTU = 1,251645 x ADC - 888,878830
        |
        v
Jika NTU < 0, jadikan 0
        |
        v
Display, evaluasi ambang, dan Fuzzy
```

Koefisien tersimpan di `main/config.h`:

```cpp
constexpr float TURBIDITY_SLOPE     = 1.251645f;
constexpr float TURBIDITY_INTERCEPT = -888.878830f;
constexpr uint16_t TURBIDITY_CALIBRATED_ADC_MIN = 710;
constexpr uint16_t TURBIDITY_CALIBRATED_ADC_MAX = 1084;
```

Fungsi konversi di `main/sensors.cpp`:

```cpp
float sensors_turbidityAdcToNtu(float raw) {
    const float ntu = TURBIDITY_SLOPE * raw + TURBIDITY_INTERCEPT;
    return ntu < 0.0f ? 0.0f : ntu;
}
```

Fungsi pembaruan sensor menggunakan ADC hasil filter, bukan ADC sesaat:

```cpp
const uint16_t raw = sensors_readTurbidityRaw();
const float filteredRaw = pushSampleAndAverage(..., raw);
const float ntu = sensors_turbidityAdcToNtu(filteredRaw);
```

Penggunaan filter penting karena noise satu atau dua hitungan ADC dapat mengubah hasil NTU. Dengan slope sekitar `1,25 NTU per ADC`, perubahan 1 ADC secara teori setara sekitar `1,25 NTU`.

Koefisien diperbarui offline melalui `kalibrasi_turbidity.py`, kemudian hasilnya dimasukkan ke `main/config.h` dan firmware di-upload ulang. Wizard dua titik tidak digunakan karena akan mengganti dasar delapan titik referensi dengan hanya dua titik.

Menu **Kalibrasi Sensor -> Turbidity** langsung membuka **Live Turbidity**. Halaman tersebut menampilkan ADC moving average, volt untuk diagnosis rangkaian, NTU hasil regresi, dan `Stat` validitas pembacaan. `Stat` bukan status kualitas air atau hasil Fuzzy.

| Stat | Kondisi | Arti |
|---|---|---|
| `ERROR` | ADC mentah 0 atau 4095 | Periksa sensor, kabel, dan rangkaian ADC. |
| `BAWAH RENTANG` | ADC terfilter kurang dari 710 | Nilai dijepit ke 0 NTU, tetapi belum memiliki pembanding Lab Bante di bawah titik tersebut. |
| `TERKALIBRASI` | ADC terfilter 710-1084 | Berada pada rentang 0-468 NTU yang diuji terhadap Lab Bante. |
| `ESTIMASI >468` | ADC terfilter lebih dari 1084 | Rumus menghitung hasil, tetapi belum diverifikasi Lab Bante. |

## 8. Batas Klaim Pengukuran


| Kondisi             | Perlakuan yang benar                                                                                  |
| --------------------- | ------------------------------------------------------------------------------------------------------- |
| ADC kurang dari 710 | Hasil dijepit ke 0 NTU; sebutkan sebagai di bawah rentang data kalibrasi.                             |
| ADC 710 sampai 1084 | Hasil berada dalam rentang kalibrasi yang diuji, yaitu 0-468 NTU.                                     |
| ADC lebih dari 1084 | Rumus masih dapat menghitung nilai, tetapi hasil adalah estimasi di luar rentang kalibrasi.           |
| ADC 4095 atau 0     | Periksa kabel, sensor, dan rangkaian karena dapat mengindikasikan kondisi jenuh, putus, atau korslet. |

Contoh: ADC `1200` menghasilkan sekitar `613 NTU` secara matematis. Nilai tersebut tidak boleh diklaim terkalibrasi sebelum dibandingkan kembali dengan Lab Bante pada rentang sekitar `500-650 NTU`.

## 9. Cara Memperbarui Kalibrasi

File `kalibrasi_turbidity.py` menghitung ulang regresi dari data yang tersedia. Masukkan pasangan data baru pada list berikut:

```python
ADC = [710, 718, 726, 732, 742, 748, 852, 1084]
NTU_LAB_BANTE = [0.00, 9.44, 19.47, 28.97, 38.72, 47.45, 177.30, 468.00]
```

Satu indeks harus selalu berasal dari sampel yang sama. Setelah data ditambahkan, jalankan:

```text
python kalibrasi_turbidity.py
```

Untuk menghitung tanpa membuka grafik, misalnya saat verifikasi melalui terminal, jalankan:

```text
python kalibrasi_turbidity.py --no-plot
```

Program akan menampilkan nilai slope, intercept, `R^2`, RMSE, residual per titik, dan koefisien baru untuk firmware. Salin hasilnya ke konstanta `TURBIDITY_SLOPE` dan `TURBIDITY_INTERCEPT` di `main/config.h`.

Untuk kalibrasi versi berikutnya, ambil minimal tiga sampai lima pembacaan ADC yang stabil pada setiap larutan, lalu gunakan rata-rata ADC. Prioritaskan titik yang belum padat, misalnya `75-100 NTU`, `125-150 NTU`, `250-300 NTU`, serta `500-650 NTU` bila alat akan digunakan di atas 468 NTU.

## 10. Jawaban Ringkas untuk Sidang

**Mengapa memakai ADC, bukan volt?**

Karena ADC adalah nilai asli yang dibaca STM32. Konversi ke volt tidak menambah informasi dan dapat menambah galat pembulatan. Tegangan referensi 3,3 V tetap penting karena menentukan skala ADC dan memastikan input PA1 aman.

**Mengapa memakai regresi linear?**

Karena delapan titik menunjukkan hubungan ADC dan NTU yang sangat linear (`R^2 = 0,999976`). Regresi menggunakan seluruh titik, sehingga lebih representatif daripada memilih hanya dua titik. Model polynomial tidak dipakai karena titik data masih terbatas dan polynomial lebih mudah mengikuti noise atau menghasilkan perilaku tidak realistis di luar data.

**Dari mana slope dan intercept berasal?**

Dari perhitungan kuadrat terkecil terhadap seluruh pasangan `ADC alat` dan `NTU Lab Bante`. Script Python proyek menghitungnya otomatis dengan rumus slope dan intercept pada Bagian 4.1.

**Apakah alat bisa membaca di atas 468 NTU?**

Firmware dapat menghitung nilai di atas 468 NTU, tetapi nilai tersebut adalah ekstrapolasi. Klaim kalibrasi saat ini dibatasi pada 0-468 NTU sampai ada pembanding Lab Bante pada rentang yang lebih tinggi.

**Apa keterbatasan penelitian ini?**

Setiap titik data saat ini belum memiliki pengulangan yang cukup untuk menghitung repeatability. Karena itu, hasil ini tepat disebut kalibrasi awal atau kalibrasi versi 1, bukan validasi akhir seluruh rentang sensor.
