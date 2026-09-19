# Kalibrasi Turbidity Berbasis Regresi Linear ADC STM32

## 1. Tujuan

Dokumen ini menjelaskan konversi nilai ADC sensor turbidity menjadi NTU pada alat berbasis STM32F401CCU6 BlackPill. Kalibrasi menggunakan **ADC Alatku** sebagai input dan **Turbidity Alat Lab Bante** sebagai nilai referensi.

Nilai `Turbidity (Alatku)` pada file Excel tidak dipakai untuk membentuk rumus, karena alat tersebut bukan instrumen referensi. Dokumen ini memakai data gabungan dari `kalibrasiturbidy.md` versi sebelumnya dan `Data kalibrasi.xlsx`.

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

Rumus kalibrasi memakai ADC mentah yang sudah difilter, bukan nilai volt. ADC merupakan nilai asli yang direkam oleh STM32 sehingga konversi volt tidak diperlukan untuk menghitung NTU.

## 3. Data Referensi Gabungan

Data berikut disusun berdasarkan ADC secara menaik. Setiap baris adalah satu pasangan ADC alat dan nilai referensi Lab Bante.


| No. | Sumber               | Sampel        | ADC alat (x) | Turbidity Lab Bante, NTU (y) |
| ----: | ---------------------- | --------------- | -------------: | -----------------------------: |
|   1 | Referensi sebelumnya | Referensi 1   |          710 |                         0,00 |
|   2 | Referensi sebelumnya | Referensi 2   |          718 |                         9,44 |
|   3 | Referensi sebelumnya | Referensi 3   |          726 |                        19,47 |
|   4 | Referensi sebelumnya | Referensi 4   |          732 |                        28,97 |
|   5 | Referensi sebelumnya | Referensi 5   |          742 |                        38,72 |
|   6 | Referensi sebelumnya | Referensi 6   |          748 |                        47,45 |
|   7 | Excel                | Sampel 1      |          754 |                        16,26 |
|   8 | Excel                | Sampel 5      |          779 |                        34,43 |
|   9 | Excel                | Sampel 4      |          788 |                        32,28 |
|  10 | Excel                | Sampel 3      |          790 |                        24,00 |
|  11 | Excel                | Sampel 6      |        795,5 |                        40,44 |
|  12 | Excel                | Sampel 2      |          797 |                        18,22 |
|  13 | Excel                | Sampel 7      |          821 |                        59,98 |
|  14 | Excel                | Sampel 8      |          832 |                        87,22 |
|  15 | Kedua sumber         | Sample extra  |          852 |                       177,30 |
|  16 | Excel                | Sampel 9      |          877 |                       150,90 |
|  17 | Kedua sumber         | Full Formazin |         1084 |                       468,00 |

### 3.1 Aturan Pengolahan Data

- Data `Full Aquades` dari Excel (`ADC 916`, `0,00 NTU`) tidak digunakan karena anomali terhadap titik air jernih lain dan menghasilkan ketidaksesuaian besar dengan pola data gabungan.
- Kondisi sensor tidak dicelup air (`ADC 637`) tidak digunakan karena bukan sampel air dan tidak mempunyai nilai pembanding Lab Bante.
- `Sample extra` (`852`, `177,30 NTU`) dan `Full Formazin` (`1084`, `468,00 NTU`) tercatat pada kedua sumber. Masing-masing dihitung satu kali agar pengukuran yang sama tidak memperoleh bobot ganda pada regresi.
- ADC Sampel 6 pada Excel dicatat sebagai rentang `791-800`. Perhitungan menggunakan titik tengahnya, yaitu `795,5 ADC`, sebagai estimasi. Pengukuran ulang dengan beberapa pembacaan stabil tetap diperlukan.

## 4. Metode: Regresi Linear Kuadrat Terkecil

Kalibrasi menggunakan regresi linear sederhana dengan metode kuadrat terkecil (*ordinary least squares*). Metode ini mencari satu garis lurus yang paling mendekati seluruh pasangan data.

```text
y = m x + b
```

Keterangan:

```text
y = NTU referensi Lab Bante
x = ADC mentah STM32 setelah moving average
m = slope atau kemiringan garis
b = intercept atau titik potong terhadap sumbu y
```

Residual setiap titik dihitung sebagai:

```text
residual_i = y_referensi_i - y_prediksi_i
```

Regresi memilih `m` dan `b` yang meminimalkan jumlah kuadrat residual:

```text
SSE = jumlah (y_referensi_i - (m x_i + b))^2
```

### 4.1 Rumus Slope dan Intercept

Untuk `n` titik data:

```text
x_bar = jumlah x_i / n
y_bar = jumlah y_i / n

m = jumlah ((x_i - x_bar)(y_i - y_bar)) / jumlah ((x_i - x_bar)^2)
b = y_bar - m x_bar
```

### 4.2 Perhitungan Data Gabungan

Jumlah data adalah `n = 17`.

```text
sum x_i = 13545,5
sum y_i = 1253,08

x_bar = 13545,5 / 17
      = 796,794117647

y_bar = 1253,08 / 17
      = 73,710588235
```

Nilai jumlah deviasi yang diperlukan untuk regresi adalah:

```text
Sxy = jumlah ((x_i - x_bar)(y_i - y_bar))
    = 150585,777058824

Sxx = jumlah ((x_i - x_bar)^2)
    = 124401,529411765
```

Slope dan interceptnya adalah:

```text
m = Sxy / Sxx
  = 1,210481718118

b = y_bar - m x_bar
  = -890,794124280201
```

Persamaan regresi sebelum pembulatan firmware:

```text
NTU = (1,210481718118 x ADC) - 890,794124280201
```

Untuk konstanta `float` firmware, koefisien dapat dibulatkan menjadi enam angka di belakang koma:

```cpp
constexpr float TURBIDITY_SLOPE = 1.210482f;
constexpr float TURBIDITY_INTERCEPT = -890.794124f;
```

Sehingga persamaan firmware menjadi:

```text
NTU = (1,210482 x ADC) - 890,794124
```

Intercept negatif adalah konsekuensi matematis dari garis regresi dan bukan berarti NTU negatif mempunyai makna fisik. Firmware harus menjepit hasil negatif menjadi `0 NTU`.

## 5. Contoh Perhitungan Manual

Misalnya ADC hasil moving average adalah `852`:

```text
NTU = (1,210482 x 852) - 890,794124
NTU = 1031,330664 - 890,794124
NTU = 140,536540 NTU
```

Nilai Lab Bante pada titik tersebut adalah `177,30 NTU`, sehingga residualnya:

```text
residual = 177,30 - 140,54
         = 36,76 NTU
```

Contoh ADC `710`:

```text
NTU = (1,210482 x 710) - 890,794124
NTU = -31,351904 NTU
```

Hasil tersebut harus dijepit firmware menjadi `0 NTU`.

## 6. Evaluasi Hasil Kalibrasi


| Metrik                  |      Hasil | Arti                                                                          |
| ------------------------- | -----------: | ------------------------------------------------------------------------------- |
| Jumlah data             |         17 | Pasangan data unik setelah penyaringan dan penghapusan duplikat.              |
| R kuadrat (R2)          |   0,901655 | Hubungan ADC dan NTU bersifat positif, tetapi variasi data masih cukup besar. |
| RMSE                    | 34,198 NTU | Besar galat prediksi tipikal terhadap seluruh data gabungan.                  |
| Galat terbesar          | 55,740 NTU | Terjadi pada ADC 797 dengan referensi 18,22 NTU.                              |
| Rentang ADC tervalidasi |   710-1084 | Rentang ADC yang memiliki pembanding Lab Bante.                               |
| Rentang NTU tervalidasi |  0-468 NTU | Rentang nilai Lab Bante yang diuji.                                           |

Model ini memakai seluruh data gabungan yang telah dipilih, sehingga mencerminkan variasi pada kedua sesi data. Nilai `R2` yang lebih rendah dan RMSE yang lebih tinggi daripada kalibrasi sebelumnya menunjukkan bahwa hasil pembacaan belum konsisten pada semua titik, terutama pada rentang sekitar `754-877 ADC`.

Karena itu, rumus ini dapat disebut kalibrasi gabungan atau kalibrasi awal, tetapi belum cukup untuk mengklaim akurasi tinggi. Setiap larutan perlu diukur ulang minimal tiga sampai lima kali dalam kondisi sensor, pencahayaan, wadah, pengadukan, dan waktu stabilisasi yang sama.

## 7. Implementasi Firmware STM32

Alur program:

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
NTU = 1,210482 x ADC - 890,794124
|
v
Jika NTU < 0, jadikan 0
|
v
Display, evaluasi ambang, dan Fuzzy
```

Apabila rumus baru akan digunakan pada firmware, konstanta yang perlu diterapkan di `main/config.h` adalah:

```cpp
constexpr float TURBIDITY_SLOPE = 1.210482f;
constexpr float TURBIDITY_INTERCEPT = -890.794124f;
constexpr uint16_t TURBIDITY_CALIBRATED_ADC_MIN = 710;
constexpr uint16_t TURBIDITY_CALIBRATED_ADC_MAX = 1084;
```

Fungsi konversi harus memakai ADC hasil filter:

```cpp
float sensors_turbidityAdcToNtu(float raw) {
    const float ntu = TURBIDITY_SLOPE * raw + TURBIDITY_INTERCEPT;
    return ntu < 0.0f ? 0.0f : ntu;
}
```

### 7.1 Resolusi Perubahan ADC

Slope kalibrasi adalah `1,210482 NTU per ADC`. Artinya, setiap kenaikan `1 ADC` setara kira-kira `1,21 NTU` menurut model ini.

Perubahan kecil pada pembacaan ADC dapat langsung mengubah hasil NTU. Oleh karena itu, firmware perlu memakai moving average 20 sampel sebelum konversi agar output lebih stabil. Ketelitian pengukuran nyata tetap dibatasi oleh variasi data kalibrasi; resolusi `1,21 NTU per ADC` tidak berarti hasil pengukuran pasti akurat sampai `1,21 NTU`.

### 7.2 Contoh Output Prediksi


| ADC input | Perhitungan model                         | Output setelah penjepitan | Status        |
| ----------: | ------------------------------------------- | --------------------------: | --------------- |
|       735 | `(1,210482 x 735) - 890,794124 = -1,09`   |                  0,00 NTU | DALAM RENTANG |
|       736 | `(1,210482 x 736) - 890,794124 = 0,12`    |                  0,12 NTU | DALAM RENTANG |
|       748 | `(1,210482 x 748) - 890,794124 = 14,65`   |                 14,65 NTU | DALAM RENTANG |
|       754 | `(1,210482 x 754) - 890,794124 = 21,91`   |                 21,91 NTU | DALAM RENTANG |
|       761 | `(1,210482 x 761) - 890,794124 = 30,38`   |                 30,38 NTU | DALAM RENTANG |
|       852 | `(1,210482 x 852) - 890,794124 = 140,54`  |                140,54 NTU | DALAM RENTANG |
|      1084 | `(1,210482 x 1084) - 890,794124 = 421,37` |                421,37 NTU | DALAM RENTANG |
|      1200 | `(1,210482 x 1200) - 890,794124 = 561,78` |                561,78 NTU | EKSTRAPOLASI |

Status memakai rentang ADC: kurang dari 710 adalah `BAWAH RENTANG`,
710-1084 adalah `DALAM RENTANG`, dan lebih dari 1084 adalah `EKSTRAPOLASI`.
ADC mentah 0 atau 4095 diprioritaskan sebagai `ERROR`.
ADC 735 tetap dalam rentang data walaupun hasilnya dijepit ke nol.
Ekstrapolasi tidak berarti output pasti di atas 468 NTU; pada ADC 1084
prediksinya hanya 421,37 NTU. Status dalam rentang bukan jaminan akurasi.

Contoh keluaran pada serial monitor atau display untuk ADC terfilter `754`:

```text
ADC filter : 754
NTU model  : 21,91 NTU
Status     : DALAM RENTANG
Catatan    : Kenaikan 1 ADC setara sekitar 1,21 NTU pada model ini.
```

## 8. Batas Klaim Pengukuran


| Kondisi             | Perlakuan yang benar                                                                                      |
| --------------------- | ----------------------------------------------------------------------------------------------------------- |
| ADC kurang dari 710 | Hasil dijepit ke 0 NTU, tetapi berada di bawah rentang data pembanding.                                   |
| ADC 710 sampai 1084 | Berada dalam rentang ADC yang diuji terhadap Lab Bante, dengan RMSE gabungan 34,198 NTU.                  |
| ADC lebih dari 1084 | Rumus masih dapat menghitung nilai, tetapi hasil merupakan ekstrapolasi dan belum diverifikasi Lab Bante. |
| ADC 0 atau 4095     | Periksa sensor, kabel, dan rangkaian ADC karena dapat mengindikasikan putus, korslet, atau kondisi jenuh. |

Contoh: ADC `1200` menghasilkan sekitar `561,78 NTU` secara matematis. Nilai tersebut tidak boleh diklaim terkalibrasi sebelum dibandingkan dengan Lab Bante pada rentang tersebut.

## 9. Cara Memperbarui Kalibrasi

Data yang digunakan dalam regresi ini dapat direpresentasikan sebagai berikut:

```python
ADC = [710, 718, 726, 732, 742, 748, 754, 779, 788, 790, 795.5,
       797, 821, 832, 852, 877, 1084]
NTU_LAB_BANTE = [0.00, 9.44, 19.47, 28.97, 38.72, 47.45, 16.26,
                 34.43, 32.28, 24.00, 40.44, 18.22, 59.98, 87.22,
                 177.30, 150.90, 468.00]
```

Satu indeks ADC harus selalu berasal dari sampel yang sama dengan indeks NTU Lab Bante. Data duplikat tidak boleh dimasukkan kembali kecuali merupakan pengukuran ulang yang independen dan memang sengaja ingin diberi bobot tambahan.

Untuk kalibrasi berikutnya, ambil minimal tiga sampai lima pembacaan ADC stabil untuk setiap larutan dan gunakan nilai rata-ratanya. Prioritaskan pengukuran ulang pada rentang yang saat ini paling bervariasi, yaitu sekitar `15-180 NTU`.

## 10. Jawaban Ringkas untuk Sidang

**Mengapa memakai ADC, bukan volt?**

Karena ADC adalah nilai asli yang dibaca STM32. Konversi ke volt tidak menambah informasi untuk regresi dan dapat menambah galat pembulatan.

**Mengapa memakai regresi linear?**

Regresi linear menghasilkan satu persamaan yang memakai seluruh 17 pasangan data unik. Hasilnya menunjukkan hubungan positif ADC-NTU, tetapi data gabungan masih bervariasi, sehingga hasil harus disampaikan bersama nilai `R2 = 0,901655` dan `RMSE = 34,198 NTU`.

**Mengapa Full Aquades tidak digunakan?**

Titik `916 ADC = 0,00 NTU` tidak sejalan dengan titik air jernih lain pada data gabungan. Titik tersebut diperlakukan sebagai anomali dan dikeluarkan agar tidak mendistorsi regresi.

**Mengapa data yang sama dari dua sumber tidak dihitung dua kali?**

Pasangan `852 ADC = 177,30 NTU` dan `1084 ADC = 468,00 NTU` adalah data yang sama pada kedua sumber. Menghitungnya dua kali akan memberi bobot ganda pada satu pengukuran tanpa menambah bukti baru.

**Apa keterbatasan kalibrasi ini?**

Setiap titik belum memiliki pengulangan yang cukup untuk menghitung repeatability. Variasi antar data menyebabkan galat prediksi masih besar, sehingga kalibrasi ini tepat disebut kalibrasi gabungan awal dan perlu pengukuran ulang yang lebih terkontrol.
