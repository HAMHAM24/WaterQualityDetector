/**
 * @file    tasks.cpp
 * @brief   Implementasi seluruh task FreeRTOS firmware. Setiap task
 *          murni mengorkestrasi pemanggilan modul driver/GUI sesuai
 *          periode masing-masing; tidak ada logika bisnis di sini.
 */

#include "tasks.h"
#include "globals.h"
#include "config.h"
#include "sensors.h"
#include "buttons.h"
#include "display.h"
#include "gui.h"

// =============================================================================
// TASK BUTTON — scan + debounce, periode 10-20 ms, prioritas tinggi
// =============================================================================
static void taskButton(void* pvParameters) {
    (void)pvParameters;
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(TASK_PERIOD_BUTTON_MS);

    for (;;) {
        buttons_update();
        vTaskDelayUntil(&lastWakeTime, period);
    }
}

// =============================================================================
// TASK TEMPERATURE — baca DS18B20, periode 1000 ms
// =============================================================================
static void taskTemperature(void* pvParameters) {
    (void)pvParameters;
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(TASK_PERIOD_TEMPERATURE_MS);

    for (;;) {
        sensors_requestTemperature();
        // Menunggu waktu konversi sensor secara kooperatif (bukan delay()
        // global); task lain tetap berjalan normal selama task ini idle.
        vTaskDelay(pdMS_TO_TICKS(DS18B20_CONVERSION_MS));
        sensors_readTemperature();

        vTaskDelayUntil(&lastWakeTime, period);
    }
}

// =============================================================================
// TASK WATER SENSOR — baca TDS & Turbidity dengan moving average, periode 200 ms
// =============================================================================
static void taskWaterSensor(void* pvParameters) {
    (void)pvParameters;
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(TASK_PERIOD_WATER_SENSOR_MS);

    for (;;) {
        sensors_updateTDS();
        sensors_updateTurbidity();
        vTaskDelayUntil(&lastWakeTime, period);
    }
}

// =============================================================================
// TASK GUI — hanya mengubah state (FSM), tidak menggambar apapun
// =============================================================================
static void taskGui(void* pvParameters) {
    (void)pvParameters;
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(TASK_PERIOD_GUI_MS);

    ButtonEventMsg msg;

    for (;;) {
        gui_tick(); // transisi otomatis berbasis waktu (mis. splash screen)

        // Kuras seluruh event tombol yang tertunda di queue tanpa memblokir.
        while (xQueueReceive(g_buttonEventQueue, &msg, 0) == pdTRUE) {
            gui_update(msg);
        }

        vTaskDelayUntil(&lastWakeTime, period);
    }
}

// =============================================================================
// TASK OLED — hanya menggambar, periode 100 ms, refresh hanya saat dirty
// =============================================================================
static void taskOled(void* pvParameters) {
    (void)pvParameters;
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(TASK_PERIOD_OLED_MS);

    for (;;) {
        bool needRedraw = false;

        if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            needRedraw = g_systemState.displayDirty;
            g_systemState.displayDirty = false;
            xSemaphoreGive(g_dataMutex);
        }

        if (needRedraw) {
            gui_draw();
        }

        vTaskDelayUntil(&lastWakeTime, period);
    }
}

// =============================================================================
// TASK SERIAL DEBUG — laporan status sistem, periode 1000 ms
// =============================================================================
static void taskSerialDebug(void* pvParameters) {
    (void)pvParameters;
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(TASK_PERIOD_SERIAL_DEBUG_MS);

    static const char* const menuNames[] = {
        "SPLASH", "HOME", "PARAMETER", "MEASUREMENT", "CALIBRATION",
        "CALIBRATION_TDS", "CALIBRATION_TURBIDITY", "CALIBRATION_TEMPERATURE",
        "SETTINGS", "ABOUT"
    };

    for (;;) {
        SensorData sensorSnapshot;
        ButtonState buttonSnapshot[static_cast<uint8_t>(ButtonID::COUNT)];
        MenuState menuSnapshot;

        if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            sensorSnapshot = g_sensorData;
            for (uint8_t i = 0; i < static_cast<uint8_t>(ButtonID::COUNT); i++) {
                buttonSnapshot[i] = g_buttonStates[i];
            }
            menuSnapshot = g_systemState.currentMenu;
            xSemaphoreGive(g_dataMutex);
        }

        Serial1.println(F("---- Water Quality Analyzer Debug ----"));

        Serial1.print(F("Temperature   : "));
        Serial1.print(sensorSnapshot.temperature);
        Serial1.println(F(" C"));

        Serial1.print(F("TDS Raw/Flt   : "));
        Serial1.print(sensorSnapshot.tdsRaw);
        Serial1.print(F(" / "));
        Serial1.println(sensorSnapshot.tdsFiltered);

        Serial1.print(F("Turb Raw/Flt  : "));
        Serial1.print(sensorSnapshot.turbidityRaw);
        Serial1.print(F(" / "));
        Serial1.println(sensorSnapshot.turbidityFiltered);

        Serial1.print(F("Buttons P/R/H : "));
        for (uint8_t i = 0; i < static_cast<uint8_t>(ButtonID::COUNT); i++) {
            Serial1.print(buttonSnapshot[i].pressed ? '1' : '0');
        }
        Serial1.print(' ');
        for (uint8_t i = 0; i < static_cast<uint8_t>(ButtonID::COUNT); i++) {
            Serial1.print(buttonSnapshot[i].hold ? '1' : '0');
        }
        Serial1.println();

        Serial1.print(F("Menu Aktif    : "));
        Serial1.println(menuNames[static_cast<uint8_t>(menuSnapshot)]);

        Serial1.print(F("Free Heap     : "));
        Serial1.print(xPortGetFreeHeapSize());
        Serial1.println(F(" bytes"));

        vTaskDelayUntil(&lastWakeTime, period);
    }
}

// =============================================================================
// PEMBUATAN SELURUH TASK
// =============================================================================
void tasks_createAll() {
    xTaskCreate(taskButton, "TaskButton", STACK_SIZE_BUTTON, nullptr,
                TASK_PRIORITY_BUTTON, nullptr);

    xTaskCreate(taskTemperature, "TaskTemp", STACK_SIZE_TEMPERATURE, nullptr,
                TASK_PRIORITY_TEMPERATURE, nullptr);

    xTaskCreate(taskWaterSensor, "TaskWater", STACK_SIZE_WATER_SENSOR, nullptr,
                TASK_PRIORITY_WATER_SENSOR, nullptr);

    xTaskCreate(taskGui, "TaskGui", STACK_SIZE_GUI, nullptr,
                TASK_PRIORITY_GUI, nullptr);

    xTaskCreate(taskOled, "TaskOled", STACK_SIZE_OLED, nullptr,
                TASK_PRIORITY_OLED, nullptr);

    xTaskCreate(taskSerialDebug, "TaskDebug", STACK_SIZE_SERIAL_DEBUG, nullptr,
                TASK_PRIORITY_SERIAL_DEBUG, nullptr);
}
