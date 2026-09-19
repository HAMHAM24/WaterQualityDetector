"""Hitung kalibrasi sensor turbidity dari ADC STM32 terhadap referensi Lab Bante.

Jalankan:
    python kalibrasi_turbidity.py

Grafik bersifat opsional dan memerlukan matplotlib:
    pip install matplotlib
"""

import argparse
from math import sqrt

# Data gabungan unik, diurutkan berdasarkan ADC. Full Aquades anomali
# (916 ADC, 0 NTU) serta pasangan duplikat antarsumber tidak digunakan.
# Nilai 795.5 adalah titik tengah rentang ADC 791-800 untuk Sampel 6.
ADC = [
    710, 718, 726, 732, 742, 748, 754, 779, 788, 790, 795.5,
    797, 821, 832, 852, 877, 1084,
]
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


def print_results(slope, intercept, r_squared, rmse, predictions, residuals):
    print("=== HASIL KALIBRASI TURBIDITY ===")
    print(f"NTU = ({slope:.6f} * ADC) + ({intercept:.6f})")
    print(f"R^2  = {r_squared:.6f}")
    print(f"RMSE = {rmse:.3f} NTU")
    print(f"Rentang data: ADC {min(ADC):.0f}-{max(ADC):.0f}, "
          f"NTU {min(NTU_LAB_BANTE):.2f}-{max(NTU_LAB_BANTE):.2f}")
    print()
    print("=== KOEFISIEN UNTUK STM32 ===")
    print(f"#define TURBIDITY_SLOPE      {slope:.6f}f")
    print(f"#define TURBIDITY_INTERCEPT  {intercept:.6f}f")
    print()
    print("=== DETAIL TITIK ===")
    print(" ADC | Lab Bante | Prediksi | Error (Lab - Prediksi)")
    print("-----+-----------+----------+-----------------------")
    for adc, actual, predicted, residual in zip(
        ADC, NTU_LAB_BANTE, predictions, residuals
    ):
        print(
            f"{adc:4.0f} | {actual:9.2f} | {predicted:8.2f} | {residual:+21.2f}"
        )


def show_plots(slope, intercept, r_squared, predictions, residuals):
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("\nGrafik dilewati karena matplotlib belum terpasang.")
        print("Pasang dengan: pip install matplotlib")
        return

    x_min, x_max = min(ADC), max(ADC)
    line_adc = [x_min + (x_max - x_min) * index / 200 for index in range(201)]
    line_ntu = [slope * value + intercept for value in line_adc]

    plt.figure(figsize=(9, 5))
    plt.scatter(ADC, NTU_LAB_BANTE, color="navy", label="Referensi Lab Bante")
    plt.plot(
        line_adc,
        line_ntu,
        color="crimson",
        label=f"NTU = {slope:.4f} ADC {intercept:+.4f}\nR^2 = {r_squared:.6f}",
    )
    plt.xlabel("ADC mentah STM32")
    plt.ylabel("Turbidity Lab Bante (NTU)")
    plt.title("Kalibrasi Sensor Turbidity")
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()

    plt.figure(figsize=(9, 4))
    plt.axhline(0, color="black", linewidth=1)
    plt.scatter(ADC, residuals, color="darkorange")
    plt.xlabel("ADC mentah STM32")
    plt.ylabel("Error: Lab Bante - Prediksi (NTU)")
    plt.title("Residual Kalibrasi")
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.show()


def main():
    parser = argparse.ArgumentParser(
        description="Hitung regresi kalibrasi turbidity ADC STM32 terhadap Lab Bante."
    )
    parser.add_argument(
        "--no-plot",
        action="store_true",
        help="Tampilkan hasil numerik tanpa membuka grafik.",
    )
    args = parser.parse_args()

    result = linear_regression(ADC, NTU_LAB_BANTE)
    print_results(*result)
    if not args.no_plot:
        slope, intercept, r_squared, _rmse, predictions, residuals = result
        show_plots(slope, intercept, r_squared, predictions, residuals)


if __name__ == "__main__":
    main()
