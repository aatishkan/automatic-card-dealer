#include "Display.h"

// Every screen is two rows of at most 16 characters:
//   "................"   <- ruler

Display::Display() : lcd_(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS) {}

void Display::begin() {
  lcd_.init();
  lcd_.backlight();
  lcd_.clear();
  // Empty cache never matches a padded line, so the first draw always writes.
  memset(shown_, 0, sizeof(shown_));
}

void Display::setLine(uint8_t row, const char *text) {
  char padded[LCD_COLS + 1];
  uint8_t i = 0;
  for (; i < LCD_COLS && text[i] != '\0'; i++) padded[i] = text[i];
  for (; i < LCD_COLS; i++) padded[i] = ' ';  // overwrite leftovers from the last screen
  padded[LCD_COLS] = '\0';

  if (strcmp(padded, shown_[row]) == 0) return;
  strcpy(shown_[row], padded);
  lcd_.setCursor(0, row);
  lcd_.print(padded);
}

void Display::setLineP(uint8_t row, const __FlashStringHelper *text) {
  // F() strings live in flash to save the Uno's 2 KB of RAM; copy out to compare.
  char buf[LCD_COLS + 1];
  strncpy_P(buf, (const char *)text, LCD_COLS);
  buf[LCD_COLS] = '\0';
  setLine(row, buf);
}

void Display::showHome() {
  setLineP(0, F("Card Dealer"));
  setLineP(1, F("SELECT to start"));
}

void Display::showModeSelect(bool euchreSelected) {
  setLineP(0, F("Select game:"));
  setLineP(1, euchreSelected ? F(" Classic >Euchre") : F(">Classic  Euchre"));
}

void Display::showPlayers(uint8_t players) {
  char buf[LCD_COLS + 1];
  snprintf_P(buf, sizeof(buf), PSTR("Players: %u"), players);
  setLine(0, buf);
  setLineP(1, F("UP/DN  SEL=next"));
}

void Display::showCards(uint8_t players, uint8_t cards) {
  char buf[LCD_COLS + 1];
  snprintf_P(buf, sizeof(buf), PSTR("Cards each: %u"), cards);
  setLine(0, buf);

  uint16_t total = (uint16_t)players * cards;
  if (total > DECK_SIZE) {
    snprintf_P(buf, sizeof(buf), PSTR("Too many %u>%u"), total, DECK_SIZE);
  } else {
    snprintf_P(buf, sizeof(buf), PSTR("Total: %u cards"), total);
  }
  setLine(1, buf);
}

void Display::showEuchre() {
  setLineP(0, F("Euchre 4 players"));
  setLineP(1, F("5 each (3/2) SEL"));
}

void Display::showConfirm(uint8_t players, uint8_t totalCards, bool euchre) {
  char buf[LCD_COLS + 1];
  if (euchre) {
    snprintf_P(buf, sizeof(buf), PSTR("Euchre: %u cards"), totalCards);
  } else {
    snprintf_P(buf, sizeof(buf), PSTR("Deal %ux%u=%u?"), players, totalCards / players, totalCards);
  }
  setLine(0, buf);
  setLineP(1, F("SEL=go HOME=quit"));
}

void Display::showDealing(uint8_t dealt, uint8_t total, uint8_t player) {
  char buf[LCD_COLS + 1];
  snprintf_P(buf, sizeof(buf), PSTR("Dealing %u/%u"), dealt, total);
  setLine(0, buf);
  // Players are 0-indexed internally, 1-indexed for humans.
  snprintf_P(buf, sizeof(buf), PSTR("Player %u  HOME=x"), player + 1);
  setLine(1, buf);
}

void Display::showDone(uint8_t total) {
  char buf[LCD_COLS + 1];
  snprintf_P(buf, sizeof(buf), PSTR("Done! %u cards"), total);
  setLine(0, buf);
  setLineP(1, F("SELECT for menu"));
}

void Display::showJam(uint8_t dealt, uint8_t total, bool rotateStuck) {
  if (rotateStuck) {
    setLineP(0, F("Rotate stuck?"));
  } else {
    char buf[LCD_COLS + 1];
    // dealt counts finished cards, so the stuck one is dealt + 1.
    snprintf_P(buf, sizeof(buf), PSTR("Jam? card %u/%u"), dealt + 1, total);
    setLine(0, buf);
  }
  setLineP(1, F("SEL=retry HOME=x"));
}

void Display::showFault(const __FlashStringHelper *line1, const __FlashStringHelper *line2) {
  setLineP(0, line1);
  setLineP(1, line2);
}
