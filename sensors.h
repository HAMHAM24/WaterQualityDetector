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
 *        average, lalu memperbarui g_sensorData (tdsRaw & tdsFiltered)
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
 *        average, lalu memperbarui g_sensorData (turbidityRaw &
 *        turbidityFiltered) secara thread-safe.
 */
void sensors_updateTurbidity();

#endif // SENSORS_H
