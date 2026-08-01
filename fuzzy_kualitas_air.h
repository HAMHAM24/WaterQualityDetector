/* ==========================================================================
 * fuzzy_kualitas_air.h
 * Header Fuzzy Sugeno - Klasifikasi Kualitas Air (TDS & Turbidity)
 * Target: STM32 Blackpill (STM32F4xx) - murni float, tanpa dynamic memory
 * ========================================================================== */

#ifndef FUZZY_KUALITAS_AIR_H
#define FUZZY_KUALITAS_AIR_H

#include <math.h>

/* ---------- ENUM STATUS OUTPUT ------------------------------------------ */
typedef enum {
    STATUS_LAYAK = 0,
    STATUS_LTM,
    STATUS_TL
} KualitasAir_t;

typedef enum {
    SUHU_NORMAL = 0,
    SUHU_ABNORMAL
} StatusSuhu_t;

/* --------------------------------------------------------------------------
 * PROFIL BAKU MUTU (membuat menu Parameter benar-benar berpengaruh)
 *
 * Setiap peruntukan air (higiene sanitasi, SPA, kolam renang, pemandian
 * umum) memiliki baku mutu berbeda, sehingga breakpoint membership
 * function fuzzy juga harus berbeda. Struktur ini dipakai agar mesin
 * fuzzy tetap generik: satu implementasi, banyak profil.
 *
 * Pola breakpoint mengikuti FIS hasil tuning MATLAB (kualitas_air.fis):
 *   Rendah  = trimf(0,   0,   A)
 *   Sedang  = trimf(0,   A,   B)
 *   Tinggi  = trimf(A,   B,   C)
 * dengan A = ambang "baik", B = baku mutu maksimum, C = batas semesta.
 *
 * Struktur ini sengaja C murni (tanpa Arduino.h) agar file .c ini dapat
 * dikompilasi sebagai C sekaligus dipakai dari C++.
 * ------------------------------------------------------------------------ */
typedef struct {
    float tdsA;          /* ppm - ambang TDS "baik"                        */
    float tdsB;          /* ppm - baku mutu TDS maksimum                   */
    float tdsC;          /* ppm - batas atas semesta TDS                   */

    float turbA;         /* NTU - ambang kekeruhan "baik"                  */
    float turbB;         /* NTU - baku mutu kekeruhan maksimum             */
    float turbC;         /* NTU - batas atas semesta kekeruhan             */

    float threshLayak;   /* skor minimum agar berstatus LAYAK              */
    float threshLTM;     /* skor minimum agar berstatus LTM                */
} FuzzyProfil_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- DEKLARASI FUNGSI UTAMA -------------------------------------- */

/**
 * @brief Menghitung skor kualitas air (0-100) dengan profil baku mutu
 *        tertentu. Ini adalah entry point utama yang dipakai firmware.
 * @param profil     Profil baku mutu aktif (tidak boleh NULL).
 * @param tds        Nilai TDS terkompensasi suhu, satuan ppm.
 * @param turbidity  Nilai kekeruhan, satuan NTU.
 */
float FuzzyKualitasAir_HitungSkorProfil(const FuzzyProfil_t* profil,
                                         float tds, float turbidity);

/**
 * @brief Versi ringkas memakai profil bawaan Higiene Sanitasi. Dipertahankan
 *        agar uji validasi baseline MATLAB (TDS=350, Turb=10 -> 32.8) tetap
 *        dapat dijalankan tanpa menyiapkan profil terlebih dahulu.
 */
float FuzzyKualitasAir_HitungSkor(float tds, float turbidity);

/**
 * @brief Mengubah skor menjadi label status memakai ambang milik profil.
 */
KualitasAir_t FuzzyKualitasAir_GetStatusProfil(const FuzzyProfil_t* profil, float skor);

/**
 * @brief Versi ringkas dengan ambang bawaan (75.0 / 25.0).
 */
KualitasAir_t FuzzyKualitasAir_GetStatus(float skor);

const char*   FuzzyKualitasAir_GetPesan(KualitasAir_t status);
float         FuzzyKualitasAir_KompensasiTDS(float tds_raw, float suhu);
StatusSuhu_t  FuzzyKualitasAir_CekStatusSuhu(float suhu);

/**
 * @brief Profil bawaan Higiene Sanitasi (setara isi kualitas_air.fis).
 */
const FuzzyProfil_t* FuzzyKualitasAir_ProfilDefault(void);

#ifdef __cplusplus
}
#endif

#endif /* FUZZY_KUALITAS_AIR_H */
