#include "Buttons.h"
#include "config.h"

void Buttons::begin() {
  const uint8_t pins[BTN_COUNT] = {PIN_BTN_UP, PIN_BTN_DOWN, PIN_BTN_SELECT, PIN_BTN_HOME};
  for (uint8_t i = 0; i < BTN_COUNT; i++) {
    State &s = btn_[i];
    s.pin = pins[i];
    s.canRepeat = (i == BTN_UP || i == BTN_DOWN);
    pinMode(s.pin, INPUT_PULLUP);
    // Treat a button held during boot as already pressed, so it doesn't fire
    // the moment we start.
    s.rawPressed = s.stablePressed = (digitalRead(s.pin) == LOW);
    s.rawChangedAt = 0;
    s.nextRepeatAt = 0;
    s.repeating = false;
    s.event = false;
  }
}

void Buttons::update(uint32_t now) {
  for (uint8_t i = 0; i < BTN_COUNT; i++) {
    State &s = btn_[i];
    bool raw = (digitalRead(s.pin) == LOW);  // pulled up, so pressed = LOW

    if (raw != s.rawPressed) {
      // Contacts are bouncing (or just changed); restart the settle timer.
      s.rawPressed = raw;
      s.rawChangedAt = now;
    }

    if (raw != s.stablePressed && (now - s.rawChangedAt) >= BTN_DEBOUNCE_MS) {
      s.stablePressed = raw;
      s.repeating = raw && s.canRepeat;
      if (raw) {
        s.event = true;
        s.nextRepeatAt = now + BTN_REPEAT_DELAY_MS;
      }
    }

    // Signed compare so this keeps working when millis() wraps (~49 days).
    if (s.repeating && (int32_t)(now - s.nextRepeatAt) >= 0) {
      s.event = true;
      s.nextRepeatAt = now + BTN_REPEAT_RATE_MS;
    }
  }
}

bool Buttons::consume(Button b) {
  bool e = btn_[b].event;
  btn_[b].event = false;
  return e;
}

void Buttons::clear() {
  for (uint8_t i = 0; i < BTN_COUNT; i++) btn_[i].event = false;
}
