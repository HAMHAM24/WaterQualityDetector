# Rule Base Fuzzy Sugeno — Deteksi Kelayakan Kualitas Fisik Air
### Parameter: TDS, Turbidity (Kekeruhan), Suhu — untuk 3 peruntukan (Air Minum, Higiene Sanitasi, Pemandian Umum)

---

## 1. Dasar Regulasi (Verifikasi dari Permenkes RI No. 2 Tahun 2023)

File Permenkes yang diunggah sudah dicek langsung isi tabelnya. Berikut kutipan angka yang relevan (parameter fisik saja):

| Peruntukan | Tabel Acuan | Suhu | TDS | Kekeruhan |
|---|---|---|---|---|
| **Air Minum** | Tabel 1 – Parameter Wajib Air Minum | Suhu udara ± 3°C | < 300 mg/L | < 3 NTU |
| **Higiene Sanitasi** | Tabel 3 – Parameter Air Higiene Sanitasi | Suhu udara ± 3°C | < 300 mg/L | < 3 NTU |
| **Air Kolam Renang** *(pembanding, bukan dipakai)* | Tabel — Parameter Fisik Kolam Renang | 16–40°C | – | 0,5 NTU |
| **Air Pemandian Umum** | Tabel 10 – Parameter Fisik Air Pemandian Umum | **15–35°C** (kontak lama) | tidak diatur (air alami tanpa pengolahan) | diukur sebagai **kejernihan** (Secchi disk 200 mm, terlihat jelas ≥ 1,6 m), **bukan NTU** |

**Catatan penting yang perlu kamu cantumkan di BAB metodologi TA:**
- Untuk Air Minum & Higiene Sanitasi, angka TDS < 300 mg/L dan Kekeruhan < 3 NTU **memang persis** seperti yang kamu sebutkan — sudah sesuai Tabel 1 & Tabel 3 Permenkes No. 2/2023. Perlu dicatat bahwa Tabel 3 (Higiene Sanitasi) di regulasi aslinya sebenarnya juga mencantumkan syarat suhu ±3°C, tapi karena parameter yang kamu pakai untuk konteks ini sengaja dibatasi hanya TDS + Turbidity, itu murni keputusan desain sistemmu (bisa kamu jelaskan sebagai penyederhanaan sensor karena suhu higiene sanitasi umumnya sudah mengikuti suhu ruang tempat penampungan).
- Untuk Air Pemandian Umum, regulasi **tidak mengatur TDS** (karena ini air alami tanpa pengolahan) — ini menguatkan alasanmu untuk bypass TDS. Regulasi juga **tidak memakai satuan NTU** untuk parameter fisiknya, melainkan "kejernihan" dengan metode Secchi disk (manual, visual). Karena piringan Secchi tidak feasible untuk alat portable otomatis, penggunaan sensor turbidity (NTU) sebagai **proksi rekayasa** dari parameter kejernihan adalah pendekatan yang lazim dipakai di penelitian sejenis (lihat referensi §2) — cukup dicantumkan sebagai batasan/asumsi penelitian di TA-mu, bukan sebagai kutipan literal dari Permenkes.

---

## 2. Referensi Pendukung (selain Permenkes)

Selain Permenkes No. 2/2023, rule base ini disusun dengan mengacu pada pola penelitian sejenis yang sudah pernah dipublikasikan, supaya strukturnya "ideal dan valid" secara metodologis:

1. **Putra, dkk.** — *"Penerapan Logika Fuzzy Untuk Mendeteksi Kualitas Air Higiene Sanitasi Menggunakan Metode Sugeno (Studi Kasus: Air Tanah Kota Bekasi)"*, Prosiding Seminar Nasional Mahasiswa Ilmu Komputer. Riset ini paling relevan karena persis membahas Higiene Sanitasi dengan Sugeno, sensor pH/TDS/kekeruhan/DS18B20, dan mengacu Permenkes 32/2017 (versi sebelum revisi 2023 — parameter fisiknya sama).
2. **Khodijah, S., Rumani, R. M., Sunarya, U.** — *"Perancangan Dan Implementasi Alat Ukur Untuk Penentuan Kualitas Air Berbasis Logika Fuzzy Metode Sugeno"*, eProceedings of Engineering, 2017. Struktur rule base 3-input (pH/TDS/Turbidity) dengan Sugeno orde-0 (singleton) menjadi acuan pola rule table di dokumen ini.
3. **(Jurnal Pengembangan Teknologi Informasi dan Ilmu Komputer, Vol. V No. 8)** — *"Sistem Klasifikasi Mutu Air PDAM Berdasarkan Zat Terlarut, pH, dan Turbidity Menggunakan Metode Fuzzy Sugeno Berbasis Arduino"*. Mengonfirmasi pola threshold TDS & turbidity yang lazim dipakai pada objek air PDAM/air minum.
4. **Cholilulloh, M., Syauqy, D., Tibyani** — *"Implementasi Metode Fuzzy Pada Kualitas Air Kolam Berdasarkan Suhu dan Kekeruhan"*, Jurnal Pengembangan Teknologi Informasi dan Ilmu Komputer, 2018. Menjadi acuan pola rule base 2-input (Suhu × Kekeruhan) yang dipakai pada bagian Pemandian Umum di dokumen ini (karena TDS di-bypass, struktur 2 input inilah yang paling mendekati).
5. Prinsip **"parameter terburuk (limiting parameter) menentukan status mutu"** yang lazim dipakai pada metode indeks mutu air versi Indonesia (mis. Metode STORET / Indeks Pencemaran, Kepmen LH No. 115 Tahun 2003) dipakai sebagai dasar logika penyusunan rule (lihat §4) — bukan dikutip persis, hanya diadaptasi prinsipnya untuk menjamin rule base bersifat konservatif terhadap keselamatan pengguna air.

---

## 3. Variabel Input & Himpunan Keanggotaan (Fuzzifikasi)

Semua variabel dipetakan ke **3 himpunan** dengan level severity 0 (ideal), 1 (batas/menyimpang), 2 (buruk/ekstrem) — supaya polanya konsisten dan mudah diterjemahkan ke rule.

### 3.1 TDS (mg/L) — dipakai di Air Minum & Higiene Sanitasi (acuan: < 300 mg/L)

| Himpunan | Bentuk | Parameter (mg/L) | Severity |
|---|---|---|---|
| IDEAL | Trapesium | (0, 0, 150, 250) | 0 |
| BATAS | Segitiga | (150, 300, 450) | 1 |
| TINGGI | Trapesium | (300, 450, 2000, 2000) | 2 |

### 3.2 Turbidity / Kekeruhan (NTU) — Air Minum & Higiene Sanitasi (acuan: < 3 NTU)

| Himpunan | Bentuk | Parameter (NTU) | Severity |
|---|---|---|---|
| JERNIH | Trapesium | (0, 0, 1.5, 2.5) | 0 |
| SEDANG | Segitiga | (1.5, 3, 5) | 1 |
| KERUH | Trapesium | (4, 6, 100, 100) | 2 |

### 3.3 Suhu — Air Minum & Higiene Sanitasi
Dinyatakan sebagai **ΔT = │T_ukur − T_ruang│** (mengikuti bunyi regulasi "suhu udara ± 3°C"):

| Himpunan | Bentuk | Parameter (°C) | Severity |
|---|---|---|---|
| IDEAL | Trapesium | (0, 0, 1, 3) | 0 |
| MENYIMPANG | Segitiga | (2, 4, 6) | 1 |
| EKSTREM | Trapesium | (5, 8, 50, 50) | 2 |

### 3.4 Turbidity (NTU, proksi kejernihan) — Air Pemandian Umum
Karena badan air alami secara default lebih keruh dari air minum, ambang digeser lebih longgar (acuan tambahan: baku mutu kekeruhan air permukaan kelas rekreasi/kontak langsung):

| Himpunan | Bentuk | Parameter (NTU) | Severity |
|---|---|---|---|
| JERNIH | Trapesium | (0, 0, 10, 20) | 0 |
| SEDANG | Segitiga | (15, 30, 50) | 1 |
| KERUH_BERLUMPUR | Trapesium | (40, 60, 500, 500) | 2 |

### 3.5 Suhu — Air Pemandian Umum
Regulasi memberi **rentang mutlak** 15–35°C, bukan toleransi ± dari suhu ruang. Dinyatakan sebagai deviasi keluar rentang: **Δ = maks(0, 15 − T_ukur, T_ukur − 35)**:

| Himpunan | Bentuk | Parameter (°C, deviasi) | Severity |
|---|---|---|---|
| IDEAL (15–35°C) | Trapesium | (0, 0, 1, 3) | 0 |
| MENYIMPANG | Segitiga | (2, 5, 8) | 1 |
| EKSTREM | Trapesium | (6, 10, 30, 30) | 2 |

---

## 4. Metodologi Penyusunan Rule (logika agregasi)

Supaya rule base tidak asal tebak dan tetap konservatif terhadap keselamatan pengguna, dipakai logika **"limiting parameter" (parameter terburuk menentukan)** yang diadaptasi dari prinsip indeks mutu air:

1. Hitung severity tiap input aktif (0 / 1 / 2) dari hasil fuzzifikasi (ambil label dengan derajat keanggotaan dominan/µ tertinggi).
2. `max_sev` = severity tertinggi di antara semua input.
3. `count_max` = jumlah input yang berada pada level `max_sev` tersebut (mendeteksi apakah masalahnya cuma 1 parameter atau serentak beberapa parameter).
4. Pemetaan ke output singleton:

| max_sev | count_max | Output (singleton z) |
|---|---|---|
| 0 | – | **SANGAT_LAYAK** (z = 1.00) |
| 1 | 1 | **LAYAK_SARING_RINGAN** (z = 0.75) |
| 1 | ≥ 2 | **CUKUP_PROSES_SEDANG** (z = 0.50) |
| 2 | 1 | **KRITIS_PROSES_INTENSIF** (z = 0.25) |
| 2 | ≥ 2 | **TIDAK_LOLOS** (z = 0.00) |

Logika ini masuk akal secara fisik: satu parameter yang buruk sendirian tetap membuat air itu berisiko (turun ke KRITIS), tapi begitu **dua atau lebih parameter fisik memburuk bersamaan**, air dinyatakan **TIDAK_LOLOS** — konsisten dengan deskripsi himpunanmu ("air sangat pekat/keruh, kualitas fisik rusak").

### Defuzzifikasi Sugeno (Weighted Average)
Karena semua output berupa singleton, keluaran akhir dihitung dengan rata-rata terbobot standar orde-0 Sugeno:

```
z* = Σ (αᵢ × zᵢ) / Σ αᵢ
```

di mana `αᵢ` = derajat pemenuhan (firing strength) rule ke-i, umumnya diambil dari operator **min** (AND) antar derajat keanggotaan input, dan `zᵢ` = nilai singleton keluaran rule tersebut.

---

## 5. RULE BASE — AIR MINUM (3 input: TDS, Turbidity, ΔSuhu) — 27 rules

| No | TDS | Turbidity | ΔSuhu | → Output | z |
|---|---|---|---|---|---|
| R1 | IDEAL | JERNIH | IDEAL | SANGAT_LAYAK | 1.00 |
| R2 | IDEAL | JERNIH | MENYIMPANG | LAYAK_SARING_RINGAN | 0.75 |
| R3 | IDEAL | JERNIH | EKSTREM | KRITIS_PROSES_INTENSIF | 0.25 |
| R4 | IDEAL | SEDANG | IDEAL | LAYAK_SARING_RINGAN | 0.75 |
| R5 | IDEAL | SEDANG | MENYIMPANG | CUKUP_PROSES_SEDANG | 0.50 |
| R6 | IDEAL | SEDANG | EKSTREM | KRITIS_PROSES_INTENSIF | 0.25 |
| R7 | IDEAL | KERUH | IDEAL | KRITIS_PROSES_INTENSIF | 0.25 |
| R8 | IDEAL | KERUH | MENYIMPANG | KRITIS_PROSES_INTENSIF | 0.25 |
| R9 | IDEAL | KERUH | EKSTREM | TIDAK_LOLOS | 0.00 |
| R10 | BATAS | JERNIH | IDEAL | LAYAK_SARING_RINGAN | 0.75 |
| R11 | BATAS | JERNIH | MENYIMPANG | CUKUP_PROSES_SEDANG | 0.50 |
| R12 | BATAS | JERNIH | EKSTREM | KRITIS_PROSES_INTENSIF | 0.25 |
| R13 | BATAS | SEDANG | IDEAL | CUKUP_PROSES_SEDANG | 0.50 |
| R14 | BATAS | SEDANG | MENYIMPANG | CUKUP_PROSES_SEDANG | 0.50 |
| R15 | BATAS | SEDANG | EKSTREM | KRITIS_PROSES_INTENSIF | 0.25 |
| R16 | BATAS | KERUH | IDEAL | KRITIS_PROSES_INTENSIF | 0.25 |
| R17 | BATAS | KERUH | MENYIMPANG | KRITIS_PROSES_INTENSIF | 0.25 |
| R18 | BATAS | KERUH | EKSTREM | TIDAK_LOLOS | 0.00 |
| R19 | TINGGI | JERNIH | IDEAL | KRITIS_PROSES_INTENSIF | 0.25 |
| R20 | TINGGI | JERNIH | MENYIMPANG | KRITIS_PROSES_INTENSIF | 0.25 |
| R21 | TINGGI | JERNIH | EKSTREM | TIDAK_LOLOS | 0.00 |
| R22 | TINGGI | SEDANG | IDEAL | KRITIS_PROSES_INTENSIF | 0.25 |
| R23 | TINGGI | SEDANG | MENYIMPANG | KRITIS_PROSES_INTENSIF | 0.25 |
| R24 | TINGGI | SEDANG | EKSTREM | TIDAK_LOLOS | 0.00 |
| R25 | TINGGI | KERUH | IDEAL | TIDAK_LOLOS | 0.00 |
| R26 | TINGGI | KERUH | MENYIMPANG | TIDAK_LOLOS | 0.00 |
| R27 | TINGGI | KERUH | EKSTREM | TIDAK_LOLOS | 0.00 |

---

## 6. RULE BASE — HIGIENE SANITASI (2 input: TDS, Turbidity) — 9 rules

| No | TDS | Turbidity | → Output | z |
|---|---|---|---|---|
| R1 | IDEAL | JERNIH | SANGAT_LAYAK | 1.00 |
| R2 | IDEAL | SEDANG | LAYAK_SARING_RINGAN | 0.75 |
| R3 | IDEAL | KERUH | KRITIS_PROSES_INTENSIF | 0.25 |
| R4 | BATAS | JERNIH | LAYAK_SARING_RINGAN | 0.75 |
| R5 | BATAS | SEDANG | CUKUP_PROSES_SEDANG | 0.50 |
| R6 | BATAS | KERUH | KRITIS_PROSES_INTENSIF | 0.25 |
| R7 | TINGGI | JERNIH | KRITIS_PROSES_INTENSIF | 0.25 |
| R8 | TINGGI | SEDANG | KRITIS_PROSES_INTENSIF | 0.25 |
| R9 | TINGGI | KERUH | TIDAK_LOLOS | 0.00 |

---

## 7. RULE BASE — AIR PEMANDIAN UMUM (2 input: ΔSuhu, Turbidity-proksi; **TDS di-bypass**) — 9 rules

| No | ΔSuhu | Turbidity | → Output | z |
|---|---|---|---|---|
| R1 | IDEAL | JERNIH | SANGAT_LAYAK | 1.00 |
| R2 | IDEAL | SEDANG | LAYAK_SARING_RINGAN | 0.75 |
| R3 | IDEAL | KERUH_BERLUMPUR | KRITIS_PROSES_INTENSIF | 0.25 |
| R4 | MENYIMPANG | JERNIH | LAYAK_SARING_RINGAN | 0.75 |
| R5 | MENYIMPANG | SEDANG | CUKUP_PROSES_SEDANG | 0.50 |
| R6 | MENYIMPANG | KERUH_BERLUMPUR | KRITIS_PROSES_INTENSIF | 0.25 |
| R7 | EKSTREM | JERNIH | KRITIS_PROSES_INTENSIF | 0.25 |
| R8 | EKSTREM | SEDANG | KRITIS_PROSES_INTENSIF | 0.25 |
| R9 | EKSTREM | KERUH_BERLUMPUR | TIDAK_LOLOS | 0.00 |

---

## 8. Rekap & Catatan untuk BAB Metodologi TA

- **Air Minum**: 27 rules (3×3×3), rule terberat/paling konservatif karena syarat kualitasnya paling ketat di antara ketiganya (sesuai amanat Permenkes bahwa air minum harus "aman digunakan secara langsung").
- **Higiene Sanitasi**: 9 rules (3×2), memakai batas TDS & Turbidity identik dengan Air Minum karena Tabel 3 Permenkes memang menetapkan angka yang sama persis.
- **Pemandian Umum**: 9 rules (3×2), TDS di-bypass sesuai sifat air alami tanpa pengolahan (justifikasi ini valid karena Permenkes memang tidak mencantumkan baku mutu TDS untuk kategori ini). Parameter kejernihan Secchi disk didekati dengan sensor turbidity — **cantumkan ini sebagai batasan masalah (assumption/limitation)** di BAB I atau BAB III skripsimu, supaya dosen penguji tidak mempertanyakan validitas metodologisnya.
- Karena ketiga konteks memakai sistem satu tuas selektor (mis. dropdown/switch "Jenis Air" pada alat), disarankan membuat 3 fungsi rule terpisah di kode (bukan 1 rule base gabungan) — lebih mudah divalidasi dan didebug per konteks.

Kalau kamu mau, aku bisa lanjutkan buatkan kode implementasi (Python `scikit-fuzzy` atau Arduino C++) dari rule base ini, atau ubah dokumen ini jadi file Word (.docx) supaya langsung bisa ditempel ke laporan TA.
