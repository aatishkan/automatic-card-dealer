// Dealer — what to deal (DealPlan) and the non-blocking loop that deals it.
#pragma once
#include <Arduino.h>
#include "Motors.h"

// A deal is a number of passes around the table. In each pass, every player
// gets cardsAt(pass, player) cards in a row before the head moves on.
//   Classic 4 players x 5 cards -> 5 passes, 1 card each per pass.
//   Euchre                       -> 2 passes, 3/2/3/2 then 2/3/2/3.
struct DealPlan {
  uint8_t players = 0;
  uint8_t passes = 0;
  const uint8_t *table = nullptr;  // passes x players, row-major. nullptr = 1 card every time.
  bool euchre = false;

  uint8_t cardsAt(uint8_t pass, uint8_t player) const {
    return table ? table[pass * players + player] : 1;
  }
  uint8_t totalCards() const;

  static DealPlan classic(uint8_t players, uint8_t cardsEach);
  static DealPlan euchreDeal();
};

enum DealerStatus : uint8_t {
  DEALER_IDLE,
  DEALER_RUNNING,
  DEALER_JAMMED,    // motors stopped; waiting for retry() or abort()
  DEALER_FINISHED
};

enum JamCause : uint8_t {
  JAM_FEED,    // break-beam never saw the card
  JAM_ROTATE   // head didn't reach the seat in time
};

class Dealer {
public:
  void begin();
  void start(const DealPlan &plan, uint32_t now);
  void update(uint32_t now);
  void retry(uint32_t now);  // re-attempt whatever jammed (the card, or the rotation)
  void abort();              // stop all motors right now

  DealerStatus status() const { return status_; }
  JamCause jamCause() const { return jamCause_; }
  uint8_t dealt() const { return dealt_; }
  uint8_t total() const { return plan_.totalCards(); }
  uint8_t currentPlayer() const { return player_; }

private:
  enum Phase : uint8_t { HOMING_START, ROTATING, FEEDING, SETTLING, HOMING_END };

  void beginStep(uint32_t now);
  void advance();
  bool rotateDone();  // true once the head has arrived; flags a jam if it faulted

  FeedMotor    feed_;
  RotateAxis  *rotate_ = nullptr;
  DealPlan     plan_;
  DealerStatus status_ = DEALER_IDLE;
  JamCause     jamCause_ = JAM_FEED;
  Phase        phase_ = HOMING_START;
  uint8_t      pass_ = 0;
  uint8_t      player_ = 0;
  uint8_t      cardInStep_ = 0;  // cards already given to this player in this pass
  uint8_t      dealt_ = 0;
  uint32_t     phaseStart_ = 0;
};
