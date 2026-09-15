# Arsip Kalibrasi Turbidity Hardcode Lama

**Status: sudah tidak aktif. Jangan gunakan dokumen ini sebagai dasar firmware, pengujian, atau presentasi.**

Dokumen ini sebelumnya menjelaskan interpolasi linear bertingkat dari tiga titik pengujian lama. Metode tersebut telah digantikan oleh regresi linear kuadrat terkecil menggunakan delapan pasangan `ADC Alatku` dan `NTU Lab Bante` terbaru.

## Referensi Aktif

| Kebutuhan | Dokumen / File |
|---|---|
| Data sumber terbaru | `sampel_baru_turbidity.md` dan `turbidity compare.xlsx` |
| Perhitungan regresi dan materi sidang | `kalibrasiturbidy.md` |
| Menghitung ulang koefisien | `kalibrasi_turbidity.py` |
| Implementasi firmware | `main/config.h` dan `main/sensors.cpp` |

## Metode Aktif

```text
NTU = (1,251645 x ADC) - 888,878830
```

Rentang yang tervalidasi adalah ADC `710-1084` atau `0-468 NTU`. Nilai di atas rentang tetap dapat dihitung firmware sebagai estimasi, tetapi belum dapat diklaim terkalibrasi.

Riwayat metode lama dipertahankan melalui commit Git dan workbook arsip `Perbandingan_Turbidity.xlsx`.
