/**
 * @file    tasks.h
 * @brief   Task Manager — mendeklarasikan seluruh task FreeRTOS firmware
 *          beserta fungsi untuk membuat semuanya sekaligus.
 */

#ifndef TASKS_H
#define TASKS_H

#include <Arduino.h>
#include <STM32FreeRTOS.h>

/**
 * @brief Membuat seluruh task FreeRTOS firmware (Button, Temperature,
 *        Water Sensor, GUI, OLED, Serial Debug) dengan prioritas dan
 *        ukuran stack sesuai config.h. Dipanggil satu kali dari setup()
 *        sebelum vTaskStartScheduler().
 * @return true jika seluruh task berhasil dibuat.
 */
bool tasks_createAll();

#endif // TASKS_H
