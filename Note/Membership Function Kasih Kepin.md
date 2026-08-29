# Rancangan Membership Function Fuzzy Sugeno

Bagian ini digunakan untuk melengkapi subbab perancangan algoritma Fuzzy Sugeno pada dokumen `Skripsi Oli 1.docx`. Berdasarkan isi dokumen, sistem yang dirancang adalah alat deteksi dini kualitas fisik air menggunakan tiga parameter input, yaitu TDS, kekeruhan, dan suhu. Output sistem menggunakan model Sugeno orde nol dengan empat konstanta keluaran: Sangat Layak, Perlu Proses Sedang, Perlu Proses Intensif, dan Tidak Lolos.



## 1. Himpunan Keanggotaan Input TDS

Variabel TDS memiliki rentang kerja 0 sampai 600 mg/L. Batas 300 mg/L digunakan sebagai ambang utama baku mutu, sedangkan nilai di atas 300 mg/L diperlakukan sebagai area tidak memenuhi syarat. Untuk memberi peringatan dini sebelum mencapai ambang regulasi, area sebelum 300 mg/L dibagi menjadi kategori Sangat Layak, Perlu Proses Sedang, dan Perlu Proses Intensif.

| Kode | Kategori | Tipe MF | Parameter |
|---|---|---|---|
| SL | Sangat Layak | trapmf | [0, 0, 150, 225] |
| PS | Perlu Proses Sedang | trimf | [150, 225, 300] |
| PI | Perlu Proses Intensif | trimf | [225, 300, 450] |
| TL | Tidak Lolos | trapmf | [300, 450, 600, 600] |

Persamaan derajat keanggotaan TDS:

```text
mu_TDS_SL(x) =
1,                         x <= 150
(225 - x) / (225 - 150),   150 < x < 225
0,                         x >= 225

mu_TDS_PS(x) =
0,                         x <= 150 atau x >= 300
(x - 150) / (225 - 150),   150 < x <= 225
(300 - x) / (300 - 225),   225 < x < 300

mu_TDS_PI(x) =
0,                         x <= 225 atau x >= 450
(x - 225) / (300 - 225),   225 < x <= 300
(450 - x) / (450 - 300),   300 < x < 450

mu_TDS_TL(x) =
0,                         x <= 300
(x - 300) / (450 - 300),   300 < x < 450
1,                         x >= 450
```

## 2. Himpunan Keanggotaan Input Kekeruhan

Variabel kekeruhan memiliki rentang kerja 0 sampai 25 NTU. Ambang 3 NTU digunakan sebagai batas baku mutu utama. Nilai mendekati 3 NTU dikategorikan sebagai kondisi yang perlu perhatian karena sudah berada di sekitar batas aman.

| Kode | Kategori | Tipe MF | Parameter |
|---|---|---|---|
| SL | Sangat Layak | trapmf | [0, 0, 1.5, 2.25] |
| PS | Perlu Proses Sedang | trimf | [1.5, 2.25, 3] |
| PI | Perlu Proses Intensif | trimf | [2.25, 3, 4.5] |
| TL | Tidak Lolos | trapmf | [3, 4.5, 25, 25] |

Persamaan derajat keanggotaan kekeruhan:

```text
mu_NTU_SL(x) =
1,                           x <= 1.5
(2.25 - x) / (2.25 - 1.5),   1.5 < x < 2.25
0,                           x >= 2.25

mu_NTU_PS(x) =
0,                           x <= 1.5 atau x >= 3
(x - 1.5) / (2.25 - 1.5),    1.5 < x <= 2.25
(3 - x) / (3 - 2.25),        2.25 < x < 3

mu_NTU_PI(x) =
0,                           x <= 2.25 atau x >= 4.5
(x - 2.25) / (3 - 2.25),     2.25 < x <= 3
(4.5 - x) / (4.5 - 3),       3 < x < 4.5

mu_NTU_TL(x) =
0,                           x <= 3
(x - 3) / (4.5 - 3),         3 < x < 4.5
1,                           x >= 4.5
```

## 3. Himpunan Keanggotaan Input Suhu

Variabel suhu tidak langsung menggunakan suhu air absolut, tetapi menggunakan deviasi suhu terhadap suhu udara sekitar. Hal ini mengikuti batas baku mutu suhu air yang dinyatakan sebagai T_udara +-3 derajat C. Rentang kerja deviasi suhu dibuat 0 sampai 10 derajat C.

| Kode | Kategori | Tipe MF | Parameter |
|---|---|---|---|
| SL | Sangat Layak | trapmf | [0, 0, 1, 1.5] |
| PS | Perlu Proses Sedang | trimf | [1, 1.75, 2.5] |
| PI | Perlu Proses Intensif | trimf | [2, 2.75, 3.5] |
| TL | Tidak Lolos | trapmf | [3, 4, 10, 10] |

Persamaan derajat keanggotaan deviasi suhu:

```text
mu_DT_SL(x) =
1,                         x <= 1
(1.5 - x) / (1.5 - 1),     1 < x < 1.5
0,                         x >= 1.5

mu_DT_PS(x) =
0,                         x <= 1 atau x >= 2.5
(x - 1) / (1.75 - 1),      1 < x <= 1.75
(2.5 - x) / (2.5 - 1.75),  1.75 < x < 2.5

mu_DT_PI(x) =
0,                         x <= 2 atau x >= 3.5
(x - 2) / (2.75 - 2),      2 < x <= 2.75
(3.5 - x) / (3.5 - 2.75),  2.75 < x < 3.5

mu_DT_TL(x) =
0,                         x <= 3
(x - 3) / (4 - 3),         3 < x < 4
1,                         x >= 4
```

## 4. Membership Function Output Sugeno

Karena metode yang digunakan adalah Fuzzy Sugeno orde nol, output tidak berbentuk kurva fuzzy, melainkan konstanta tegas atau singleton. Nilai output dibuat dalam rentang 0 sampai 1 agar mudah diproses oleh mikrokontroler.

| Kode | Kategori Output | Singleton z |
|---|---|---|
| SL | Sangat Layak | 1.00 |
| PS | Perlu Proses Sedang | 0.67 |
| PI | Perlu Proses Intensif | 0.33 |
| TL | Tidak Lolos | 0.00 |

Nilai akhir dihitung menggunakan metode weighted average:

```text
Z = (sum(w_i * z_i)) / (sum(w_i))
```

dengan:

```text
Z   = nilai akhir defuzzifikasi
w_i = firing strength aturan ke-i
z_i = konstanta singleton aturan ke-i
```

## 5. Dasar Penentuan Batas Membership Function

Penentuan batas membership function dilakukan dengan prinsip peringatan dini. Ambang baku mutu tidak hanya digunakan sebagai batas biner layak atau tidak layak, tetapi dijadikan pusat transisi antar kategori fuzzy. Dengan cara ini, sistem dapat memberikan informasi bertahap sebelum parameter benar-benar melewati batas regulasi.

Pada parameter TDS, nilai 300 mg/L digunakan sebagai titik batas utama. Nilai jauh di bawah ambang dikategorikan Sangat Layak, nilai yang mendekati ambang dikategorikan Perlu Proses Sedang, nilai pada sekitar ambang dikategorikan Perlu Proses Intensif, dan nilai di atas ambang dikategorikan Tidak Lolos.

Pada parameter kekeruhan, nilai 3 NTU digunakan sebagai titik batas utama. Semakin dekat nilai kekeruhan terhadap 3 NTU, semakin tinggi tingkat proses yang dibutuhkan. Nilai di atas 3 NTU masuk ke area Tidak Lolos karena telah melampaui batas kualitas fisik yang digunakan pada rancangan sistem.

Pada parameter suhu, sistem menggunakan deviasi suhu terhadap suhu udara sekitar. Nilai deviasi sampai 3 derajat C digunakan sebagai batas utama. Apabila deviasi suhu semakin mendekati atau melewati 3 derajat C, status keanggotaan bergeser dari Sangat Layak menuju Tidak Lolos.



