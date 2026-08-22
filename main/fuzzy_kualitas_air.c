/* ==========================================================================
 * fuzzy_kualitas_air.c
 * Implementasi Fuzzy Sugeno Order-0 untuk klasifikasi kualitas air.
 *
 * 3 INPUT  : TDS (ppm) x Turbidity (NTU) x Suhu (Celsius)
 * 27 RULE  : 3 x 3 x 3, identik dengan kualitas_air.fis
 * OUTPUT   : skor 0.0 - 1.0, dipetakan ke 5 level kualitas
 *
 * PERINGATAN: tabel 27 rule di bawah adalah DRAF ASUMSI DESAIN, bukan
 * turunan dokumen baku mutu resmi. Lihat penjelasan lengkap di header
 * fuzzy_kualitas_air.h. WAJIB divalidasi client/pembimbing.
 * ========================================================================== */

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

/* ---------- BATAS SUHU UNTUK LABEL DISPLAY (CRISP, DI LUAR FUZZY) -------- */
#define SUHU_DINGIN_MAX   24.0f   /* <= 24 C -> label "Dingin"              */
#define SUHU_PANAS_MIN    32.0f   /* >= 32 C -> label "Panas"               */
#define SUHU_REFERENSI    25.0f   /* suhu acuan kompensasi TDS              */

/* --------------------------------------------------------------------------
 * PROFIL DEFAULT
 * Nilai MF di bawah HARUS identik dengan kualitas_air.fis:
 *   [Input1] TDS       Range=[0 1200]
 *   [Input2] Turbidity Range=[0 30]
 *   [Input3] Suhu      Range=[0 40]
 * ------------------------------------------------------------------------ */
static const FuzzyProfil_t s_profilDefault = {
    /* TDS: Rendah [0 0 150 300], Sedang [150 500 1000], Tinggi [500 1000 1200 1200] */
    150.0f, 300.0f,
    150.0f, 500.0f, 1000.0f,
    500.0f, 1000.0f, 1200.0f,

    /* Turbidity: Jernih [0 0 1.5 3], Sedang [1.5 10 25], Keruh [10 25 30 30] */
    1.5f, 3.0f,
    1.5f, 10.0f, 25.0f,
    10.0f, 25.0f, 30.0f,

    /* Suhu: Dingin [0 0 24 28], Normal [24 28 32], Panas [28 32 40 40] */
    24.0f, 28.0f,
    24.0f, 28.0f, 32.0f,
    28.0f, 32.0f, 40.0f,

    /* Thresholds */
    THRESH_EXCELLENT, THRESH_GOOD, THRESH_POOR, THRESH_VERY_POOR
};

/* --------------------------------------------------------------------------
 * TABEL 27 RULE (SUGENO ORDER-0)
 *
 * Indeks: zTable[TDS][Turbidity][Suhu]
 *   TDS        : 0 = Rendah, 1 = Sedang, 2 = Tinggi
 *   Turbidity  : 0 = Jernih, 1 = Sedang, 2 = Keruh
 *   Suhu       : 0 = Dingin, 1 = Normal, 2 = Panas
 *
 * Urutan ini SAMA dengan blok [Rules] pada kualitas_air.fis, sehingga
 * setiap baris .fis dapat dicocokkan langsung baris-per-baris.
 *
 * Skema asumsi: Suhu Normal = netral, Dingin/Panas = turun 1 tingkat.
 * ------------------------------------------------------------------------ */
static const float zTable[3][3][3] = {
    /* ---- TDS Rendah ---- */
    {
        /* Jernih : Dingin        Normal          Panas       */
        {  OUT_GOOD,       OUT_EXCELLENT,  OUT_GOOD       },
        /* Sedang */
        {  OUT_POOR,       OUT_GOOD,       OUT_POOR       },
        /* Keruh  */
        {  OUT_VERY_POOR,  OUT_POOR,       OUT_VERY_POOR  }
    },
    /* ---- TDS Sedang ---- */
    {
        /* Jernih */
        {  OUT_POOR,       OUT_GOOD,       OUT_POOR       },
        /* Sedang */
        {  OUT_VERY_POOR,  OUT_POOR,       OUT_VERY_POOR  },
        /* Keruh  */
        {  OUT_NOT_SUITABLE, OUT_VERY_POOR, OUT_NOT_SUITABLE }
    },
    /* ---- TDS Tinggi ---- */
    {
        /* Jernih */
        {  OUT_VERY_POOR,  OUT_POOR,       OUT_VERY_POOR  },
        /* Sedang */
        {  OUT_NOT_SUITABLE, OUT_VERY_POOR, OUT_NOT_SUITABLE },
        /* Keruh  */
        {  OUT_NOT_SUITABLE, OUT_NOT_SUITABLE, OUT_NOT_SUITABLE }
    }
};

const FuzzyProfil_t* FuzzyKualitasAir_ProfilDefault(void)
{
    return &s_profilDefault;
}

float FuzzyKualitasAir_SuhuNetral(void)
{
    /* Puncak MF Normal: suhu yang tidak memicu penalti Dingin maupun Panas. */
    return s_profilDefault.tempNormal_b;
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
 * FUNGSI UTAMA: Hitung skor fuzzy (0.0 - 1.0) dari TDS, Turbidity, dan Suhu
 * ========================================================================== */
float FuzzyKualitasAir_HitungSkorProfil(const FuzzyProfil_t* profil,
                                         float tds, float turbidity, float suhu)
{
    if (profil == NULL) {
        profil = &s_profilDefault;
    }

    /* --- 1. CLAMP INPUT KE RENTANG SEMESTA ---
     * Nilai di luar batas dijepit agar tidak menghasilkan zero-firing
     * (seluruh rule bernilai 0 sehingga skor tidak dapat dihitung).
     */
    if (tds < 0.0f) tds = 0.0f;
    if (tds > profil->tdsTinggi_c) tds = profil->tdsTinggi_c;

    if (turbidity < 0.0f) turbidity = 0.0f;
    if (turbidity > profil->turbKeruh_c) turbidity = profil->turbKeruh_c;

    if (suhu < 0.0f) suhu = 0.0f;
    if (suhu > profil->tempPanas_c) suhu = profil->tempPanas_c;

    /* --- 2. FUZZIFIKASI KETIGA INPUT --- */
    const float mfTds[3] = {
        trapmf(tds, 0.0f, 0.0f, profil->tdsRendah_b, profil->tdsRendah_c),
        trimf(tds, profil->tdsSedang_a, profil->tdsSedang_b, profil->tdsSedang_c),
        trapmf(tds, profil->tdsTinggi_a, profil->tdsTinggi_b,
               profil->tdsTinggi_c, profil->tdsTinggi_c)
    };

    const float mfTurb[3] = {
        trapmf(turbidity, 0.0f, 0.0f, profil->turbJernih_b, profil->turbJernih_c),
        trimf(turbidity, profil->turbSedang_a, profil->turbSedang_b, profil->turbSedang_c),
        trapmf(turbidity, profil->turbKeruh_a, profil->turbKeruh_b,
               profil->turbKeruh_c, profil->turbKeruh_c)
    };

    const float mfSuhu[3] = {
        trapmf(suhu, 0.0f, 0.0f, profil->tempDingin_b, profil->tempDingin_c),
        trimf(suhu, profil->tempNormal_a, profil->tempNormal_b, profil->tempNormal_c),
        trapmf(suhu, profil->tempPanas_a, profil->tempPanas_b,
               profil->tempPanas_c, profil->tempPanas_c)
    };

    /* --- 3. EVALUASI 27 RULE (AND = MIN) + DEFUZZIFIKASI (Weighted Average) --- */
    float sum_wz = 0.0f;
    float sum_w  = 0.0f;

    for (int t = 0; t < 3; t++) {
        if (mfTds[t] <= 0.0f) continue;          /* rule pasti tidak menyala */
        for (int b = 0; b < 3; b++) {
            if (mfTurb[b] <= 0.0f) continue;
            const float wPartial = fmin2(mfTds[t], mfTurb[b]);
            for (int s = 0; s < 3; s++) {
                if (mfSuhu[s] <= 0.0f) continue;
                const float w = fmin2(wPartial, mfSuhu[s]);
                sum_wz += w * zTable[t][b][s];
                sum_w  += w;
            }
        }
    }

    if (sum_w < 1e-6f) {
        return OUT_NOT_SUITABLE;
    }

    return sum_wz / sum_w;
}

float FuzzyKualitasAir_HitungSkor(float tds, float turbidity, float suhu)
{
    return FuzzyKualitasAir_HitungSkorProfil(&s_profilDefault, tds, turbidity, suhu);
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
 * BAGIAN SUHU TAMBAHAN - Kompensasi TDS & Label Status Suhu
 *
 * Catatan: suhu kini juga menjadi INPUT FUZZY ke-3. Fungsi di bawah tetap
 * dipertahankan untuk dua keperluan yang berbeda:
 *   - KompensasiTDS()  : normalisasi konduktivitas ke suhu acuan 25 C
 *   - CekStatusSuhu()  : label tegas (Dingin/Normal/Panas) untuk ditampilkan
 *                        di OLED, bukan untuk perhitungan skor fuzzy.
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
