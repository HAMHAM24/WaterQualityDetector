# Tahapan Input Suhu Udara Manual

Pada penelitian ini, suhu udara sekitar digunakan sebagai nilai acuan untuk menilai parameter suhu air. Nilai suhu udara dimasukkan secara manual oleh pengguna sebelum proses pengukuran sampel air dilakukan. Selanjutnya, sistem membaca suhu air menggunakan sensor DS18B20 dan menghitung deviasi suhu sebagai input pada sistem Fuzzy Sugeno.

## Tahapan Sistem

1. Pengguna menyalakan alat.
2. Sistem menampilkan menu input suhu udara sekitar pada OLED.
3. Pengguna memasukkan nilai suhu udara sekitar secara manual melalui tombol navigasi.
4. Sistem menyimpan nilai suhu udara tersebut sebagai `T_udara`.
5. Pengguna mencelupkan sensor DS18B20 ke dalam sampel air.
6. Sensor DS18B20 membaca suhu air sebagai `T_air`.
7. Sistem menghitung deviasi suhu menggunakan persamaan:

```text
Delta T = |T_air - T_udara|
```

8. Nilai `Delta T` digunakan sebagai input fuzzy suhu.
9. Sistem menggabungkan input suhu, TDS, dan kekeruhan untuk proses inferensi Fuzzy Sugeno.
10. Sistem menampilkan hasil klasifikasi kualitas fisik air pada OLED.

## Variabel Suhu

```text
T_udara = suhu udara sekitar yang dimasukkan manual oleh pengguna (°C)
T_air   = suhu air yang dibaca oleh sensor DS18B20 (°C)
Delta T = selisih absolut antara suhu air dan suhu udara sekitar (°C)
```

## Membership Function Suhu

Input suhu pada sistem fuzzy menggunakan nilai `Delta T`, bukan suhu air absolut. Rentang input deviasi suhu dibuat dari 0 sampai 10°C. Batas utama kelayakan suhu adalah 3°C, sesuai acuan suhu udara sekitar ±3°C.

| Kode | Kategori | Tipe MF | Parameter |
|---|---|---|---|
| SL | Sangat Layak | trapmf | [0, 0, 1, 1.5] |
| PS | Perlu Proses Sedang | trimf | [1, 1.75, 2.5] |
| PI | Perlu Proses Intensif | trimf | [2, 2.75, 3.5] |
| TL | Tidak Lolos | trapmf | [3, 4, 10, 10] |

## Contoh Perhitungan

Misalnya pengguna memasukkan suhu udara sekitar sebesar 28°C. Setelah sensor DS18B20 dicelupkan ke air, suhu air yang terbaca adalah 31°C.

```text
Delta T = |31 - 28|
Delta T = 3°C
```

Berdasarkan hasil tersebut, nilai deviasi suhu berada pada batas toleransi suhu udara ±3°C. Nilai ini kemudian diproses oleh membership function suhu dan digabungkan dengan parameter TDS serta kekeruhan pada sistem Fuzzy Sugeno.

## Kalimat Siap Tempel ke Skripsi

Pada penelitian ini, suhu udara sekitar digunakan sebagai acuan pembanding terhadap suhu air. Nilai suhu udara sekitar dimasukkan secara manual oleh pengguna melalui menu pada perangkat sebelum proses pengukuran dilakukan. Setelah nilai suhu udara tersimpan, sensor DS18B20 dicelupkan ke dalam sampel air untuk memperoleh nilai suhu air. Sistem kemudian menghitung deviasi suhu menggunakan persamaan ΔT = |T_air - T_udara|. Nilai deviasi suhu tersebut digunakan sebagai input fuzzy suhu karena baku mutu suhu air mengacu pada rentang suhu udara sekitar ±3°C.

