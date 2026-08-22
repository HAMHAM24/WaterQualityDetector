/* ==========================================================================
 * fuzzy_kualitas_air.h
 * Header Fuzzy Sugeno - Klasifikasi Kualitas Air
 * Input : TDS (ppm) + Turbidity (NTU) + Suhu (Celsius)  -> 3 input
 * Rule  : 3 x 3 x 3 = 27 aturan (identik kualitas_air.fis)
 * Output: Sugeno Order-0, skala 0.0 - 1.0 (5 level kualitas)
 * Target: STM32 Blackpill (STM32F4xx) - murni float, tanpa dynamic memory
 *
 * ==========================================================================
 * PERINGATAN PENTING TENTANG RULE BASE (BELUM DIVALIDASI)
 * --------------------------------------------------------------------------
 * Tabel 27 aturan pada fuzzy_kualitas_air.c adalah DRAF ASUMSI DESAIN,
 * BUKAN turunan dari dokumen baku mutu resmi (Permenkes/SNI) maupun dari
 * catatan client. Note/Membership function.txt hanya mendefinisikan bentuk
 * membership function dan 5 label output; dokumen tersebut TIDAK memuat
 * tabel aturan sama sekali.
 *
 * Skema asumsi yang dipakai saat ini:
 *   - Suhu Normal  -> netral (skor mengikuti matriks TDS x Turbidity)
 *   - Suhu Dingin  -> turunkan 1 tingkat kualitas
 *   - Suhu Panas   -> turunkan 1 tingkat kualitas
 *
 * Tabel ini WAJIB divalidasi oleh client / dosen pembimbing sebelum
 * dianggap final dan sebelum alat dipakai untuk pengambilan keputusan.
 * ========================================================================== */

#ifndef FUZZY_KUALITAS_AIR_H
#define FUZZY_KUALITAS_AIR_H

#include <math.h>

/* ---------- ENUM STATUS OUTPUT KUALITAS AIR (5 LEVEL) ------------------- */
typedef enum {
    STATUS_EXCELLENT = 0,   /* z1 = 1.00 - Sangat Baik / Sangat Layak       */
    STATUS_GOOD,            /* z2 = 0.75 - Baik / Layak                     */
    STATUS_POOR,            /* z3 = 0.50 - Perlu Filtrasi Ringan            */
    STATUS_VERY_POOR,       /* z4 = 0.25 - Sangat Buruk / Filtrasi Intensif */
    STATUS_NOT_SUITABLE     /* z5 = 0.00 - Tidak Lolos / Dilarang           */
} KualitasAir_t;

/* ---------- ENUM STATUS SUHU AIR (3 LEVEL) ----------------------------- */
typedef enum {
    SUHU_DINGIN = 0,        /* <= 24 C                                      */
    SUHU_NORMAL,            /* 24 - 32 C                                    */
    SUHU_PANAS              /* >= 32 C                                      */
} StatusSuhu_t;

/* --------------------------------------------------------------------------
 * PROFIL BAKU MUTU FUZZY
 *
 * Mendefinisikan parameter fungsi keanggotaan (MF) trapesium (trapmf) dan
 * segitiga (trimf) serta ambang klasifikasi status kualitas air (0.0 - 1.0).
 *
 * Pola kurva (sama untuk ketiga variabel input):
 *   TDS Rendah  : trapmf [0, 0, tdsRendah_b, tdsRendah_c]
 *   TDS Sedang  : trimf  [tdsSedang_a, tdsSedang_b, tdsSedang_c]
 *   TDS Tinggi  : trapmf [tdsTinggi_a, tdsTinggi_b, tdsTinggi_c, tdsTinggi_c]
 *
 *   Turb Jernih : trapmf [0, 0, turbJernih_b, turbJernih_c]
 *   Turb Sedang : trimf  [turbSedang_a, turbSedang_b, turbSedang_c]
 *   Turb Keruh  : trapmf [turbKeruh_a, turbKeruh_b, turbKeruh_c, turbKeruh_c]
 *
 *   Suhu Dingin : trapmf [0, 0, tempDingin_b, tempDingin_c]
 *   Suhu Normal : trimf  [tempNormal_a, tempNormal_b, tempNormal_c]
 *   Suhu Panas  : trapmf [tempPanas_a, tempPanas_b, tempPanas_c, tempPanas_c]
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

    /* Parameter Suhu (Celsius) */
    float tempDingin_b;      /* batas atas keanggotaan penuh Dingin (1.0)    */
    float tempDingin_c;      /* batas akhir Dingin turun ke 0                */
    float tempNormal_a;      /* mulai naik Normal dari 0                     */
    float tempNormal_b;      /* puncak Normal (1.0) - dipakai sbg suhu netral */
    float tempNormal_c;      /* akhir Normal turun ke 0                      */
    float tempPanas_a;       /* mulai naik Panas dari 0                      */
    float tempPanas_b;       /* mulai keanggotaan penuh Panas (1.0)          */
    float tempPanas_c;       /* batas atas semesta Suhu                      */

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
 *        tertentu memakai 3 input fuzzy (TDS, Turbidity, Suhu).
 * @param profil     Profil baku mutu aktif (NULL -> memakai profil default).
 * @param tds        Nilai TDS terkompensasi suhu, satuan ppm (mg/L).
 * @param turbidity  Nilai kekeruhan, satuan NTU.
 * @param suhu       Nilai suhu air, satuan Celsius.
 * @return Skor kualitas air pada skala 0.0 - 1.0.
 */
float FuzzyKualitasAir_HitungSkorProfil(const FuzzyProfil_t* profil,
                                         float tds, float turbidity, float suhu);

/**
 * @brief Versi ringkas memakai profil default (Air Minum & Higiene Sanitasi).
 */
float FuzzyKualitasAir_HitungSkor(float tds, float turbidity, float suhu);

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
 * @brief Suhu netral (puncak MF Normal) yang dipakai sebagai nilai pengganti
 *        bila sensor suhu error. Memakai 0.0 C akan memicu penalti "Dingin"
 *        palsu dan menurunkan skor secara tidak sah, sehingga nilai netral
 *        ini yang dipakai agar suhu tidak memengaruhi hasil.
 */
float FuzzyKualitasAir_SuhuNetral(void);

/**
 * @brief Profil bawaan default (sesuai Note/Membership function.txt).
 */
const FuzzyProfil_t* FuzzyKualitasAir_ProfilDefault(void);

#ifdef __cplusplus
}
#endif

#endif /* FUZZY_KUALITAS_AIR_H */
