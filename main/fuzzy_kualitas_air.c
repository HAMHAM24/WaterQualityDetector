/* Fuzzy Sugeno orde-0: 4 membership x 3 input = 64 rule. */
#include "fuzzy_kualitas_air.h"
#include <stddef.h>

static float trapmf(float x, float a, float b, float c, float d) {
    if (x < a || x > d) return 0.0f;
    if (x >= b && x <= c) return 1.0f;
    if (x < b) return (a == b) ? 1.0f : (x - a) / (b - a);
    return (c == d) ? 1.0f : (d - x) / (d - c);
}

static float trimf(float x, float a, float b, float c) {
    if (x < a || x > c) return 0.0f;
    if (x == b) return 1.0f;
    if (x < b) return (x - a) / (b - a);
    return (c - x) / (c - b);
}

static float outputForSeverity(uint8_t severity) {
    static const float outputs[] = { 1.00f, 0.67f, 0.33f, 0.00f };
    return outputs[severity > 3 ? 3 : severity];
}

float FuzzyKualitasAir_HitungSkor_AirMinum(const FuzzyProfil_t* p, float tds,
                                            float turbidity, float deltaSuhu) {
    if (p == NULL) return 0.0f;
    if (tds < 0.0f) tds = 0.0f; if (tds > p->tdsMax) tds = p->tdsMax;
    if (turbidity < 0.0f) turbidity = 0.0f; if (turbidity > p->turbMax) turbidity = p->turbMax;
    if (deltaSuhu < 0.0f) deltaSuhu = 0.0f; if (deltaSuhu > p->tempMax) deltaSuhu = p->tempMax;

    const float mt[4] = {
        trapmf(tds, 0, 0, p->tdsSl_b, p->tdsSl_c),
        trimf(tds, p->tdsPs_a, p->tdsPs_b, p->tdsPs_c),
        trimf(tds, p->tdsPi_a, p->tdsPi_b, p->tdsPi_c),
        trapmf(tds, p->tdsTl_a, p->tdsTl_b, p->tdsMax, p->tdsMax)
    };
    const float mb[4] = {
        trapmf(turbidity, 0, 0, p->turbSl_b, p->turbSl_c),
        trimf(turbidity, p->turbPs_a, p->turbPs_b, p->turbPs_c),
        trimf(turbidity, p->turbPi_a, p->turbPi_b, p->turbPi_c),
        trapmf(turbidity, p->turbTl_a, p->turbTl_b, p->turbMax, p->turbMax)
    };
    const float ms[4] = {
        trapmf(deltaSuhu, 0, 0, p->tempSl_b, p->tempSl_c),
        trimf(deltaSuhu, p->tempPs_a, p->tempPs_b, p->tempPs_c),
        trimf(deltaSuhu, p->tempPi_a, p->tempPi_b, p->tempPi_c),
        trapmf(deltaSuhu, p->tempTl_a, p->tempTl_b, p->tempMax, p->tempMax)
    };

    float weighted = 0.0f, total = 0.0f;
    for (uint8_t s = 0; s < 4; ++s) for (uint8_t t = 0; t < 4; ++t)
    for (uint8_t b = 0; b < 4; ++b) {
        float w = mt[t]; if (mb[b] < w) w = mb[b]; if (ms[s] < w) w = ms[s];
        uint8_t worst = t > b ? t : b; if (s > worst) worst = s;
        weighted += w * outputForSeverity(worst); total += w;
    }
    return total > 0.0f ? weighted / total : 0.0f;
}

float FuzzyKualitasAir_HitungSkor_Higiene(const FuzzyProfil_t* p, float tds, float turbidity) {
    return FuzzyKualitasAir_HitungSkor_AirMinum(p, tds, turbidity, 0.0f);
}

float FuzzyKualitasAir_HitungSkor_Pemandian(const FuzzyProfil_t* p, float suhu, float turbidity) {
    return FuzzyKualitasAir_HitungSkor_AirMinum(p, 0.0f, turbidity, suhu);
}

KualitasAir_t FuzzyKualitasAir_GetStatusProfil(const FuzzyProfil_t* p, float score) {
    if (p == NULL) return STATUS_TIDAK_LOLOS;
    if (score >= p->threshSangatLayak) return STATUS_SANGAT_LAYAK;
    if (score >= p->threshProsesSedang) return STATUS_PROSES_SEDANG;
    if (score >= p->threshProsesIntensif) return STATUS_PROSES_INTENSIF;
    return STATUS_TIDAK_LOLOS;
}

const char* FuzzyKualitasAir_GetPesan(KualitasAir_t s) {
    switch (s) {
        case STATUS_SANGAT_LAYAK: return "Air Sangat Layak";
        case STATUS_PROSES_SEDANG: return "Perlu proses sedang";
        case STATUS_PROSES_INTENSIF: return "Perlu proses intensif";
        default: return "Tidak layak digunakan";
    }
}

const char* FuzzyKualitasAir_GetStatusBadge(KualitasAir_t s) {
    switch (s) {
        case STATUS_SANGAT_LAYAK: return "S.LAYAK";
        case STATUS_PROSES_SEDANG: return "P.SED";
        case STATUS_PROSES_INTENSIF: return "P.INT";
        default: return "T.LOLOS";
    }
}

const char* FuzzyKualitasAir_GetStatusSuhuStr(StatusSuhu_t s) {
    static const char* const labels[] = { "S.Layak", "P.Sed", "P.Int", "T.Lolos" };
    return labels[s > SUHU_TL ? SUHU_TL : s];
}

StatusSuhu_t FuzzyKualitasAir_CekStatusSuhu(float d, const FuzzyProfil_t* p) {
    if (p == NULL) return SUHU_TL;
    const float m[4] = {
        trapmf(d, 0, 0, p->tempSl_b, p->tempSl_c),
        trimf(d, p->tempPs_a, p->tempPs_b, p->tempPs_c),
        trimf(d, p->tempPi_a, p->tempPi_b, p->tempPi_c),
        trapmf(d, p->tempTl_a, p->tempTl_b, p->tempMax, p->tempMax)
    };
    uint8_t best = 0;
    for (uint8_t i = 1; i < 4; ++i) if (m[i] >= m[best]) best = i;
    return (StatusSuhu_t)best;
}

ThresholdResult_t Threshold_CekPemandianKolam(float suhu, float turbidity) {
    ThresholdResult_t r;
    r.suhuAman = suhu >= 16.0f && suhu <= 35.0f;
    r.turbidityAman = turbidity < 0.5f;
    r.semuaAman = r.suhuAman && r.turbidityAman;
    return r;
}
