#include "fuzzy_kualitas_air.h"
#include <math.h>
#include <stddef.h>   /* NULL */

/* ---------- KONFIGURASI BREAKPOINT (HASIL DARI TUNING DI MATLAB) --------
 * Nilai di bawah ini adalah profil Higiene Sanitasi dan identik dengan isi
 * kualitas_air.fis, sehingga uji validasi baseline (TDS=350, Turbidity=10
 * -> skor 32.8) tetap menghasilkan angka yang sama.
 * Profil peruntukan air lain didefinisikan di config.h.
 */
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

/* Profil bawaan: Higiene Sanitasi. Nilainya sama dengan makro di atas. */
static const FuzzyProfil_t s_profilDefault = {
    TDS_RENDAH_C,  TDS_SEDANG_C,  TDS_TINGGI_C,
    TURB_RENDAH_C, TURB_SEDANG_C, TURB_TINGGI_C,
    THRESH_LAYAK,  THRESH_LTM
};

const FuzzyProfil_t* FuzzyKualitasAir_ProfilDefault(void)
{
    return &s_profilDefault;
}

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
 * dengan breakpoint mengikuti profil baku mutu yang aktif.
 * ========================================================================== */
float FuzzyKualitasAir_HitungSkorProfil(const FuzzyProfil_t* profil,
                                         float tds, float turbidity)
{
    if (profil == NULL) {
        profil = &s_profilDefault;
    }

    /* --- 1. FUZZIFIKASI ---
     * Nilai input dijepit ke batas atas semesta agar pembacaan sensor yang
     * melampaui rentang (mis. TDS 5000 ppm) tetap menghasilkan derajat
     * keanggotaan "Tinggi" = 1 dan bukan 0 (yang akan membuat seluruh rule
     * mati dan skor jatuh ke nilai aman yang keliru).
     */
    if (tds < 0.0f) tds = 0.0f;
    if (tds > profil->tdsC) tds = profil->tdsC;
    if (turbidity < 0.0f) turbidity = 0.0f;
    if (turbidity > profil->turbC) turbidity = profil->turbC;

    float tds_rendah  = trimf(tds, 0.0f,          0.0f,          profil->tdsA);
    float tds_sedang  = trimf(tds, 0.0f,          profil->tdsA,  profil->tdsB);
    float tds_tinggi  = trimf(tds, profil->tdsA,  profil->tdsB,  profil->tdsC);

    float turb_rendah = trimf(turbidity, 0.0f,           0.0f,           profil->turbA);
    float turb_sedang = trimf(turbidity, 0.0f,           profil->turbA,  profil->turbB);
    float turb_tinggi = trimf(turbidity, profil->turbA,  profil->turbB,  profil->turbC);

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

    /* Hindari pembagian nol. Karena input sudah dijepit ke semesta dan
       ketiga MF saling overlap penuh, kondisi ini praktis tidak tercapai,
       tetapi tetap dijaga. Nilai 0.0f dipilih sebagai fail-safe: lebih baik
       melaporkan "tidak layak" daripada "aman" saat perhitungan gagal. */
    if (sum_w < 1e-6f) {
        return 0.0f;
    }

    return sum_wz / sum_w;
}

float FuzzyKualitasAir_HitungSkor(float tds, float turbidity)
{
    return FuzzyKualitasAir_HitungSkorProfil(&s_profilDefault, tds, turbidity);
}

/* ==========================================================================
 * FUNGSI KONVERSI SKOR -> LABEL STATUS
 * ========================================================================== */
KualitasAir_t FuzzyKualitasAir_GetStatusProfil(const FuzzyProfil_t* profil, float skor)
{
    if (profil == NULL) {
        profil = &s_profilDefault;
    }

    if (skor >= profil->threshLayak) {
        return STATUS_LAYAK;
    } else if (skor >= profil->threshLTM) {
        return STATUS_LTM;
    } else {
        return STATUS_TL;
    }
}

KualitasAir_t FuzzyKualitasAir_GetStatus(float skor)
{
    return FuzzyKualitasAir_GetStatusProfil(&s_profilDefault, skor);
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
   datasheet sensor TDS yang kamu pakai kalau berbeda).

   Catatan: sensors.cpp sudah melakukan kompensasi di domain tegangan
   sesuai referensi DFRobot, yang lebih akurat. Fungsi ini dipertahankan
   untuk kompatibilitas dan pemakaian di luar jalur sensor utama. */
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
