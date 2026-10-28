// Automatic card dealer — Theta Tau pledge-class project.
// See README.md for build/flash steps and docs/ for wiring and power.
//
// This file only wires the modules together. The behaviour lives in
// StateMachine.cpp (UI flow) and Dealer.cpp (dealing sequence).

#include <Wire.h>
#include "config.h"
#include "Buttons.h"
#include "Display.h"
#include "Dealer.h"
#include "StateMachine.h"

Buttons buttons;
Display display;
Dealer dealer;
StateMachine ui(buttons, display, dealer);

void setup() {
#if DEBUG_SERIAL
  Serial.begin(SERIAL_BAUD);
  DBG_PRINTLN(F("Card dealer booting"));
#if SIMULATE_MOTORS
  DBG_PRINTLN(F("SIMULATE_MOTORS=1: no motors will move"));
#endif
#endif

  Wire.begin();
  display.begin();

  if (!motorsBegin()) {
    // Without the shield we can't stop a motor that's somehow running, so
    // refuse to go further. This is the one place a blocking wait is OK.
    display.showFault(F("No motor shield"), F("Check I2C/power"));
    DBG_PRINTLN(F("Motor shield not found on I2C"));
    while (true) delay(1000);
  }

  buttons.begin();
  dealer.begin();
  ui.begin();
}

void loop() {
  ui.update(millis());
}
