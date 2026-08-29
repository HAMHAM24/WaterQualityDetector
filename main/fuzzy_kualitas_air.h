/* ==========================================================================
 * fuzzy_kualitas_air.h
 * Header Fuzzy Sugeno - Klasifikasi Kualitas Air
 * Berdasarkan Metodologi Permenkes No. 2 Tahun 2023 dengan "Limiting Parameter"
 * ========================================================================== */

#ifndef FUZZY_KUALITAS_AIR_H
#define FUZZY_KUALITAS_AIR_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

/* ---------- ENUM STATUS OUTPUT KUALITAS AIR (4 LEVEL) ------------------- */
typedef enum {
    STATUS_SANGAT_LAYAK = 0,    /* z = 1.00 */
    STATUS_PROSES_SEDANG,       /* z = 0.67 */
    STATUS_PROSES_INTENSIF,     /* z = 0.33 */
    STATUS_TIDAK_LOLOS          /* z = 0.00 */
} KualitasAir_t;

/* ---------- ENUM STATUS SUHU AIR (3 LEVEL) ----------------------------- */
typedef enum { SUHU_SL = 0, SUHU_PS, SUHU_PI, SUHU_TL } StatusSuhu_t;

/* ---------- STRUCT HASIL THRESHOLD CHECK PEMANDIAN / KOLAM ------------- */
typedef struct {
    bool suhuAman;        /* 16 <= suhu <= 35 C (Gabungan Permenkes) */
    bool turbidityAman;   /* turbidity < 0.5 NTU (Standar kolam)     */
    bool semuaAman;       /* suhuAman && turbidityAman               */
} ThresholdResult_t;

/* --------------------------------------------------------------------------
 * PROFIL BAKU MUTU FUZZY
 *
 * Empat severity: SL=0, PS=1, PI=2, TL=3.
 * ------------------------------------------------------------------------ */
typedef struct {
    /* TDS: SL trap, PS tri, PI tri, TL trap. */
    float tdsSl_b, tdsSl_c, tdsPs_a, tdsPs_b, tdsPs_c;
    float tdsPi_a, tdsPi_b, tdsPi_c, tdsTl_a, tdsTl_b, tdsMax;
    /* Turbidity: SL trap, PS tri, PI tri, TL trap. */
    float turbSl_b, turbSl_c, turbPs_a, turbPs_b, turbPs_c;
    float turbPi_a, turbPi_b, turbPi_c, turbTl_a, turbTl_b, turbMax;
    /* Delta suhu: SL trap, PS tri, PI tri, TL trap. */
    float tempSl_b, tempSl_c, tempPs_a, tempPs_b, tempPs_c;
    float tempPi_a, tempPi_b, tempPi_c, tempTl_a, tempTl_b, tempMax;
    float threshSangatLayak, threshProsesSedang, threshProsesIntensif;
} FuzzyProfil_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- DEKLARASI FUNGSI UTAMA -------------------------------------- */

/**
 * @brief Menghitung skor kualitas AIR MINUM (3 input: TDS, Turb, Suhu).
 */
float FuzzyKualitasAir_HitungSkor_AirMinum(const FuzzyProfil_t* profil, float tds, float turbidity, float deltaSuhu);

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
StatusSuhu_t  FuzzyKualitasAir_CekStatusSuhu(float deltaSuhu, const FuzzyProfil_t* profil);

#ifdef __cplusplus
}
#endif

#endif /* FUZZY_KUALITAS_AIR_H */
