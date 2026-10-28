// Display — every string that reaches the LCD is built here, so changing the
// UI wording never means touching the state machine.
#pragma once
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "config.h"

class Display {
public:
  Display();
  void begin();

  void showHome();
  void showModeSelect(bool euchreSelected);
  void showPlayers(uint8_t players);
  void showCards(uint8_t players, uint8_t cards);  // shows "Too many" when players*cards > deck
  void showEuchre();
  void showConfirm(uint8_t players, uint8_t totalCards, bool euchre);
  void showDealing(uint8_t dealt, uint8_t total, uint8_t player);
  void showDone(uint8_t total);
  void showJam(uint8_t dealt, uint8_t total, bool rotateStuck);
  void showFault(const __FlashStringHelper *line1, const __FlashStringHelper *line2);

private:
  // Writes a row only if its text changed. I2C LCD writes cost ~1 ms per
  // character, so redrawing an unchanged screen every loop would starve the
  // motor/beam polling.
  void setLine(uint8_t row, const char *text);
  void setLineP(uint8_t row, const __FlashStringHelper *text);

  LiquidCrystal_I2C lcd_;
  char shown_[LCD_ROWS][LCD_COLS + 1];
};
