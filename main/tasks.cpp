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
#include "storage.h"

// Simpan handle task agar TaskDebug dapat melaporkan high water mark.
static TaskHandle_t s_handles[6] = { nullptr };
static const char* const s_taskNames[6] = {
    "Btn", "Temp", "Water", "Gui", "Oled", "Debug"
};

// =============================================================================
// TASK BUTTON — scan + debounce, periode 15 ms, prioritas tinggi
// =============================================================================
static void taskButton(void* /* pvParameters */) {
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
        vTaskDelay(pdMS_TO_TICKS(DS18B20_CONVERSION_MS));
        sensors_readTemperature();
        vTaskDelayUntil(&lastWakeTime, period);
    }
}

// =============================================================================
// TASK WATER SENSOR — baca TDS & Turbidity, proses fuzzy, simpan kalibrasi.
// Periode 200 ms.
// =============================================================================
static void taskWaterSensor(void* /* pvParameters */) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(TASK_PERIOD_WATER_SENSOR_MS);

    for (;;) {
        // Tangani permintaan penyimpanan kalibrasi yang tertunda. Ini adalah
        // titik aman karena: (a) TaskTemp sedang idle (menunggu 750+ms),
        // (b) TaskOled belum menggambar, dan (c) I2C/OneWire tidak aktif.
        if (storage_processPendingSave()) {
            if (xSemaphoreTake(g_dataMutex, DATA_MUTEX_TIMEOUT) == pdTRUE) {
                g_systemState.calibSaving = false;
                g_systemState.displayDirty = true;
                xSemaphoreGive(g_dataMutex);
            }
        } else {
            // Reset flag kalau tidak ada penulisan — pastikan tidak macet.
            if (xSemaphoreTake(g_dataMutex, DATA_MUTEX_TIMEOUT) == pdTRUE) {
                if (g_systemState.calibSaving) {
                    g_systemState.calibSaving = false;
                    g_systemState.displayDirty = true;
                }
                xSemaphoreGive(g_dataMutex);
            }
        }

        sensors_updateTDS();
        sensors_updateTurbidity();
        sensors_processFuzzy();

        vTaskDelayUntil(&lastWakeTime, period);
    }
}

// =============================================================================
// TASK GUI — hanya mengubah state (FSM), tidak menggambar apapun
// =============================================================================
static void taskGui(void* /* pvParameters */) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(TASK_PERIOD_GUI_MS);

    ButtonEventMsg msg;

    for (;;) {
        gui_tick();

        while (xQueueReceive(g_buttonEventQueue, &msg, 0) == pdTRUE) {
            gui_update(msg);
        }

        vTaskDelayUntil(&lastWakeTime, period);
    }
}

// =============================================================================
// TASK OLED — hanya menggambar, periode 100 ms, refresh hanya saat dirty
// =============================================================================
static void taskOled(void* /* pvParameters */) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(TASK_PERIOD_OLED_MS);

    for (;;) {
        bool needRedraw = false;

        if (xSemaphoreTake(g_dataMutex, DATA_MUTEX_TIMEOUT) == pdTRUE) {
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
// TASK SERIAL DEBUG — laporan status sistem + stack high water mark
// =============================================================================
static void taskSerialDebug(void* /* pvParameters */) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(TASK_PERIOD_SERIAL_DEBUG_MS);

    static const char* const menuNames[] = {
        "SPLASH", "HOME", "PARAMETER", "MEASUREMENT", "CALIBRATION",
        "CALIBRATION_TDS", "CALIBRATION_TURBIDITY", "CALIBRATION_TEMPERATURE",
        "SETTINGS", "ABOUT"
    };

    for (;;) {
        SensorData sensorSnapshot;
        memset(&sensorSnapshot, 0, sizeof(sensorSnapshot));

        ButtonState buttonSnapshot[static_cast<uint8_t>(ButtonID::COUNT)];
        memset(buttonSnapshot, 0, sizeof(buttonSnapshot));

        MenuState menuSnapshot = MenuState::SPLASH;

        if (xSemaphoreTake(g_dataMutex, DATA_MUTEX_TIMEOUT) == pdTRUE) {
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

        Serial1.print(F("TDS Comp      : "));
        Serial1.println(sensorSnapshot.tdsCompensated);

        Serial1.print(F("Fuzzy Skor    : "));
        Serial1.print(sensorSnapshot.fuzzyScore);
        Serial1.print(F(" ["));
        Serial1.print(FuzzyKualitasAir_GetPesan(sensorSnapshot.qualityStatus));
        Serial1.println(F("]"));

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

        // Stack high water mark: byte tersisa terendah (paling kecil)
        Serial1.print(F("Stack Min (B) : "));
        for (uint8_t i = 0; i < 6; i++) {
            if (s_handles[i] != nullptr) {
                Serial1.print(s_taskNames[i]);
                Serial1.print('=');
                Serial1.print(uxTaskGetStackHighWaterMark(s_handles[i]));
                Serial1.print(' ');
            }
        }
        Serial1.println();

        Serial1.print(F("Free Heap     : "));
        Serial1.print(xPortGetFreeHeapSize());
        Serial1.println(F(" bytes"));

        vTaskDelayUntil(&lastWakeTime, period);
    }
}

// =============================================================================
// PEMBUATAN SELURUH TASK
// =============================================================================
bool tasks_createAll() {
    BaseType_t rc;

    rc = xTaskCreate(taskButton, "TaskButton", STACK_SIZE_BUTTON, nullptr,
                     TASK_PRIORITY_BUTTON, &s_handles[0]);
    if (rc != pdPASS) return false;

    rc = xTaskCreate(taskTemperature, "TaskTemp", STACK_SIZE_TEMPERATURE, nullptr,
                     TASK_PRIORITY_TEMPERATURE, &s_handles[1]);
    if (rc != pdPASS) return false;

    rc = xTaskCreate(taskWaterSensor, "TaskWater", STACK_SIZE_WATER_SENSOR, nullptr,
                     TASK_PRIORITY_WATER_SENSOR, &s_handles[2]);
    if (rc != pdPASS) return false;

    rc = xTaskCreate(taskGui, "TaskGui", STACK_SIZE_GUI, nullptr,
                     TASK_PRIORITY_GUI, &s_handles[3]);
    if (rc != pdPASS) return false;

    rc = xTaskCreate(taskOled, "TaskOled", STACK_SIZE_OLED, nullptr,
                     TASK_PRIORITY_OLED, &s_handles[4]);
    if (rc != pdPASS) return false;

    rc = xTaskCreate(taskSerialDebug, "TaskDebug", STACK_SIZE_SERIAL_DEBUG, nullptr,
                     TASK_PRIORITY_SERIAL_DEBUG, &s_handles[5]);
    if (rc != pdPASS) return false;

    return true;
}
