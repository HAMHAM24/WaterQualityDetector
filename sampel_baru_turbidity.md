# Data Sampel Turbidity Terbaru

Dokumen ini adalah transkrip data pengujian terbaru dari `turbidity compare.xlsx`. Sumber resmi untuk kalibrasi adalah pasangan **ADC Alatku** dan **Turbidity Lab Bante**. Nilai lama pada kolom `Turbidity Alatku` hanya dicatat sebagai pembacaan sebelum rumus regresi diterapkan.

## Data Kalibrasi yang Dipakai

Delapan baris berikut memiliki pasangan ADC dan NTU referensi lengkap, sehingga dipakai oleh `kalibrasi_turbidity.py` dan firmware.

| No. | Sampel | Aquades (mL) | Formazin 500 NTU (mL) | ADC Alatku | NTU Lab Bante |
|---:|---|---:|---:|---:|---:|
| 1 | Full aquades | penuh | 0,00 | 710 | 0,00 |
| 2 | Aquades + Formazin | 14,70 | 0,30 | 718 | 9,44 |
| 3 | Aquades + Formazin | 14,40 | 0,60 | 726 | 19,47 |
| 4 | Aquades + Formazin | 14,01 | 0,90 | 732 | 28,97 |
| 5 | Aquades + Formazin | 13,80 | 1,20 | 742 | 38,72 |
| 6 | Aquades + Formazin | 13,31 | 1,69 | 748 | 47,45 |
| 7 | Aquades + Formazin | 9,00 | 6,00 | 852 | 177,30 |
| 8 | Full Formazin | 0,00 | penuh | 1084 | 468,00 |

Rentang data yang benar-benar diuji adalah ADC `710-1084` atau `0-468 NTU`.

## Catatan Pembacaan Alat Sebelum Regresi

| No. asal | ADC Alatku | NTU Alatku lama | Volt Alatku | Keterangan |
|---:|---:|---:|---:|---|
| 1 | 710 | 211,5 | 0,55 | Pembacaan sebelum rumus regresi; tidak dipakai sebagai referensi. |
| 2 | 718 | 206,9 | - | Pembacaan sebelum rumus regresi; tidak dipakai sebagai referensi. |
| 3 | 726 | 202,0 | - | Pembacaan sebelum rumus regresi; tidak dipakai sebagai referensi. |
| 4 | 732 | 197,4 | - | Pembacaan sebelum rumus regresi; tidak dipakai sebagai referensi. |
| 5 | 742 | 192,6 | - | Pembacaan sebelum rumus regresi; tidak dipakai sebagai referensi. |
| 6 | 748 | 188,4 | - | Pembacaan sebelum rumus regresi; tidak dipakai sebagai referensi. |
| 7 | 852 | 192,0 | - | Pembacaan sebelum rumus regresi; tidak dipakai sebagai referensi. |
| 9 | 1084 | 0,0 | 0,88 | Pembacaan sebelum rumus regresi; tidak dipakai sebagai referensi. |

Kolom `ADC Lab Bante` kosong karena alat Lab Bante menjadi referensi nilai NTU, bukan sumber ADC.

## Data Diagnostik yang Tidak Dipakai

| Kondisi | ADC Alatku | NTU Alatku lama | Volt Alatku | Alasan tidak dipakai |
|---|---:|---:|---:|---|
| Sensor tidak dicelup air | 637 | 257,0 | 0,51 | Tidak memiliki nilai NTU referensi Lab Bante; hanya untuk diagnosis sensor kosong/tidak tercelup. |

## Penggunaan Data

1. Gunakan hanya delapan baris pada bagian **Data Kalibrasi yang Dipakai** untuk menghitung regresi.
2. Jalankan `python kalibrasi_turbidity.py --no-plot` setelah menambahkan data baru.
3. Jangan mencampurkan data dari `Perbandingan_Turbidity.xlsx`; workbook tersebut adalah arsip pengujian lama dan bukan dasar firmware saat ini.
