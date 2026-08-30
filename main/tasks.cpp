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
                if (g_systemState.currentMenu == MenuState::CALIBRATION_TURBIDITY_WIZARD &&
                    g_systemState.turbidityCalibFeedback == TurbidityCalibrationFeedback::SAVING) {
                    g_systemState.turbidityCalibFeedback = TurbidityCalibrationFeedback::SUCCESS;
                    g_systemState.turbidityCalibSuccessTick = millis();
                }
                g_systemState.displayDirty = true;
                xSemaphoreGive(g_dataMutex);
            }
        } else {
            // Permintaan dengan parameter yang sudah sama di EEPROM tidak menulis flash,
            // tetapi tetap merupakan penyimpanan yang berhasil dari sisi pengguna.
            if (xSemaphoreTake(g_dataMutex, DATA_MUTEX_TIMEOUT) == pdTRUE) {
                if (g_systemState.calibSaving) {
                    g_systemState.calibSaving = false;
                    if (g_systemState.currentMenu == MenuState::CALIBRATION_TURBIDITY_WIZARD &&
                        g_systemState.turbidityCalibFeedback == TurbidityCalibrationFeedback::SAVING) {
                        g_systemState.turbidityCalibFeedback = TurbidityCalibrationFeedback::SUCCESS;
                        g_systemState.turbidityCalibSuccessTick = millis();
                    }
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
        "SPLASH", "HOME", "INPUT_AMBIENT_TEMPERATURE", "WAITING_SAMPLING", "MEASUREMENT",
        "CALIBRATION", "CALIBRATION_TDS_MENU", "CALIBRATION_TDS_WIZARD", "TDS_MONITOR",
        "CALIBRATION_TURBIDITY_MENU", "CALIBRATION_TURBIDITY_WIZARD", "TURBIDITY_MONITOR",
        "CALIBRATION_TEMPERATURE_MENU", "CALIBRATION_TEMPERATURE_WIZARD", "TEMPERATURE_MONITOR",
        "SETTINGS", "ABOUT", "FACTORY_RESET_CONFIRM"
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

        Serial.println(F("---- Water Quality Analyzer Debug ----"));

        Serial.print(F("Temperature   : "));
        Serial.print(sensorSnapshot.temperature);
        Serial.println(F(" C"));

        Serial.print(F("TDS Raw/Flt   : "));
        Serial.print(sensorSnapshot.tdsRaw);
        Serial.print(F(" / "));
        Serial.println(sensorSnapshot.tdsFiltered);

        Serial.print(F("Turb Raw/Flt  : "));
        Serial.print(sensorSnapshot.turbidityRaw);
        Serial.print(F(" / "));
        Serial.println(sensorSnapshot.turbidityFiltered);

        Serial.print(F("TDS Comp      : "));
        Serial.println(sensorSnapshot.tdsCompensated);

        Serial.print(F("Fuzzy Skor    : "));
        Serial.print(sensorSnapshot.fuzzyScore, 2);
        Serial.print(F(" ["));
        Serial.print(FuzzyKualitasAir_GetStatusBadge(sensorSnapshot.qualityStatus));
        Serial.print(F(" - "));
        Serial.print(FuzzyKualitasAir_GetPesan(sensorSnapshot.qualityStatus));
        Serial.println(F("]"));

        Serial.print(F("Buttons P/R/H : "));
        for (uint8_t i = 0; i < static_cast<uint8_t>(ButtonID::COUNT); i++) {
            Serial.print(buttonSnapshot[i].pressed ? '1' : '0');
        }
        Serial.print(' ');
        for (uint8_t i = 0; i < static_cast<uint8_t>(ButtonID::COUNT); i++) {
            Serial.print(buttonSnapshot[i].hold ? '1' : '0');
        }
        Serial.println();

        Serial.print(F("Menu Aktif    : "));
        Serial.println(menuNames[static_cast<uint8_t>(menuSnapshot)]);

        // Stack high water mark: byte tersisa terendah (paling kecil)
        Serial.print(F("Stack Min (B) : "));
        for (uint8_t i = 0; i < 6; i++) {
            if (s_handles[i] != nullptr) {
                Serial.print(s_taskNames[i]);
                Serial.print('=');
                Serial.print(uxTaskGetStackHighWaterMark(s_handles[i]));
                Serial.print(' ');
            }
        }
        Serial.println();

        Serial.print(F("Free Heap     : "));
        Serial.print(xPortGetFreeHeapSize());
        Serial.println(F(" bytes"));

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
