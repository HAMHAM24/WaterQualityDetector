/**
 * @file    buttons.cpp
 * @brief   Implementasi driver tombol. Setiap tombol memiliki FSM
 *          debounce independen. Event yang sudah stabil dikirim ke
 *          g_buttonEventQueue agar Task GUI dapat memprosesnya tanpa
 *          perlu mengetahui detail debounce sama sekali.
 */

#include "buttons.h"
#include "globals.h"
#include "config.h"

/**
 * @struct ButtonRuntime
 * @brief  State internal debounce per tombol. Sengaja dipisah dari
 *         ButtonState (globals.h) karena ini adalah detail implementasi
 *         driver, bukan data yang perlu dibaca modul lain.
 */
struct ButtonRuntime {
    uint8_t  pin;
    bool     stableLevel;      // true = sedang ditekan (setelah debounce)
    bool     lastRawLevel;     // pembacaan mentah terakhir
    uint32_t lastChangeTick;   // waktu (ms) terakhir raw level berubah
    uint32_t pressStartTick;   // waktu (ms) mulai ditekan (stabil)
    uint32_t lastRepeatTick;   // waktu (ms) repeat event terakhir
    bool     holdEmitted;      // sudah mengirim event HOLD untuk sesi tekan ini
};

static ButtonRuntime s_buttons[static_cast<uint8_t>(ButtonID::COUNT)];

static const char* buttonName(ButtonID id) {
    switch (id) {
        case ButtonID::UP:    return "UP";
        case ButtonID::DOWN:  return "DOWN";
        case ButtonID::LEFT:  return "LEFT";
        case ButtonID::RIGHT: return "RIGHT";
        case ButtonID::OK:    return "OK";
        case ButtonID::BACK:  return "BACK";
        default:              return "UNKNOWN";
    }
}

static const char* pinName(ButtonID id) {
    switch (id) {
        case ButtonID::UP:    return "PB14";
        case ButtonID::DOWN:  return "PA8";
        case ButtonID::LEFT:  return "PB15";
        case ButtonID::RIGHT: return "PB13";
        case ButtonID::OK:    return "PB12";
        case ButtonID::BACK:  return "PB11";
        default:              return "??";
    }
}

/**
 * @brief Mengirim satu event tombol ke queue dan memperbarui ButtonState
 *        ringkas untuk keperluan debug.
 */
static void emitEvent(ButtonID id, ButtonEvent event) {
    // --- REAL-TIME BUTTON SERIAL DEBUG LOGGER ---
    if (event == ButtonEvent::PRESSED) {
        Serial.print(F(">>> [BTN PRESS]   "));
        Serial.print(buttonName(id));
        Serial.print(F("\t(Pin "));
        Serial.print(pinName(id));
        Serial.println(F(")"));
    } else if (event == ButtonEvent::RELEASED) {
        Serial.print(F(">>> [BTN RELEASE] "));
        Serial.println(buttonName(id));
    }

    ButtonEventMsg msg{id, event};
    // Non-blocking: jika queue penuh, event terlama akan tetap diproses
    // lebih dulu oleh consumer; kita tidak menunggu (xTicksToWait = 0)
    // agar Task Button tidak pernah tertahan oleh Task GUI.
    xQueueSend(g_buttonEventQueue, &msg, 0);

    uint8_t idx = static_cast<uint8_t>(id);
    if (xSemaphoreTake(g_dataMutex, DATA_MUTEX_TIMEOUT) == pdTRUE) {
        g_buttonStates[idx].pressed  = (event == ButtonEvent::PRESSED);
        g_buttonStates[idx].released = (event == ButtonEvent::RELEASED);
        g_buttonStates[idx].hold     = (event == ButtonEvent::HOLD);
        g_buttonStates[idx].repeat   = (event == ButtonEvent::REPEAT);
        xSemaphoreGive(g_dataMutex);
    }
}

/**
 * @brief Inisialisasi GPIO seluruh tombol.
 */
void buttons_init() {
    const uint8_t pins[static_cast<uint8_t>(ButtonID::COUNT)] = {
        PIN_BTN_UP, PIN_BTN_DOWN, PIN_BTN_LEFT, PIN_BTN_RIGHT, PIN_BTN_OK, PIN_BTN_BACK
    };

    uint32_t now = millis();
    for (uint8_t i = 0; i < static_cast<uint8_t>(ButtonID::COUNT); i++) {
        pinMode(pins[i], INPUT_PULLUP);
        s_buttons[i].pin = pins[i];
        s_buttons[i].stableLevel = false;
        s_buttons[i].lastRawLevel = false;
        s_buttons[i].lastChangeTick = now;
        s_buttons[i].pressStartTick = 0;
        s_buttons[i].lastRepeatTick = 0;
        s_buttons[i].holdEmitted = false;
    }
}

/**
 * @brief Melakukan satu siklus scan debounce untuk seluruh tombol dan
 *        menghasilkan event PRESSED/RELEASED/HOLD/REPEAT bila relevan.
 */
void buttons_update() {
    const uint32_t now = millis();

    for (uint8_t i = 0; i < static_cast<uint8_t>(ButtonID::COUNT); i++) {
        ButtonRuntime& btn = s_buttons[i];
        const ButtonID id = static_cast<ButtonID>(i);

        // Active LOW: pin bernilai LOW berarti tombol ditekan.
        const bool rawPressed = (digitalRead(btn.pin) == LOW);

        if (rawPressed != btn.lastRawLevel) {
            btn.lastRawLevel = rawPressed;
            btn.lastChangeTick = now;
        }

        // Terima level baru hanya setelah stabil selama BUTTON_DEBOUNCE_MS.
        if ((now - btn.lastChangeTick) >= BUTTON_DEBOUNCE_MS &&
            rawPressed != btn.stableLevel) {
            btn.stableLevel = rawPressed;

            if (btn.stableLevel) {
                // Transisi ke ditekan
                btn.pressStartTick = now;
                btn.holdEmitted = false;
                emitEvent(id, ButtonEvent::PRESSED);
            } else {
                // Transisi ke dilepas
                emitEvent(id, ButtonEvent::RELEASED);
            }
        }

        // Selama tombol masih tertekan stabil, evaluasi hold & repeat.
        if (btn.stableLevel) {
            if (!btn.holdEmitted && (now - btn.pressStartTick) >= BUTTON_HOLD_MS) {
                btn.holdEmitted = true;
                btn.lastRepeatTick = now;
                emitEvent(id, ButtonEvent::HOLD);
            } else if (btn.holdEmitted && (now - btn.lastRepeatTick) >= BUTTON_REPEAT_MS) {
                btn.lastRepeatTick = now;
                emitEvent(id, ButtonEvent::REPEAT);
            }
        }
    }
}
