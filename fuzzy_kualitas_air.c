#include "fuzzy_kualitas_air.h"
#include <math.h>

/* ---------- KONFIGURASI BREAKPOINT (HASIL DARI TUNING DI MATLAB) -------- */
/* TDS (ppm) */
#define TDS_RENDAH_A   0.0f
#define TDS_RENDAH_B   0.0f
#define TDS_RENDAH_C   300.0f

#define TDS_SEDANG_A   0.0f
#define TDS_SEDANG_B   300.0f
#define TDS_SEDANG_C   1000.0f

#define TDS_TINGGI_A   300.0f
#define TDS_TINGGI_B   1000.0f
#define TDS_TINGGI_C   1200.0f

/* Turbidity (NTU) */
#define TURB_RENDAH_A  0.0f
#define TURB_RENDAH_B  0.0f
#define TURB_RENDAH_C  5.0f

#define TURB_SEDANG_A  0.0f
#define TURB_SEDANG_B  5.0f
#define TURB_SEDANG_C  25.0f

#define TURB_TINGGI_A  5.0f
#define TURB_TINGGI_B  25.0f
#define TURB_TINGGI_C  30.0f

/* Output constant (Sugeno order-0) */
#define OUT_LAYAK      100.0f
#define OUT_LTM        50.0f
#define OUT_TL         0.0f

/* Threshold label akhir */
#define THRESH_LAYAK   75.0f
#define THRESH_LTM     25.0f

/* Suhu (di luar fuzzy, IF-ELSE biasa) */
#define SUHU_NORMAL_MIN 25.0f
#define SUHU_NORMAL_MAX 31.0f
#define SUHU_REFERENSI  25.0f   /* referensi kompensasi TDS */

/* ---------- FUNGSI MEMBERSHIP FUNCTION SEGITIGA (trimf) ----------------- */
static float trimf(float x, float a, float b, float c)
{
    if (x < a || x > c) return 0.0f;
    if (x == b) return 1.0f;
    if (x < b)  return (a == b) ? 1.0f : (x - a) / (b - a);
    return (b == c) ? 1.0f : (c - x) / (c - b);
}

/* ---------- FUNGSI MIN & MAX SEDERHANA ----------------------------------- */
static float fmin2(float a, float b) { return (a < b) ? a : b; }

/* ==========================================================================
 * FUNGSI UTAMA: hitung skor fuzzy (0-100) dari nilai TDS & Turbidity
 * ========================================================================== */
float FuzzyKualitasAir_HitungSkor(float tds, float turbidity)
{
    /* --- 1. FUZZIFIKASI --- */
    float tds_rendah  = trimf(tds, TDS_RENDAH_A, TDS_RENDAH_B, TDS_RENDAH_C);
    float tds_sedang  = trimf(tds, TDS_SEDANG_A, TDS_SEDANG_B, TDS_SEDANG_C);
    float tds_tinggi  = trimf(tds, TDS_TINGGI_A, TDS_TINGGI_B, TDS_TINGGI_C);

    float turb_rendah = trimf(turbidity, TURB_RENDAH_A, TURB_RENDAH_B, TURB_RENDAH_C);
    float turb_sedang = trimf(turbidity, TURB_SEDANG_A, TURB_SEDANG_B, TURB_SEDANG_C);
    float turb_tinggi = trimf(turbidity, TURB_TINGGI_A, TURB_TINGGI_B, TURB_TINGGI_C);

    /* --- 2. EVALUASI 9 RULE (AND = MIN) --- */
    float w[9];
    float z[9] = {
        OUT_LAYAK, OUT_LTM,   OUT_TL,     /* rule 1-3: TDS Rendah */
        OUT_LTM,   OUT_LTM,   OUT_TL,     /* rule 4-6: TDS Sedang */
        OUT_TL,    OUT_TL,    OUT_TL      /* rule 7-9: TDS Tinggi */
    };

    w[0] = fmin2(tds_rendah, turb_rendah);   /* Rendah-Rendah -> LAYAK */
    w[1] = fmin2(tds_rendah, turb_sedang);   /* Rendah-Sedang -> LTM   */
    w[2] = fmin2(tds_rendah, turb_tinggi);   /* Rendah-Tinggi -> TL    */
    w[3] = fmin2(tds_sedang, turb_rendah);   /* Sedang-Rendah -> LTM   */
    w[4] = fmin2(tds_sedang, turb_sedang);   /* Sedang-Sedang -> LTM   */
    w[5] = fmin2(tds_sedang, turb_tinggi);   /* Sedang-Tinggi -> TL    */
    w[6] = fmin2(tds_tinggi, turb_rendah);   /* Tinggi-Rendah -> TL    */
    w[7] = fmin2(tds_tinggi, turb_sedang);   /* Tinggi-Sedang -> TL    */
    w[8] = fmin2(tds_tinggi, turb_tinggi);   /* Tinggi-Tinggi -> TL    */

    /* --- 3. DEFUZZIFIKASI (Weighted Average) --- */
    float sum_wz = 0.0f;
    float sum_w  = 0.0f;

    for (int i = 0; i < 9; i++) {
        sum_wz += w[i] * z[i];
        sum_w  += w[i];
    }

    /* Hindari pembagian nol (kalau kebetulan semua w = 0, seharusnya
       tidak mungkin terjadi karena 3 MF saling overlap penuh di tiap
       range, tapi tetap dijaga untuk keamanan) */
    if (sum_w < 1e-6f) {
        return 0.0f;
    }

    return sum_wz / sum_w;
}

/* ==========================================================================
 * FUNGSI KONVERSI SKOR -> LABEL STATUS
 * ========================================================================== */
KualitasAir_t FuzzyKualitasAir_GetStatus(float skor)
{
    if (skor >= THRESH_LAYAK) {
        return STATUS_LAYAK;
    } else if (skor >= THRESH_LTM) {
        return STATUS_LTM;
    } else {
        return STATUS_TL;
    }
}

/* Opsional: dapatkan teks pesan untuk ditampilkan ke LCD/OLED */
const char* FuzzyKualitasAir_GetPesan(KualitasAir_t status)
{
    switch (status) {
        case STATUS_LAYAK: return "Air Aman";
        case STATUS_LTM:   return "Ganti Filter/Endapkan Sedimen";
        case STATUS_TL:    return "Bahaya/Dilarang Digunakan";
        default:            return "Unknown";
    }
}

/* ==========================================================================
 * BAGIAN SUHU (DI LUAR FUZZY) - Kompensasi TDS & Status Suhu
 * ========================================================================== */

/* Kompensasi nilai TDS terhadap suhu (rumus umum sensor TDS,
   referensi 25 C, koefisien 0.02 per derajat - sesuaikan dengan
   datasheet sensor TDS yang kamu pakai kalau berbeda) */
float FuzzyKualitasAir_KompensasiTDS(float tds_raw, float suhu)
{
    float faktor = 1.0f + 0.02f * (suhu - SUHU_REFERENSI);
    if (faktor < 0.1f) faktor = 0.1f;  /* proteksi pembagian nol/ekstrem */
    return tds_raw / faktor;
}

/* Cek status suhu (IF-ELSE biasa, TIDAK masuk ke fuzzy engine) */
StatusSuhu_t FuzzyKualitasAir_CekStatusSuhu(float suhu)
{
    if (suhu >= SUHU_NORMAL_MIN && suhu <= SUHU_NORMAL_MAX) {
        return SUHU_NORMAL;
    }
    return SUHU_ABNORMAL;
}

/* ==========================================================================
 * CONTOH PEMAKAIAN DI MAIN LOOP (pseudo-code, sesuaikan dengan program
 * hardware kamu yang sudah ada - HAL_ADC_GetValue, dsb)
 * ==========================================================================

while (1)
{
    float suhu_raw   = Baca_Sensor_DS18B20();      // fungsi kamu sendiri
    float tds_raw    = Baca_Sensor_TDS();           // fungsi kamu sendiri
    float turbidity  = Baca_Sensor_Turbidity();      // fungsi kamu sendiri

    // 1. Kompensasi TDS terhadap suhu
    float tds_compensated = FuzzyKualitasAir_KompensasiTDS(tds_raw, suhu_raw);

    // 2. Hitung skor fuzzy
    float skor = FuzzyKualitasAir_HitungSkor(tds_compensated, turbidity);

    // 3. Konversi ke status/label
    KualitasAir_t status = FuzzyKualitasAir_GetStatus(skor);
    const char* pesan = FuzzyKualitasAir_GetPesan(status);

    // 4. Cek status suhu terpisah (untuk ditampilkan sebagai info tambahan)
    StatusSuhu_t statusSuhu = FuzzyKualitasAir_CekStatusSuhu(suhu_raw);

    // 5. Tampilkan ke LCD/OLED (sesuaikan fungsi display kamu)
    Tampilkan_LCD(tds_compensated, turbidity, suhu_raw, skor, pesan, statusSuhu);

    HAL_Delay(1000); // sesuaikan interval pembacaan
}

========================================================================== */
