#include "fuzzy_kualitas_air.h"
#include <math.h>
#include <stddef.h>   /* NULL */

/* ---------- OUTPUT CONSTANTS (SUGENO ORDER-0: 0.0 - 1.0) ---------------- */
#define OUT_EXCELLENT     1.00f   /* Sangat Baik / Sangat Layak             */
#define OUT_GOOD          0.75f   /* Baik / Layak                           */
#define OUT_POOR          0.50f   /* Perlu Filtrasi Ringan                  */
#define OUT_VERY_POOR     0.25f   /* Sangat Buruk / Butuh Filtrasi Intensif */
#define OUT_NOT_SUITABLE  0.00f   /* Tidak Lolos / Dilarang Digunakan       */

/* ---------- AMBANG STATUS (THRESHOLDS) ----------------------------------- */
#define THRESH_EXCELLENT  0.875f  /* Skor >= 0.875 -> EXCELLENT             */
#define THRESH_GOOD       0.625f  /* 0.625 <= Skor < 0.875 -> GOOD          */
#define THRESH_POOR       0.375f  /* 0.375 <= Skor < 0.625 -> POOR          */
#define THRESH_VERY_POOR  0.125f  /* 0.125 <= Skor < 0.375 -> VERY_POOR     */
                                  /* Skor < 0.125 -> NOT_SUITABLE           */

/* ---------- BATAS SUHU (IF-ELSE, DI LUAR FUZZY) -------------------------- */
#define SUHU_DINGIN_MAX   24.0f   /* <= 24 °C -> Dingin                     */
#define SUHU_PANAS_MIN    32.0f   /* >= 32 °C -> Panas                      */
#define SUHU_REFERENSI    25.0f   /* referensi kompensasi TDS               */

/* Profil bawaan default: sesuai Note/Membership function.txt & kualitas_air.fis */
static const FuzzyProfil_t s_profilDefault = {
    /* TDS: Rendah [0 0 150 300], Sedang [150 500 1000], Tinggi [500 1000 1200 1200] */
    150.0f, 300.0f,
    150.0f, 500.0f, 1000.0f,
    500.0f, 1000.0f, 1200.0f,

    /* Turbidity: Jernih [0 0 1.5 3], Sedang [1.5 10 25], Keruh [10 25 30 30] */
    1.5f, 3.0f,
    1.5f, 10.0f, 25.0f,
    10.0f, 25.0f, 30.0f,

    /* Thresholds */
    THRESH_EXCELLENT, THRESH_GOOD, THRESH_POOR, THRESH_VERY_POOR
};

const FuzzyProfil_t* FuzzyKualitasAir_ProfilDefault(void)
{
    return &s_profilDefault;
}

/* ---------- FUNGSI MEMBERSHIP TRAPESIUM (trapmf) ------------------------ */
static float trapmf(float x, float a, float b, float c, float d)
{
    if (x < a || x > d) return 0.0f;
    if (x >= b && x <= c) return 1.0f;
    if (x < b) return (a == b) ? 1.0f : (x - a) / (b - a);
    return (c == d) ? 1.0f : (d - x) / (d - c);
}

/* ---------- FUNGSI MEMBERSHIP SEGITIGA (trimf) -------------------------- */
static float trimf(float x, float a, float b, float c)
{
    if (x < a || x > c) return 0.0f;
    if (x == b) return 1.0f;
    if (x < b) return (a == b) ? 1.0f : (x - a) / (b - a);
    return (b == c) ? 1.0f : (c - x) / (c - b);
}

/* ---------- FUNGSI MIN -------------------------------------------------- */
static float fmin2(float a, float b) { return (a < b) ? a : b; }

/* ==========================================================================
 * FUNGSI UTAMA: Hitung skor fuzzy (0.0 - 1.0) dari nilai TDS & Turbidity
 * ========================================================================== */
float FuzzyKualitasAir_HitungSkorProfil(const FuzzyProfil_t* profil,
                                         float tds, float turbidity)
{
    if (profil == NULL) {
        profil = &s_profilDefault;
    }

    /* --- 1. FUZZIFIKASI ---
     * Input dijepit ke rentang semesta [0, Max] agar nilai di luar batas
     * tidak menghasilkan zero-firing.
     */
    if (tds < 0.0f) tds = 0.0f;
    if (tds > profil->tdsTinggi_c) tds = profil->tdsTinggi_c;
    if (turbidity < 0.0f) turbidity = 0.0f;
    if (turbidity > profil->turbKeruh_c) turbidity = profil->turbKeruh_c;

    float tds_rendah = trapmf(tds, 0.0f, 0.0f, profil->tdsRendah_b, profil->tdsRendah_c);
    float tds_sedang = trimf(tds, profil->tdsSedang_a, profil->tdsSedang_b, profil->tdsSedang_c);
    float tds_tinggi = trapmf(tds, profil->tdsTinggi_a, profil->tdsTinggi_b, profil->tdsTinggi_c, profil->tdsTinggi_c);

    float turb_jernih = trapmf(turbidity, 0.0f, 0.0f, profil->turbJernih_b, profil->turbJernih_c);
    float turb_sedang = trimf(turbidity, profil->turbSedang_a, profil->turbSedang_b, profil->turbSedang_c);
    float turb_keruh  = trapmf(turbidity, profil->turbKeruh_a, profil->turbKeruh_b, profil->turbKeruh_c, profil->turbKeruh_c);

    /* --- 2. EVALUASI 9 RULE (AND = MIN) --- */
    float w[9];
    float z[9] = {
        OUT_EXCELLENT,    OUT_GOOD,         OUT_POOR,          /* rule 1-3: TDS Rendah */
        OUT_GOOD,         OUT_POOR,         OUT_VERY_POOR,     /* rule 4-6: TDS Sedang */
        OUT_POOR,         OUT_VERY_POOR,    OUT_NOT_SUITABLE   /* rule 7-9: TDS Tinggi */
    };

    w[0] = fmin2(tds_rendah, turb_jernih);  /* Rendah-Jernih -> EXCELLENT    (1.00) */
    w[1] = fmin2(tds_rendah, turb_sedang);  /* Rendah-Sedang -> GOOD         (0.75) */
    w[2] = fmin2(tds_rendah, turb_keruh);   /* Rendah-Keruh  -> POOR         (0.50) */
    w[3] = fmin2(tds_sedang, turb_jernih);  /* Sedang-Jernih -> GOOD         (0.75) */
    w[4] = fmin2(tds_sedang, turb_sedang);  /* Sedang-Sedang -> POOR         (0.50) */
    w[5] = fmin2(tds_sedang, turb_keruh);   /* Sedang-Keruh  -> VERY_POOR    (0.25) */
    w[6] = fmin2(tds_tinggi, turb_jernih);  /* Tinggi-Jernih -> POOR         (0.50) */
    w[7] = fmin2(tds_tinggi, turb_sedang);  /* Tinggi-Sedang -> VERY_POOR    (0.25) */
    w[8] = fmin2(tds_tinggi, turb_keruh);   /* Tinggi-Keruh  -> NOT_SUITABLE (0.00) */

    /* --- 3. DEFUZZIFIKASI (Weighted Average) --- */
    float sum_wz = 0.0f;
    float sum_w  = 0.0f;

    for (int i = 0; i < 9; i++) {
        sum_wz += w[i] * z[i];
        sum_w  += w[i];
    }

    if (sum_w < 1e-6f) {
        return OUT_NOT_SUITABLE;
    }

    return sum_wz / sum_w;
}

float FuzzyKualitasAir_HitungSkor(float tds, float turbidity)
{
    return FuzzyKualitasAir_HitungSkorProfil(&s_profilDefault, tds, turbidity);
}

/* ==========================================================================
 * FUNGSI KONVERSI SKOR -> LABEL STATUS (5 LEVEL)
 * ========================================================================== */
KualitasAir_t FuzzyKualitasAir_GetStatusProfil(const FuzzyProfil_t* profil, float skor)
{
    if (profil == NULL) {
        profil = &s_profilDefault;
    }

    if (skor >= profil->threshExcellent) {
        return STATUS_EXCELLENT;
    } else if (skor >= profil->threshGood) {
        return STATUS_GOOD;
    } else if (skor >= profil->threshPoor) {
        return STATUS_POOR;
    } else if (skor >= profil->threshVeryPoor) {
        return STATUS_VERY_POOR;
    } else {
        return STATUS_NOT_SUITABLE;
    }
}

KualitasAir_t FuzzyKualitasAir_GetStatus(float skor)
{
    return FuzzyKualitasAir_GetStatusProfil(&s_profilDefault, skor);
}

/* Teks pesan rekomendasi tindakan untuk OLED / Serial */
const char* FuzzyKualitasAir_GetPesan(KualitasAir_t status)
{
    switch (status) {
        case STATUS_EXCELLENT:    return "Air Sangat Baik & Layak";
        case STATUS_GOOD:         return "Air Baik / Layak Digunakan";
        case STATUS_POOR:         return "Perlu Filtrasi Ringan";
        case STATUS_VERY_POOR:    return "Butuh Filtrasi Intensif";
        case STATUS_NOT_SUITABLE: return "Tidak Lolos / Dilarang";
        default:                  return "Unknown";
    }
}

/* Teks badge ringkas untuk header / tabel */
const char* FuzzyKualitasAir_GetStatusBadge(KualitasAir_t status)
{
    switch (status) {
        case STATUS_EXCELLENT:    return "EXCELLENT";
        case STATUS_GOOD:         return "GOOD";
        case STATUS_POOR:         return "POOR";
        case STATUS_VERY_POOR:    return "V.POOR";
        case STATUS_NOT_SUITABLE: return "NOT-SUIT";
        default:                  return "UNKNOWN";
    }
}

/* String status suhu ("Dingin", "Normal", "Panas") */
const char* FuzzyKualitasAir_GetStatusSuhuStr(StatusSuhu_t status)
{
    switch (status) {
        case SUHU_DINGIN: return "Dingin";
        case SUHU_NORMAL: return "Normal";
        case SUHU_PANAS:  return "Panas";
        default:          return "Unknown";
    }
}

/* ==========================================================================
 * BAGIAN SUHU (DI LUAR FUZZY) - Kompensasi TDS & Status Suhu
 * ========================================================================== */

float FuzzyKualitasAir_KompensasiTDS(float tds_raw, float suhu)
{
    float faktor = 1.0f + 0.02f * (suhu - SUHU_REFERENSI);
    if (faktor < 0.1f) faktor = 0.1f;
    return tds_raw / faktor;
}

StatusSuhu_t FuzzyKualitasAir_CekStatusSuhu(float suhu)
{
    if (suhu <= SUHU_DINGIN_MAX) {
        return SUHU_DINGIN;
    } else if (suhu < SUHU_PANAS_MIN) {
        return SUHU_NORMAL;
    } else {
        return SUHU_PANAS;
    }
}
