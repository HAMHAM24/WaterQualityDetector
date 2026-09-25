# Model Sementara Kalibrasi Turbidity

## 1. Status Data

Dokumen ini menjelaskan model sementara pada branch `special`. Model dibentuk dari data ADC lama yang dikoreksi secara matematis, ditambah dua **asumsi** untuk air kran: `1692 ADC = 0 NTU` dan `1709 ADC = 1,30 NTU`. Nilai asumsi dan ADC hasil koreksi bukan hasil pengukuran Lab Bante. Dokumen ini tidak boleh digunakan untuk mengklaim kalibrasi tervalidasi.

## 2. Data dan Koreksi ADC Lama

Data lama diambil saat baterai hampir habis. ADC lama diskalakan memakai pembacaan air kran baru `1671,8 ADC` terhadap titik ADC lama `710`:

```text
faktor koreksi = 1671,8 / 710
               = 2,354647887324

ADC estimasi = ADC lama x 2,354647887324
```

Koreksi proporsional ini adalah asumsi. Koreksi tersebut belum dibuktikan dengan pengukuran tegangan catu, tegangan referensi ADC, atau pengulangan semua larutan.

## 3. Aturan Program Aktif

Firmware memakai ADC hasil moving average dan menjalankan tiga bagian berikut:

```text
ADC <= 1692:
    NTU = 0

1692 < ADC < 1709:
    NTU = 1,30 x (ADC - 1692) / (1709 - 1692)

ADC >= 1709:
    NTU = 1,30 + (0,514082 x (ADC - 1709))
    NTU = (0,514082 x ADC) - 877,266138
```

Segmen tengah adalah interpolasi kontinu dari `0` ke `1,30 NTU`. Segmen atas mempertahankan slope regresi estimasi lama (`0,514082 NTU/ADC`), tetapi intercept digeser agar hasil pada ADC `1709` tepat `1,30 NTU` tanpa loncatan.

Konstanta firmware di `main/config.h`:

```cpp
constexpr float TURBIDITY_ZERO_ADC = 1692.0f;
constexpr float TURBIDITY_TAP_ADC = 1709.0f;
constexpr float TURBIDITY_TAP_NTU = 1.30f;
constexpr float TURBIDITY_SLOPE = 0.514082f;
constexpr float TURBIDITY_INTERCEPT = -877.266138f;
constexpr uint16_t TURBIDITY_CALIBRATED_ADC_MIN = 1692;
constexpr uint16_t TURBIDITY_CALIBRATED_ADC_MAX = 2552;
```

## 4. Contoh Output Model

| ADC terfilter | Bagian model | Output NTU |
| ------------: | ------------ | ---------: |
| 1254 | Di bawah nol | 0,00 |
| 1691 | Di bawah nol | 0,00 |
| 1692 | Titik nol | 0,00 |
| 1693 | Interpolasi | 0,08 |
| 1694 | Interpolasi | 0,15 |
| 1700 | Interpolasi | 0,61 |
| 1708 | Interpolasi | 1,22 |
| 1709 | Titik sambung | 1,30 |
| 1723,60 | Slope regresi estimasi | 8,81 |
| 2006,16 | Slope regresi estimasi | 154,06 |
| 2552,44 | Slope regresi estimasi | 434,90 |

## 5. Tabel Riwayat Data

Kolom `Lab Bante` adalah nilai referensi dari sesi lama. Kolom `Prediksi model aktif` dihitung oleh program baru dan sengaja tidak dipakai untuk mengubah nilai referensi lama.

| No. | ADC lama | ADC estimasi | Lab Bante (NTU) | Prediksi model aktif (NTU) |
| ---: | -------: | -----------: | ---------------: | -------------------------: |
| 1 | 710,0 | 1671,80 | 0,00 | 0,00 |
| 2 | 718,0 | 1690,64 | 9,44 | 0,00 |
| 3 | 726,0 | 1709,47 | 19,47 | 1,54 |
| 4 | 732,0 | 1723,60 | 28,97 | 8,81 |
| 5 | 742,0 | 1747,15 | 38,72 | 20,91 |
| 6 | 748,0 | 1761,28 | 47,45 | 28,17 |
| 7 | 754,0 | 1775,40 | 16,26 | 35,44 |
| 8 | 779,0 | 1834,27 | 34,43 | 65,70 |
| 9 | 788,0 | 1855,46 | 32,28 | 76,59 |
| 10 | 790,0 | 1860,17 | 24,00 | 79,01 |
| 11 | 795,5 | 1873,12 | 40,44 | 85,67 |
| 12 | 797,0 | 1876,65 | 18,22 | 87,49 |
| 13 | 821,0 | 1933,17 | 59,98 | 116,54 |
| 14 | 832,0 | 1959,07 | 87,22 | 129,85 |
| 15 | 852,0 | 2006,16 | 177,30 | 154,06 |
| 16 | 877,0 | 2065,03 | 150,90 | 184,33 |
| 17 | 1084,0 | 2552,44 | 468,00 | 434,90 |

## 6. Evaluasi yang Tepat

Regresi 17 titik sebelum penyesuaian titik air kran menghasilkan `R2 = 0,901655` dan `RMSE = 34,198 NTU`. Metrik tersebut tidak berlaku sebagai metrik model aktif karena model aktif memakai batas nol, interpolasi, dan intercept yang digeser.

Jika model aktif dibandingkan secara retrospektif dengan 17 nilai referensi lama, hasilnya sekitar `R2 = 0,888924` dan `RMSE = 36,344 NTU`. Angka ini hanya menunjukkan ketidakcocokan terhadap data estimasi lama, bukan akurasi nyata pada sensor yang sekarang.

## 7. Batas Penggunaan

- `1692 -> 0 NTU` dan `1709 -> 1,30 NTU` adalah asumsi untuk air kran, bukan pembacaan instrumen referensi.
- ADC di bawah atau sama dengan `1692` selalu ditampilkan sebagai `0 NTU`.
- Status `TERKALIBRASI` pada firmware berarti berada dalam rentang model `1692-2552`; status tersebut bukan bukti verifikasi laboratorium.
- ADC di atas `2552` tetap dihitung tetapi statusnya ekstrapolasi.
- Kalibrasi final harus menggunakan sampel yang sama dengan pembacaan Lab Bante, catu daya stabil, dan minimal tiga sampai lima pembacaan stabil per larutan.

## 8. Reproduksi

Jalankan perhitungan riwayat regresi dan model firmware:

```text
python kalibrasi_turbidity.py --no-plot
```

Skrip membedakan regresi 17 titik historis dari model bertahap yang aktif di firmware.
