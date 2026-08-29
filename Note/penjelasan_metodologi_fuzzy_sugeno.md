# Penjelasan Metodologi Rule Base Fuzzy Sugeno
## Sistem Klasifikasi Kualitas Air Higiene Sanitasi (Suhu, TDS, Turbidity)

Dokumen ini menjelaskan tiga hal utama dalam rancangan sistem fuzzy Sugeno:
1. Dasar penentuan nilai output (z) pada tiap rule
2. Logika penentuan rule base (mengapa disusun seperti itu)
3. Peran ganda variabel suhu — sebagai *input* kompensasi termal untuk TDS, sekaligus sebagai parameter mandiri yang menentukan lolos/tidaknya kualitas air

---

## 1. Dasar Penentuan Nilai Output (z)

Sistem menggunakan 4 himpunan fuzzy pada tiap variabel input (Suhu, TDS, Turbidity):

| Kategori | Indeks Keparahan | Nilai z (Sugeno) |
|---|---|---|
| Sangat Layak (SL) | 0 | 1.00 |
| Perlu Proses Sedang (PS) | 1 | 0.67 |
| Perlu Proses Intensif (PI) | 2 | 0.33 |
| Tidak Lolos (TL) | 3 | 0.00 |

### Kenapa nilai z dibagi rata (1.00 / 0.67 / 0.33 / 0.00)?

Nilai z ini dihasilkan dari rumus:

```
z = 1 − (indeks_keparahan / (n − 1))
```

dengan `n = 4` (jumlah himpunan fuzzy). Sehingga:

- SL → z = 1 − (0/3) = **1.00**
- PS → z = 1 − (1/3) = **0.67**
- PI → z = 1 − (2/3) = **0.33**
- TL → z = 1 − (3/3) = **0.00**

Pembagian ini **linear dan berjarak sama (equidistant)** karena:

1. **Permenkes No. 2/2023 tidak memberi bobot berbeda antar level keparahan** — regulasi hanya menetapkan satu garis ambang batas per parameter (pass/fail), sehingga tidak ada dasar ilmiah untuk membuat satu level lebih "berat" dari level lain secara sepihak. Skala linear adalah pendekatan paling netral dan defensible ketika tidak ada data pembobotan empiris.
2. **Konsistensi dengan pola Sugeno orde-0** pada jurnal rujukan (Kharim dkk., 2025) yang juga menggunakan nilai konstanta diskrit per kategori output (bukan fungsi linear kompleks), sehingga hasil defuzzifikasi (*weighted average*) tetap mudah diinterpretasikan sebagai skor 0–1.
3. **Skala 0–1 memudahkan ambang keputusan di embedded system** (STM32): satu variabel `float hasil` dari fungsi `defuzzifikasi()` bisa langsung dibandingkan dengan threshold (`hasil >= 0.83` → Sangat Layak, dst.) tanpa perlu tabel konversi tambahan.

---

## 2. Logika Penentuan Rule Base ("Worst Parameter Wins")

### Aturan dasar

Untuk kombinasi 3 variabel (Suhu, TDS, Turbidity) dengan 4 himpunan masing-masing, total rule = 4³ = **64 rule**. Output tiap rule ditentukan oleh:

```
Output_rule = MAX(indeks_suhu, indeks_tds, indeks_turbidity)
```

Artinya, **kategori terburuk di antara ketiga parameter menentukan hasil akhir rule** — bukan rata-rata, bukan mayoritas.

### Kenapa dipilih MAX, bukan skema lain (rata-rata/voting)?

**a. Sifat parameter wajib pada Permenkes No. 2/2023**
Permenkes menetapkan Suhu, TDS, dan Turbidity sebagai parameter **fisik wajib** yang masing-masing punya baku mutu independen (Suhu ±3°C, TDS <300 mg/L, Turbidity <3 NTU). Status akhir "Memenuhi Syarat / Tidak Memenuhi Syarat" pada form pengawasan kualitas air ditentukan **per parameter**, dan air dinyatakan tidak memenuhi syarat jika **ada satu saja** parameter wajib yang gagal — terlepas dari bagaimana kondisi parameter lainnya. Ini prinsip *conjunctive compliance* (kepatuhan gabungan), bukan *compensatory scoring* (nilai bisa saling menutupi).

**b. Analogi dengan sistem if-else pada jurnal rujukan**
Pada jurnal 124-148 (Kharim dkk., 2025), logika awal sistem (sebelum fuzzy) sudah menerapkan prinsip ini secara eksplisit melalui struktur `if-else` bertingkat: begitu satu parameter (TDS, turbidity, atau pH) melewati ambang, status langsung jatuh ke kategori tidak layak/perlu perbaikan, walau parameter lain normal. Skema MAX pada rule fuzzy adalah generalisasi dari logika ini ke domain kontinu (fuzzy), sehingga transisi antar kategori jadi lebih halus (tidak jump seketika seperti if-else biner), tapi **prinsip keparahan dominan tetap dipertahankan**.

**c. Konsekuensi keamanan (safety-first design)**
Karena turbidity, TDS, dan suhu ekstrem masing-masing punya implikasi risiko kesehatan yang berbeda tapi sama-sama nyata (turbidity → melindungi patogen dari disinfeksi; TDS tinggi → potensi kontaminasi mineral/logam; suhu ekstrem → mendukung pertumbuhan mikroba), maka **tidak tepat** membiarkan satu parameter yang bagus "menutupi" parameter lain yang buruk. Prinsip *the weakest link determines the outcome* lebih sesuai untuk konteks keselamatan air konsumsi/higiene dibanding prinsip rata-rata yang biasa dipakai untuk skoring non-kritikal (misalnya penilaian performa produk).

### Contoh penerapan pada tabel rule

| Suhu | TDS | Turbidity | MAX(indeks) | Output |
|---|---|---|---|---|
| Sangat Layak (0) | Sangat Layak (0) | Tidak Lolos (3) | 3 | Tidak Lolos |
| Tidak Lolos (3) | Sangat Layak (0) | Sangat Layak (0) | 3 | Tidak Lolos |
| Perlu Proses Sedang (1) | Perlu Proses Sedang (1) | Sangat Layak (0) | 1 | Perlu Proses Sedang |

Baris pertama dan kedua menunjukkan simetri: **parameter mana pun** yang gagal (bukan cuma turbidity atau cuma TDS) akan menyeret hasil akhir ke kategori terburuknya. Tidak ada parameter yang "diistimewakan" — keduanya diperlakukan setara sesuai sifat wajib pada regulasi.

---

## 3. Peran Ganda Variabel Suhu

Variabel suhu dalam sistem ini memiliki **dua fungsi yang berjalan bersamaan dan tidak saling meniadakan**:

### Peran 1 — Suhu sebagai Kompensasi Termal Otomatis untuk TDS

Sensor TDS berbasis konduktivitas (seperti DFRobot SEN0244) **tidak mengukur jumlah zat terlarut secara langsung**, melainkan mengukur konduktivitas listrik air, yang kemudian dikonversi menjadi nilai TDS (ppm). Konduktivitas listrik air sangat dipengaruhi suhu karena kenaikan suhu meningkatkan mobilitas ion terlarut, sehingga nilai konduktivitas (dan TDS terbaca) ikut naik meski jumlah zat padat sesungguhnya tidak berubah. Efek ini dikenal sebagai *thermal drift*, dengan koefisien standar **≈2% per °C** terhadap suhu referensi 25°C.

**Rumus kompensasi termal (thermal compensation):**

```
compensationCoefficient = 1.0 + 0.02 × (T_terukur − 25.0)
V_terkompensasi = V_mentah / compensationCoefficient
```

Nilai `V_terkompensasi` selanjutnya dikonversi ke satuan ppm menggunakan polinomial kalibrasi standar sensor:

```
TDS(ppm) = (133.42×V³ − 255.86×V² + 857.39×V) × 0.5
```

di mana `V = V_terkompensasi`, dan faktor `0.5` adalah rasio konversi EC→TDS standar (dapat disesuaikan 0.5–0.7 sesuai hasil kalibrasi sensor terhadap alat TDS meter standar).

**Fungsi peran ini:** memastikan nilai `final_tds` yang masuk ke fuzzifikasi TDS adalah nilai yang **sudah bersih dari bias suhu** — bukan sekadar angka mentah yang bisa naik-turun semata karena fluktuasi suhu air, bukan karena perubahan kualitas air yang sesungguhnya.

### Peran 2 — Suhu sebagai Parameter Mandiri Penentu Lolos/Tidaknya Air

Terlepas dari perannya dalam preprocessing TDS, suhu **tetap diukur, difuzzifikasi, dan dimasukkan sebagai salah satu dari tiga sumbu rule base** (Suhu × TDS × Turbidity), karena Permenkes No. 2/2023 menetapkan suhu sebagai **parameter fisik wajib tersendiri** dengan baku mutu ±3°C dari suhu ruangan/udara sekitar. Artinya, air dengan suhu yang menyimpang jauh dari suhu lingkungan (misalnya karena kontaminasi termal, gangguan pipa, atau proses kimia yang tidak diinginkan) tetap dapat dinyatakan **Tidak Lolos**, meskipun nilai TDS-nya sendiri (setelah dikompensasi) berada dalam rentang aman.

**Fungsi peran ini:** suhu ikut menentukan status akhir kualitas air melalui skema *worst-wins* pada Bagian 2 — persis seperti TDS dan turbidity, tanpa pengecualian.

### Kenapa dua peran ini tidak saling bertentangan

Kedua peran beroperasi pada **tahap pemrosesan yang berbeda**:

| Tahap | Peran Suhu |
|---|---|
| **Preprocessing (sebelum fuzzifikasi)** | Sebagai variabel koreksi numerik pada rumus konversi tegangan→TDS. Suhu di sini hanya dipakai secara matematis untuk menstabilkan pembacaan sensor TDS, tidak menghasilkan output fuzzy apa pun. |
| **Fuzzifikasi & rule base (setelah preprocessing)** | Sebagai variabel input independen ketiga (bersama TDS-terkompensasi dan Turbidity), dengan himpunan fuzzy sendiri (Sangat Layak/Proses Sedang/Proses Intensif/Tidak Lolos) dan ikut menentukan output akhir melalui skema MAX. |

Dengan kata lain: **nilai suhu yang sama** (misalnya hasil pembacaan sensor DS18B20) dipakai dua kali untuk dua tujuan berbeda — sekali sebagai variabel koreksi di dalam rumus TDS (tidak membentuk himpunan fuzzy sendiri di titik ini), dan sekali lagi sebagai variabel fuzzy mandiri di rule base (dengan himpunan fuzzy dan sumbu rule sendiri). Ini analog dengan bagaimana satu sensor bisa memberi dua jenis informasi sekaligus: informasi koreksi (metadata) untuk sensor lain, dan informasi substansi (data utama) untuk penilaiannya sendiri.

### Diagram alur singkat

```
Sensor DS18B20 (suhu) ─┬─→ [Rumus Kompensasi Termal] ─→ V_terkompensasi ─→ TDS(ppm) terkoreksi ─→ Fuzzifikasi TDS ─┐
                        │                                                                                          │
                        └─→ Fuzzifikasi Suhu (Δ terhadap suhu ruangan) ───────────────────────────────────────────┼─→ Rule Base (MAX) ─→ Defuzzifikasi ─→ Output Akhir
                                                                                                                    │
Sensor Turbidity ───────────────────────────────────────────────────────→ Fuzzifikasi Turbidity ──────────────────┘
```

---

## Ringkasan

1. **Nilai z** dibagi linear-equidistant (1.00/0.67/0.33/0.00) karena Permenkes tidak memberi dasar pembobotan berbeda antar level, sehingga skala rata adalah pendekatan paling netral dan mudah diimplementasikan di sistem embedded.
2. **Rule base** disusun dengan prinsip *worst parameter wins* (MAX), sesuai sifat parameter wajib pada Permenkes yang tidak mengenal kompensasi antar parameter — satu parameter gagal berarti keseluruhan gagal, demi menjaga aspek keselamatan air konsumsi/higiene sanitasi.
3. **Suhu berperan ganda**: (a) sebagai variabel koreksi numerik dalam rumus kompensasi termal untuk menstabilkan pembacaan sensor TDS terhadap fluktuasi suhu, dan (b) sebagai variabel fuzzy independen yang tetap ikut menentukan status lolos/tidaknya kualitas air sesuai baku mutu suhu ±3°C pada Permenkes No. 2/2023. Kedua peran ini beroperasi di tahap pemrosesan berbeda sehingga tidak saling meniadakan.
