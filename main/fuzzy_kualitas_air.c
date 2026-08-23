/* ==========================================================================
 * fuzzy_kualitas_air.c
 * Implementasi Fuzzy Sugeno Order-0 untuk klasifikasi kualitas air.
 * Mengadaptasi prinsip "limiting parameter" sesuai matriks severity.
 * ========================================================================== */

#include "fuzzy_kualitas_air.h"
#include <math.h>
#include <stddef.h>   /* NULL */

/* ---------- OUTPUT CONSTANTS (SUGENO ORDER-0) ---------------- */
#define OUT_SANGAT_LAYAK          1.00f
#define OUT_LAYAK_SARING_RINGAN   0.75f
#define OUT_CUKUP_PROSES_SEDANG   0.50f
#define OUT_KRITIS_PROSES_INTENSIF 0.25f
#define OUT_TIDAK_LOLOS           0.00f

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

static float fmin2(float a, float b) { return (a < b) ? a : b; }

/* Fungsi bantu menghitung output Z berdasarkan aturan Limiting Parameter */
static float hitungOutput_2Input(int s1, int s2) {
    int max_sev = (s1 > s2) ? s1 : s2;
    int count_max = 0;
    if (s1 == max_sev) count_max++;
    if (s2 == max_sev) count_max++;

    if (max_sev == 0) return OUT_SANGAT_LAYAK;
    if (max_sev == 1 && count_max == 1) return OUT_LAYAK_SARING_RINGAN;
    if (max_sev == 1 && count_max >= 2) return OUT_CUKUP_PROSES_SEDANG;
    if (max_sev == 2 && count_max == 1) return OUT_KRITIS_PROSES_INTENSIF;
    return OUT_TIDAK_LOLOS;
}

static float hitungOutput_3Input(int s1, int s2, int s3) {
    int max_sev = (s1 > s2) ? s1 : s2;
    max_sev = (s3 > max_sev) ? s3 : max_sev;
    
    int count_max = 0;
    if (s1 == max_sev) count_max++;
    if (s2 == max_sev) count_max++;
    if (s3 == max_sev) count_max++;

    if (max_sev == 0) return OUT_SANGAT_LAYAK;
    if (max_sev == 1 && count_max == 1) return OUT_LAYAK_SARING_RINGAN;
    if (max_sev == 1 && count_max >= 2) return OUT_CUKUP_PROSES_SEDANG;
    if (max_sev == 2 && count_max == 1) return OUT_KRITIS_PROSES_INTENSIF;
    return OUT_TIDAK_LOLOS;
}

/* ==========================================================================
 * 1. AIR MINUM (3 INPUT: TDS, Turb, dTemp)
 * ========================================================================== */
float FuzzyKualitasAir_HitungSkor_AirMinum(const FuzzyProfil_t* profil, float tds, float turbidity, float dTemp)
{
    if (profil == NULL) return 0.0f;

    if (tds < 0.0f) tds = 0.0f;
    if (tds > profil->tds2_max) tds = profil->tds2_max;
    if (turbidity < 0.0f) turbidity = 0.0f;
    if (turbidity > profil->turb2_max) turbidity = profil->turb2_max;
    if (dTemp < 0.0f) dTemp = 0.0f;
    if (dTemp > profil->temp2_max) dTemp = profil->temp2_max;

    const float mfTds[3] = {
        trapmf(tds, 0.0f, 0.0f, profil->tds0_b, profil->tds0_c),
        trimf(tds, profil->tds1_a, profil->tds1_b, profil->tds1_c),
        trapmf(tds, profil->tds2_a, profil->tds2_b, profil->tds2_max, profil->tds2_max)
    };

    const float mfTurb[3] = {
        trapmf(turbidity, 0.0f, 0.0f, profil->turb0_b, profil->turb0_c),
        trimf(turbidity, profil->turb1_a, profil->turb1_b, profil->turb1_c),
        trapmf(turbidity, profil->turb2_a, profil->turb2_b, profil->turb2_max, profil->turb2_max)
    };

    const float mfTemp[3] = {
        trapmf(dTemp, 0.0f, 0.0f, profil->temp0_b, profil->temp0_c),
        trimf(dTemp, profil->temp1_a, profil->temp1_b, profil->temp1_c),
        trapmf(dTemp, profil->temp2_a, profil->temp2_b, profil->temp2_max, profil->temp2_max)
    };

    float sum_wz = 0.0f;
    float sum_w  = 0.0f;

    for (int t = 0; t < 3; t++) {
        if (mfTds[t] <= 0.0f) continue;
        for (int b = 0; b < 3; b++) {
            if (mfTurb[b] <= 0.0f) continue;
            for (int s = 0; s < 3; s++) {
                if (mfTemp[s] <= 0.0f) continue;
                
                float w = fmin2(mfTds[t], fmin2(mfTurb[b], mfTemp[s]));
                float z = hitungOutput_3Input(t, b, s);
                sum_wz += w * z;
                sum_w  += w;
            }
        }
    }

    if (sum_w < 1e-6f) return OUT_TIDAK_LOLOS;
    return sum_wz / sum_w;
}

/* ==========================================================================
 * 2. HIGIENE SANITASI (2 INPUT: TDS, Turb)
 * ========================================================================== */
float FuzzyKualitasAir_HitungSkor_Higiene(const FuzzyProfil_t* profil, float tds, float turbidity)
{
    if (profil == NULL) return 0.0f;

    if (tds < 0.0f) tds = 0.0f;
    if (tds > profil->tds2_max) tds = profil->tds2_max;
    if (turbidity < 0.0f) turbidity = 0.0f;
    if (turbidity > profil->turb2_max) turbidity = profil->turb2_max;

    const float mfTds[3] = {
        trapmf(tds, 0.0f, 0.0f, profil->tds0_b, profil->tds0_c),
        trimf(tds, profil->tds1_a, profil->tds1_b, profil->tds1_c),
        trapmf(tds, profil->tds2_a, profil->tds2_b, profil->tds2_max, profil->tds2_max)
    };

    const float mfTurb[3] = {
        trapmf(turbidity, 0.0f, 0.0f, profil->turb0_b, profil->turb0_c),
        trimf(turbidity, profil->turb1_a, profil->turb1_b, profil->turb1_c),
        trapmf(turbidity, profil->turb2_a, profil->turb2_b, profil->turb2_max, profil->turb2_max)
    };

    float sum_wz = 0.0f;
    float sum_w  = 0.0f;

    for (int t = 0; t < 3; t++) {
        if (mfTds[t] <= 0.0f) continue;
        for (int b = 0; b < 3; b++) {
            if (mfTurb[b] <= 0.0f) continue;
            
            float w = fmin2(mfTds[t], mfTurb[b]);
            float z = hitungOutput_2Input(t, b);
            sum_wz += w * z;
            sum_w  += w;
        }
    }

    if (sum_w < 1e-6f) return OUT_TIDAK_LOLOS;
    return sum_wz / sum_w;
}

/* ==========================================================================
 * 3. PEMANDIAN UMUM (2 INPUT: dTemp, Turb) - TDS DIBYPASS
 * ========================================================================== */
float FuzzyKualitasAir_HitungSkor_Pemandian(const FuzzyProfil_t* profil, float dTemp, float turbidity)
{
    if (profil == NULL) return 0.0f;

    if (turbidity < 0.0f) turbidity = 0.0f;
    if (turbidity > profil->turb2_max) turbidity = profil->turb2_max;
    if (dTemp < 0.0f) dTemp = 0.0f;
    if (dTemp > profil->temp2_max) dTemp = profil->temp2_max;

    const float mfTurb[3] = {
        trapmf(turbidity, 0.0f, 0.0f, profil->turb0_b, profil->turb0_c),
        trimf(turbidity, profil->turb1_a, profil->turb1_b, profil->turb1_c),
        trapmf(turbidity, profil->turb2_a, profil->turb2_b, profil->turb2_max, profil->turb2_max)
    };

    const float mfTemp[3] = {
        trapmf(dTemp, 0.0f, 0.0f, profil->temp0_b, profil->temp0_c),
        trimf(dTemp, profil->temp1_a, profil->temp1_b, profil->temp1_c),
        trapmf(dTemp, profil->temp2_a, profil->temp2_b, profil->temp2_max, profil->temp2_max)
    };

    float sum_wz = 0.0f;
    float sum_w  = 0.0f;

    for (int b = 0; b < 3; b++) {
        if (mfTurb[b] <= 0.0f) continue;
        for (int s = 0; s < 3; s++) {
            if (mfTemp[s] <= 0.0f) continue;
            
            float w = fmin2(mfTurb[b], mfTemp[s]);
            float z = hitungOutput_2Input(b, s); // b=Turbidity, s=Suhu
            sum_wz += w * z;
            sum_w  += w;
        }
    }

    if (sum_w < 1e-6f) return OUT_TIDAK_LOLOS;
    return sum_wz / sum_w;
}

/* ==========================================================================
 * FUNGSI KONVERSI SKOR -> LABEL STATUS
 * ========================================================================== */
KualitasAir_t FuzzyKualitasAir_GetStatusProfil(const FuzzyProfil_t* profil, float skor)
{
    if (profil == NULL) {
        return STATUS_TIDAK_LOLOS;
    }

    if (skor >= profil->threshSangatLayak) {
        return STATUS_SANGAT_LAYAK;
    } else if (skor >= profil->threshLayakSaring) {
        return STATUS_LAYAK_SARING_RINGAN;
    } else if (skor >= profil->threshCukup) {
        return STATUS_CUKUP_PROSES_SEDANG;
    } else if (skor >= profil->threshKritis) {
        return STATUS_KRITIS;
    } else {
        return STATUS_TIDAK_LOLOS;
    }
}

const char* FuzzyKualitasAir_GetPesan(KualitasAir_t status)
{
    switch (status) {
        case STATUS_SANGAT_LAYAK:       return "Air Sangat Layak";
        case STATUS_LAYAK_SARING_RINGAN:return "Layak, Saring Ringan";
        case STATUS_CUKUP_PROSES_SEDANG:return "Cukup, Perlu Olah Air";
        case STATUS_KRITIS:             return "Kurang, Butuh Saring Total";
        case STATUS_TIDAK_LOLOS:        return "Tidak Layak / Dilarang";
        default:                        return "Unknown";
    }
}

const char* FuzzyKualitasAir_GetStatusBadge(KualitasAir_t status)
{
    switch (status) {
        case STATUS_SANGAT_LAYAK:       return "S.LAYAK";
        case STATUS_LAYAK_SARING_RINGAN:return "LAYAK";
        case STATUS_CUKUP_PROSES_SEDANG:return "CUKUP";
        case STATUS_KRITIS:             return "KURANG";
        case STATUS_TIDAK_LOLOS:        return "T.LAYAK";
        default:                        return "UNKNOWN";
    }
}

const char* FuzzyKualitasAir_GetStatusSuhuStr(StatusSuhu_t status)
{
    switch (status) {
        case SUHU_IDEAL:      return "Ideal";
        case SUHU_MENYIMPANG: return "Batas";
        case SUHU_EKSTREM:    return "Ekstr";
        default:              return "Unknown";
    }
}

StatusSuhu_t FuzzyKualitasAir_CekStatusSuhu(float dTemp, const FuzzyProfil_t* profil)
{
    if (profil == NULL) return SUHU_EKSTREM;

    float ide = trapmf(dTemp, 0.0f, 0.0f, profil->temp0_b, profil->temp0_c);
    float meny = trimf(dTemp, profil->temp1_a, profil->temp1_b, profil->temp1_c);
    float eks = trapmf(dTemp, profil->temp2_a, profil->temp2_b, profil->temp2_max, profil->temp2_max);

    if (ide >= meny && ide >= eks) return SUHU_IDEAL;
    if (meny >= ide && meny >= eks) return SUHU_MENYIMPANG;
    return SUHU_EKSTREM;
}

ThresholdResult_t Threshold_CekPemandianKolam(float suhu, float turbidity)
{
    ThresholdResult_t res;
    // Ambang batas gabungan konservatif: Suhu 16-35 C, Turbidity < 0.5 NTU
    res.suhuAman = (suhu >= 16.0f && suhu <= 35.0f);
    res.turbidityAman = (turbidity < 0.5f);
    res.semuaAman = res.suhuAman && res.turbidityAman;
    return res;
}

float FuzzyKualitasAir_KompensasiTDS(float tds_raw, float suhu_aktual)
{
    // Menggunakan referensi standar kompensasi 25 derajat Celsius
    float faktor = 1.0f + 0.02f * (suhu_aktual - 25.0f);
    if (faktor < 0.1f) faktor = 0.1f;
    return tds_raw / faktor;
}
