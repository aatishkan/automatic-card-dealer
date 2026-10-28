// Buttons — debounced pushbuttons with hold-to-repeat on UP/DOWN.
#pragma once
#include <Arduino.h>

enum Button : uint8_t {
  BTN_UP = 0,
  BTN_DOWN,
  BTN_SELECT,
  BTN_HOME,
  BTN_COUNT
};

class Buttons {
public:
  void begin();

  // Call once per loop(). Samples pins and queues press / repeat events.
  void update(uint32_t now);

  // True once per press (and once per repeat tick while UP/DOWN is held).
  // Reading the event clears it, so each consumer sees it exactly once.
  bool consume(Button b);

  // Drop any queued events, e.g. on a screen change so a press that was
  // meant for the old screen doesn't leak into the new one.
  void clear();

private:
  struct State {
    uint8_t  pin;
    bool     canRepeat;
    bool     rawPressed;     // last raw reading
    bool     stablePressed;  // debounced state
    uint32_t rawChangedAt;
    uint32_t nextRepeatAt;
    bool     repeating;      // only after a real press, so a button held at boot never auto-repeats
    bool     event;
  };
  State btn_[BTN_COUNT];
};
