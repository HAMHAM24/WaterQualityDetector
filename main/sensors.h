/**
 * @file    sensors.h
 * @brief   Driver untuk sensor suhu DS18B20, TDS DFRobot, dan turbidity
 *          SEN0189. Modul ini murni "hardware access layer" — tidak
 *          mengetahui apapun tentang GUI atau menu.
 */

#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>

/**
 * @brief Inisialisasi seluruh perangkat keras sensor: resolusi ADC,
 *        bus OneWire, dan sensor DS18B20.
 */
void sensors_init();

/**
 * @brief Memulai konversi suhu DS18B20 (non-blocking terhadap task lain).
 *        Dipanggil oleh Task Temperature sebelum menunggu waktu konversi.
 */
void sensors_requestTemperature();

/**
 * @brief Membaca hasil konversi suhu DS18B20 setelah waktu konversi
 *        terpenuhi (DS18B20_CONVERSION_MS). Memperbarui g_sensorData
 *        (temperature & temperatureStatus) secara thread-safe.
 */
void sensors_readTemperature();

/**
 * @brief Membaca nilai ADC mentah sensor TDS pada PA0.
 * @return Nilai ADC 0-4095.
 */
uint16_t sensors_readTDSRaw();

/**
 * @brief Mengambil satu sampel TDS, memasukkannya ke buffer moving
 *        average, mengubahnya menjadi ppm, lalu memperbarui g_sensorData
 *        secara thread-safe. Dipanggil periodik oleh Task Water Sensor.
 */
void sensors_updateTDS();

/**
 * @brief Membaca nilai ADC mentah sensor turbidity pada PA1.
 * @return Nilai ADC 0-4095.
 */
uint16_t sensors_readTurbidityRaw();

/**
 * @brief Mengambil satu sampel turbidity, memasukkannya ke buffer moving
 *        average, mengubahnya menjadi NTU, lalu memperbarui g_sensorData
 *        secara thread-safe.
 */
void sensors_updateTurbidity();

/**
 * @brief Mengeksekusi kompensasi suhu TDS dan perhitungan skor Fuzzy Sugeno
 *        memakai profil baku mutu yang sedang aktif, lalu menyimpan hasil
 *        ke g_sensorData secara thread-safe.
 */
void sensors_processFuzzy();

/**
 * @brief Mengubah nilai ADC mentah menjadi tegangan di sisi sensor,
 *        termasuk koreksi pembagi tegangan.
 * @param raw     Nilai ADC 0-4095 (boleh berupa rata-rata, karena float).
 * @param divider Faktor pembagi tegangan rangkaian (lihat config.h).
 */
float sensors_adcToVoltage(float raw, float divider);

/**
 * @brief Mengubah tegangan sensor TDS menjadi nilai TDS (ppm) memakai
 *        polinomial resmi DFRobot, dengan kompensasi suhu di domain
 *        tegangan dan penerapan K-factor hasil kalibrasi.
 * @param voltage     Tegangan sisi sensor (volt).
 * @param temperature Suhu air (Celsius) untuk kompensasi.
 * @return Nilai TDS dalam ppm.
 */
float sensors_voltageToTds(float voltage, float temperature);

/**
 * @brief Mengubah tegangan sensor turbidity menjadi nilai kekeruhan (NTU)
 *        relatif terhadap tegangan air jernih hasil kalibrasi.
 * @param voltage Tegangan sisi sensor (volt).
 * @return Nilai kekeruhan dalam NTU.
 */
float sensors_voltageToNtu(float voltage);

#endif // SENSORS_H
