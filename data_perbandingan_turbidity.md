# Arsip Data Perbandingan Turbidity Lama

**Status: arsip, tidak digunakan untuk kalibrasi firmware saat ini.**

Sumber data: `Perbandingan_Turbidity.xlsx`. Data pada workbook ini diambil sebelum dataset Lab Bante terbaru tersedia. Beberapa titik tidak konsisten dengan hubungan ADC-NTU pada data terbaru, sehingga tidak boleh digabungkan dengan regresi aktif.

Dataset aktif, rumus firmware, dan prosedur pembaruan berada pada:

- `sampel_baru_turbidity.md` untuk data sumber terbaru.
- `kalibrasiturbidy.md` untuk metode, perhitungan, dan batas klaim.
- `turbidity compare.xlsx` untuk workbook kalibrasi aktif.

| No. | Sampel                              | Turbidity alat lab (NTU) | ADC alat | Turbidity alat sendiri (NTU) | Status / catatan                                                                  |
| --: | ----------------------------------- | -----------------------: | -------: | ---------------------------- | --------------------------------------------------------------------------------- |
|   1 | Aquades Lab Kes                     |                     2,96 |      613 | 303,8 NTU                    | Nilai ADC dan alat lab tersedia.                                                  |
|   2 | Aquades Fabian                      |                     3,07 |      371 | 325 NTU                      | Nilai ADC dan alat lab tersedia.                                                  |
|   3 | Aquades + Formazin 1 (kadar tinggi) |                      568 |      962 | 273 NTU                      | Nilai lab`0` perlu divalidasi karena tidak selaras dengan label kadar tinggi.   |
|   4 | Aquades + Formazin 2 (kadar sedang) |                      175 |        - | TBA                          | Nilai turbidity alat sendiri belum tersedia.                                      |
|   5 | Aquades + Formazin 3 (kadar rendah) |                      120 |      684 | 175 NTU                      | Nilai lab`568` perlu divalidasi karena tidak selaras dengan label kadar rendah. |
|   6 | Air minum Amidis                    |                     3,17 |      384 | 120 NTU                      | Nilai turbidity alat lab belum tersedia.                                          |
|   7 | Air keran                           |                      3,3 |      561 | 297 NTU                      | Nilai ADC dan alat lab tersedia.                                                  |

## Data Tambahan Dari Spreadsheet

Bagian berikut dicatat terpisah karena posisi/arti kolomnya pada spreadsheet belum dapat dipastikan. Jangan gunakan untuk kalibrasi sebelum validasi manual.

| Keterangan pada spreadsheet | Nilai yang tercatat                                                        |
| --------------------------- | -------------------------------------------------------------------------- |
| Sampel`10 mL`             | Turbidity lab:`-`; ADC: `120 NTU`; turbidity alat sendiri: `297 NTU` |
| Baris Formazin 1            | Jumlah aquades:`0`; jumlah Formazin 500 NTU: `33`                      |
| Baris Formazin 2            | Jumlah aquades:`5,25`; jumlah Formazin 500 NTU: `5,25`                 |
| Baris Formazin 3            | Jumlah aquades:`31`; jumlah Formazin 500 NTU: `31`                     |
| Baris`full`               | Jumlah aquades:`31`; jumlah Formazin 500 NTU: `31`                     |

## Catatan Arsip

- Nilai `-`, `TBA`, atau teks lain dianggap belum ada data numerik.
- Kolom `Turbidity alat sendiri (NTU)` adalah pembacaan rumus lama dan bukan nilai referensi.
- Data ini boleh dipakai sebagai riwayat pengembangan, tetapi bukan untuk memperbarui `TURBIDITY_SLOPE` atau `TURBIDITY_INTERCEPT`.
