/* ==========================================================================
 * fuzzy_kualitas_air.h
 * Header Fuzzy Sugeno - Klasifikasi Kualitas Air (TDS & Turbidity)
 * Target: STM32 Blackpill (STM32F4xx) - murni float, tanpa dynamic memory
 * Skala Output: 0.0 - 1.0 (5 Level Kualitas)
 * ========================================================================== */

#ifndef FUZZY_KUALITAS_AIR_H
#define FUZZY_KUALITAS_AIR_H

#include <math.h>

/* ---------- ENUM STATUS OUTPUT KUALITAS AIR (5 LEVEL) ------------------- */
typedef enum {
    STATUS_EXCELLENT = 0,   /* z1 = 1.00 - Sangat Baik / Sangat Layak     */
    STATUS_GOOD,            /* z2 = 0.75 - Baik / Layak                   */
    STATUS_POOR,            /* z3 = 0.50 - Perlu Filtrasi Ringan          */
    STATUS_VERY_POOR,       /* z4 = 0.25 - Sangat Buruk / Filtrasi Intensif */
    STATUS_NOT_SUITABLE     /* z5 = 0.00 - Tidak Lolos / Dilarang         */
} KualitasAir_t;

/* ---------- ENUM STATUS SUHU AIR (3 LEVEL) ----------------------------- */
typedef enum {
    SUHU_DINGIN = 0,        /* <= 24 °C                                   */
    SUHU_NORMAL,            /* 24 - 32 °C                                 */
    SUHU_PANAS              /* >= 32 °C                                   */
} StatusSuhu_t;

/* --------------------------------------------------------------------------
 * PROFIL BAKU MUTU FUZZY
 *
 * Mendefinisikan parameter fungsi keanggotaan (MF) trapesium (trapmf) dan
 * segitiga (trimf) serta ambang klasifikasi status kualitas air (0.0 - 1.0).
 *
 * Pola kurva:
 *   TDS Rendah     : trapmf [0, 0, tdsRendah_b, tdsRendah_c]
 *   TDS Sedang     : trimf  [tdsSedang_a, tdsSedang_b, tdsSedang_c]
 *   TDS Tinggi     : trapmf [tdsTinggi_a, tdsTinggi_b, tdsTinggi_c, tdsTinggi_c]
 *
 *   Turb Jernih    : trapmf [0, 0, turbJernih_b, turbJernih_c]
 *   Turb Sedang    : trimf  [turbSedang_a, turbSedang_b, turbSedang_c]
 *   Turb Keruh     : trapmf [turbKeruh_a, turbKeruh_b, turbKeruh_c, turbKeruh_c]
 * ------------------------------------------------------------------------ */
typedef struct {
    /* Parameter TDS (ppm / mg/L) */
    float tdsRendah_b;       /* batas atas keanggotaan penuh (1.0)           */
    float tdsRendah_c;       /* batas akhir turun ke 0                       */
    float tdsSedang_a;       /* mulai naik dari 0                            */
    float tdsSedang_b;       /* puncak keanggotaan penuh (1.0)               */
    float tdsSedang_c;       /* akhir turun ke 0                             */
    float tdsTinggi_a;       /* mulai naik dari 0                            */
    float tdsTinggi_b;       /* mulai keanggotaan penuh (1.0)                */
    float tdsTinggi_c;       /* batas atas semesta TDS                       */

    /* Parameter Turbidity (NTU) */
    float turbJernih_b;      /* batas atas keanggotaan penuh (1.0)           */
    float turbJernih_c;      /* batas akhir turun ke 0                       */
    float turbSedang_a;      /* mulai naik dari 0                            */
    float turbSedang_b;      /* puncak keanggotaan penuh (1.0)               */
    float turbSedang_c;      /* akhir turun ke 0                             */
    float turbKeruh_a;       /* mulai naik dari 0                            */
    float turbKeruh_b;       /* mulai keanggotaan penuh (1.0)                */
    float turbKeruh_c;       /* batas atas semesta Turbidity                 */

    /* Ambang Skor (0.0 - 1.0) untuk konversi ke enum status */
    float threshExcellent;   /* skor minimum untuk EXCELLENT (misal 0.875)   */
    float threshGood;        /* skor minimum untuk GOOD (misal 0.625)        */
    float threshPoor;        /* skor minimum untuk POOR (misal 0.375)        */
    float threshVeryPoor;    /* skor minimum untuk VERY_POOR (misal 0.125)   */
} FuzzyProfil_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- DEKLARASI FUNGSI UTAMA -------------------------------------- */

/**
 * @brief Menghitung skor kualitas air (0.0 - 1.0) dengan profil baku mutu
 *        tertentu.
 * @param profil     Profil baku mutu aktif (tidak boleh NULL).
 * @param tds        Nilai TDS terkompensasi suhu, satuan ppm (mg/L).
 * @param turbidity  Nilai kekeruhan, satuan NTU.
 * @return Skor kualitas air pada skala 0.0 - 1.0.
 */
float FuzzyKualitasAir_HitungSkorProfil(const FuzzyProfil_t* profil,
                                         float tds, float turbidity);

/**
 * @brief Versi ringkas memakai profil default (Higiene Sanitasi / Air Minum).
 */
float FuzzyKualitasAir_HitungSkor(float tds, float turbidity);

/**
 * @brief Mengubah skor (0.0 - 1.0) menjadi enum status kualitas air.
 */
KualitasAir_t FuzzyKualitasAir_GetStatusProfil(const FuzzyProfil_t* profil, float skor);

/**
 * @brief Versi ringkas penentuan status memakai profil default.
 */
KualitasAir_t FuzzyKualitasAir_GetStatus(float skor);

/**
 * @brief Mengembalikan teks pesan rekomendasi tindakan.
 */
const char* FuzzyKualitasAir_GetPesan(KualitasAir_t status);

/**
 * @brief Mengembalikan string ringkas label status kualitas air.
 */
const char* FuzzyKualitasAir_GetStatusBadge(KualitasAir_t status);

/**
 * @brief Mengembalikan string status suhu ("Dingin", "Normal", "Panas").
 */
const char* FuzzyKualitasAir_GetStatusSuhuStr(StatusSuhu_t status);

float         FuzzyKualitasAir_KompensasiTDS(float tds_raw, float suhu);
StatusSuhu_t  FuzzyKualitasAir_CekStatusSuhu(float suhu);

/**
 * @brief Profil bawaan default (sesuai Note/Membership function.txt).
 */
const FuzzyProfil_t* FuzzyKualitasAir_ProfilDefault(void);

#ifdef __cplusplus
}
#endif

#endif /* FUZZY_KUALITAS_AIR_H */
