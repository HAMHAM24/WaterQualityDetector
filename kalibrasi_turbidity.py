"""Hitung regresi estimasi turbidity setelah koreksi proporsional ADC lama.

Jalankan:
    python kalibrasi_turbidity.py

Grafik bersifat opsional dan memerlukan matplotlib:
    pip install matplotlib
"""

import argparse
from math import sqrt

# Data gabungan unik lama, diurutkan berdasarkan ADC. Full Aquades anomali
# (916 ADC, 0 NTU) serta pasangan duplikat antarsumber tidak digunakan.
# Nilai 795.5 adalah titik tengah rentang ADC 791-800 untuk Sampel 6.
ADC_LAMA = [
    710, 718, 726, 732, 742, 748, 754, 779, 788, 790, 795.5,
    797, 821, 832, 852, 877, 1084,
]
ADC_AIR_JERNIH_LAMA = 710.0
ADC_AIR_KRAN_BARU = 1671.8
FAKTOR_KOREKSI_ADC = ADC_AIR_KRAN_BARU / ADC_AIR_JERNIH_LAMA
ADC_ESTIMASI = [value * FAKTOR_KOREKSI_ADC for value in ADC_LAMA]
TURBIDITY_ZERO_ADC = 1692.0
TURBIDITY_TAP_ADC = 1709.0
TURBIDITY_TAP_NTU = 1.30
TURBIDITY_SLOPE_FIRMWARE = 0.514082
TURBIDITY_INTERCEPT_FIRMWARE = (
    TURBIDITY_TAP_NTU - TURBIDITY_SLOPE_FIRMWARE * TURBIDITY_TAP_ADC
)
NTU_LAB_BANTE = [
    0.00, 9.44, 19.47, 28.97, 38.72, 47.45, 16.26, 34.43, 32.28,
    24.00, 40.44, 18.22, 59.98, 87.22, 177.30, 150.90, 468.00,
]


def linear_regression(x, y):
    """Return slope, intercept, R-squared, RMSE, predictions, and residuals."""
    if len(x) != len(y):
        raise ValueError("Jumlah data ADC dan NTU Lab Bante harus sama.")
    if len(x) < 2:
        raise ValueError("Minimal diperlukan dua titik kalibrasi.")

    count = len(x)
    mean_x = sum(x) / count
    mean_y = sum(y) / count
    sum_xx = sum((value - mean_x) ** 2 for value in x)

    if sum_xx == 0:
        raise ValueError("Nilai ADC tidak boleh semuanya sama.")

    slope = sum(
        (x_value - mean_x) * (y_value - mean_y)
        for x_value, y_value in zip(x, y)
    ) / sum_xx
    intercept = mean_y - slope * mean_x
    predictions = [slope * value + intercept for value in x]
    residuals = [actual - predicted for actual, predicted in zip(y, predictions)]

    residual_sum_squares = sum(error**2 for error in residuals)
    total_sum_squares = sum((value - mean_y) ** 2 for value in y)
    r_squared = 1.0 if total_sum_squares == 0 else 1 - (
        residual_sum_squares / total_sum_squares
    )
    rmse = sqrt(residual_sum_squares / count)

    return slope, intercept, r_squared, rmse, predictions, residuals


def firmware_model(adc):
    """Model sementara firmware dengan asumsi air kran, bukan regresi baru."""
    if adc <= TURBIDITY_ZERO_ADC:
        return 0.0
    if adc < TURBIDITY_TAP_ADC:
        return TURBIDITY_TAP_NTU * (adc - TURBIDITY_ZERO_ADC) / (
            TURBIDITY_TAP_ADC - TURBIDITY_ZERO_ADC
        )
    return TURBIDITY_TAP_NTU + TURBIDITY_SLOPE_FIRMWARE * (
        adc - TURBIDITY_TAP_ADC
    )


def print_results(slope, intercept, r_squared, rmse, predictions, residuals):
    print("=== HASIL REGRESI TURBIDITY KOREKSI ESTIMASI ===")
    print(f"Faktor koreksi ADC = {ADC_AIR_KRAN_BARU} / {ADC_AIR_JERNIH_LAMA} "
          f"= {FAKTOR_KOREKSI_ADC:.9f}")
    print("PERINGATAN: ADC hasil koreksi bukan data pengukuran ulang.")
    print(f"NTU = ({slope:.6f} * ADC) + ({intercept:.6f})")
    print(f"R^2  = {r_squared:.6f}")
    print(f"RMSE = {rmse:.3f} NTU")
    print(f"Rentang estimasi: ADC {min(ADC_ESTIMASI):.2f}-{max(ADC_ESTIMASI):.2f}, "
          f"NTU {min(NTU_LAB_BANTE):.2f}-{max(NTU_LAB_BANTE):.2f}")
    print()
    print("=== REGRESI 17 TITIK (RIWAYAT ESTIMASI) ===")
    print(f"NTU = ({slope:.6f} * ADC) + ({intercept:.6f})")
    print()
    print("=== MODEL SEMENTARA FIRMWARE ===")
    print(f"ADC <= {TURBIDITY_ZERO_ADC:.0f}: 0.00 NTU")
    print(f"ADC {TURBIDITY_ZERO_ADC:.0f}-{TURBIDITY_TAP_ADC:.0f}: "
          f"interpolasi 0.00-{TURBIDITY_TAP_NTU:.2f} NTU")
    print(f"ADC >= {TURBIDITY_TAP_ADC:.0f}: "
          f"({TURBIDITY_SLOPE_FIRMWARE:.6f} * ADC) + "
          f"({TURBIDITY_INTERCEPT_FIRMWARE:.6f})")
    print("Konstanta firmware:")
    print(f"constexpr float TURBIDITY_SLOPE = {TURBIDITY_SLOPE_FIRMWARE:.6f}f;")
    print(f"constexpr float TURBIDITY_INTERCEPT = {TURBIDITY_INTERCEPT_FIRMWARE:.6f}f;")
    print("PERINGATAN: titik 1692 dan 1709 adalah asumsi air kran.")
    print()
    print("=== DETAIL TITIK ===")
    print("ADC lama | ADC estimasi | Lab Bante | Model FW | Error (Lab - Model)")
    print("---------+--------------+-----------+----------+--------------------")
    for adc_lama, adc_estimasi, actual in zip(
        ADC_LAMA, ADC_ESTIMASI, NTU_LAB_BANTE
    ):
        firmware_prediction = firmware_model(adc_estimasi)
        firmware_residual = actual - firmware_prediction
        print(
            f"{adc_lama:8.1f} | {adc_estimasi:12.2f} | {actual:9.2f} | "
            f"{firmware_prediction:8.2f} | {firmware_residual:+19.2f}"
        )


def show_plots(slope, intercept, r_squared, predictions, residuals):
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("\nGrafik dilewati karena matplotlib belum terpasang.")
        print("Pasang dengan: pip install matplotlib")
        return

    x_min, x_max = min(ADC_ESTIMASI), max(ADC_ESTIMASI)
    line_adc = [x_min + (x_max - x_min) * index / 200 for index in range(201)]
    line_ntu = [firmware_model(value) for value in line_adc]

    plt.figure(figsize=(9, 5))
    plt.scatter(ADC_ESTIMASI, NTU_LAB_BANTE, color="navy", label="ADC koreksi estimasi")
    plt.plot(
        line_adc,
        line_ntu,
        color="crimson",
        label="Model sementara firmware",
    )
    plt.xlabel("ADC STM32 hasil koreksi estimasi")
    plt.ylabel("Turbidity Lab Bante (NTU)")
    plt.title("Model Sementara Sensor Turbidity")
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()

    plt.figure(figsize=(9, 4))
    plt.axhline(0, color="black", linewidth=1)
    firmware_residuals = [
        actual - firmware_model(adc)
        for adc, actual in zip(ADC_ESTIMASI, NTU_LAB_BANTE)
    ]
    plt.scatter(ADC_ESTIMASI, firmware_residuals, color="darkorange")
    plt.xlabel("ADC STM32 hasil koreksi estimasi")
    plt.ylabel("Error: Lab Bante - Prediksi (NTU)")
    plt.title("Residual Model Sementara")
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.show()


def main():
    parser = argparse.ArgumentParser(
        description="Hitung regresi estimasi turbidity dari koreksi proporsional ADC lama."
    )
    parser.add_argument(
        "--no-plot",
        action="store_true",
        help="Tampilkan hasil numerik tanpa membuka grafik.",
    )
    args = parser.parse_args()

    result = linear_regression(ADC_ESTIMASI, NTU_LAB_BANTE)
    print_results(*result)
    if not args.no_plot:
        slope, intercept, r_squared, _rmse, predictions, residuals = result
        show_plots(slope, intercept, r_squared, predictions, residuals)


if __name__ == "__main__":
    main()
