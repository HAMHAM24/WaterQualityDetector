/* ==========================================================================
 * fuzzy_kualitas_air.h
 * Header Fuzzy Sugeno - Klasifikasi Kualitas Air
 * Berdasarkan Metodologi Permenkes No. 2 Tahun 2023 dengan "Limiting Parameter"
 * ========================================================================== */

#ifndef FUZZY_KUALITAS_AIR_H
#define FUZZY_KUALITAS_AIR_H

#include <math.h>
#include <stdbool.h>

/* ---------- ENUM STATUS OUTPUT KUALITAS AIR (5 LEVEL) ------------------- */
typedef enum {
    STATUS_SANGAT_LAYAK = 0,    /* z = 1.00 */
    STATUS_LAYAK_SARING_RINGAN, /* z = 0.75 */
    STATUS_CUKUP_PROSES_SEDANG, /* z = 0.50 */
    STATUS_KRITIS,              /* z = 0.25 */
    STATUS_TIDAK_LOLOS          /* z = 0.00 */
} KualitasAir_t;

/* ---------- ENUM STATUS SUHU AIR (3 LEVEL) ----------------------------- */
typedef enum {
    SUHU_NORMAL = 0,
    SUHU_DINGIN,
    SUHU_PANAS
} StatusSuhu_t;

/* ---------- STRUCT HASIL THRESHOLD CHECK PEMANDIAN / KOLAM ------------- */
typedef struct {
    bool suhuAman;        /* 16 <= suhu <= 35 C (Gabungan Permenkes) */
    bool turbidityAman;   /* turbidity < 0.5 NTU (Standar kolam)     */
    bool semuaAman;       /* suhuAman && turbidityAman               */
} ThresholdResult_t;

/* --------------------------------------------------------------------------
 * PROFIL BAKU MUTU FUZZY
 *
 * Menggunakan pendekatan 3 Severity Level (0=Ideal, 1=Batas, 2=Buruk).
 * Untuk suhu air absolut: Normal=0, Dingin=1, Panas=2.
 *   IDEAL (0)   : trapmf [0, 0, p0_b, p0_c]
 *   BATAS (1)   : trimf  [p1_a, p1_b, p1_c]
 *   TINGGI (2)  : trapmf [p2_a, p2_b, max, max]
 * ------------------------------------------------------------------------ */
typedef struct {
    /* Parameter TDS (mg/L) */
    float tds0_b, tds0_c;
    float tds1_a, tds1_b, tds1_c;
    float tds2_a, tds2_b, tds2_max;

    /* Parameter Turbidity (NTU) */
    float turb0_b, turb0_c;
    float turb1_a, turb1_b, turb1_c;
    float turb2_a, turb2_b, turb2_max;

    /* Parameter Suhu Air Absolut (Celsius) */
    float suhuDingin_b, suhuDingin_c;           /* trapmf [0, 0, b, c] */
    float suhuNormal_a, suhuNormal_b, suhuNormal_c; /* trimf [a, b, c] */
    float suhuPanas_a, suhuPanas_b, suhuPanas_c;    /* trapmf [a, b, c, c] */

    /* Thresholds */
    float threshSangatLayak;
    float threshLayakSaring;
    float threshCukup;
    float threshKritis;
} FuzzyProfil_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- DEKLARASI FUNGSI UTAMA -------------------------------------- */

/**
 * @brief Menghitung skor kualitas AIR MINUM (3 input: TDS, Turb, Suhu).
 */
float FuzzyKualitasAir_HitungSkor_AirMinum(const FuzzyProfil_t* profil, float tds, float turbidity, float suhu);

/**
 * @brief Menghitung skor kualitas HIGIENE SANITASI (2 input: TDS, Turb).
 */
float FuzzyKualitasAir_HitungSkor_Higiene(const FuzzyProfil_t* profil, float tds, float turbidity);

/**
 * @brief Fungsi legacy evaluasi fuzzy Pemandian (tidak dipakai mode aktif).
 */
float FuzzyKualitasAir_HitungSkor_Pemandian(const FuzzyProfil_t* profil, float suhu, float turbidity);

/**
 * @brief Evaluasi langsung (non-fuzzy threshold checker) khusus Pemandian / Kolam
 *        berdasarkan ambang batas gabungan konservatif: Suhu 16-35 C, Turb < 0.5 NTU.
 * @param suhu       Suhu aktual air (Celsius).
 * @param turbidity  Nilai kekeruhan (NTU).
 */
ThresholdResult_t Threshold_CekPemandianKolam(float suhu, float turbidity);

/**
 * @brief Mengubah skor (0.0 - 1.0) menjadi enum status kualitas air.
 */
KualitasAir_t FuzzyKualitasAir_GetStatusProfil(const FuzzyProfil_t* profil, float skor);

/**
 * @brief Mengembalikan teks pesan rekomendasi tindakan.
 */
const char* FuzzyKualitasAir_GetPesan(KualitasAir_t status);

/**
 * @brief Mengembalikan string ringkas label status kualitas air.
 */
const char* FuzzyKualitasAir_GetStatusBadge(KualitasAir_t status);

/**
 * @brief Mengembalikan string status suhu ("Ideal", "Menyimpang", "Ekstrem").
 */
const char* FuzzyKualitasAir_GetStatusSuhuStr(StatusSuhu_t status);

float         FuzzyKualitasAir_KompensasiTDS(float tds_raw, float suhu_aktual);
StatusSuhu_t  FuzzyKualitasAir_CekStatusSuhu(float suhu, const FuzzyProfil_t* profil);

#ifdef __cplusplus
}
#endif

#endif /* FUZZY_KUALITAS_AIR_H */
