// StateMachine — owns the UI flow. Reads buttons, tells the Dealer what to
// do, and picks which Display screen to show. No delays anywhere: update()
// is called every loop() and always returns quickly.
//
//   HOME -> MODE_SELECT -> CLASSIC_PLAYERS -> CLASSIC_CARDS -> CONFIRM
//                       \-> EUCHRE ---------------------------/
//   CONFIRM -> DEALING -> DONE -> HOME
//              DEALING <-> JAM   (SELECT retries the stuck card)
//   HOME button from any screen -> HOME (stops motors if dealing)
#pragma once
#include <Arduino.h>
#include "Buttons.h"
#include "Display.h"
#include "Dealer.h"

enum UiState : uint8_t {
  ST_HOME,
  ST_MODE_SELECT,
  ST_CLASSIC_PLAYERS,
  ST_CLASSIC_CARDS,
  ST_EUCHRE,
  ST_CONFIRM,
  ST_DEALING,
  ST_JAM,
  ST_DONE
};

class StateMachine {
public:
  StateMachine(Buttons &buttons, Display &display, Dealer &dealer)
      : buttons_(buttons), display_(display), dealer_(dealer) {}

  void begin();
  void update(uint32_t now);

private:
  void enter(UiState s);
  void handleInput(uint32_t now);
  void render();

  Buttons &buttons_;
  Display &display_;
  Dealer  &dealer_;

  UiState  state_ = ST_HOME;
  // Remembered between deals so a second hand of the same game is three presses.
  bool     euchre_ = false;
  uint8_t  players_ = DEFAULT_PLAYERS;
  uint8_t  cards_ = DEFAULT_CARDS;
  DealPlan plan_;
};
