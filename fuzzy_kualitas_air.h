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

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- DEKLARASI FUNGSI UTAMA -------------------------------------- */
float         FuzzyKualitasAir_HitungSkor(float tds, float turbidity);
KualitasAir_t FuzzyKualitasAir_GetStatus(float skor);
const char*   FuzzyKualitasAir_GetPesan(KualitasAir_t status);
float         FuzzyKualitasAir_KompensasiTDS(float tds_raw, float suhu);
StatusSuhu_t  FuzzyKualitasAir_CekStatusSuhu(float suhu);

#ifdef __cplusplus
}
#endif

#endif /* FUZZY_KUALITAS_AIR_H */
