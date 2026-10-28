#include "Dealer.h"

// ---------------------------------------------------------------------------
// Deal plans
// ---------------------------------------------------------------------------

// Standard euchre deal: alternate 3 and 2 around the table, then swap, so
// everyone ends with 5. Rows are passes, columns are players.
static const uint8_t EUCHRE_PLAYERS = 4;
static const uint8_t EUCHRE_PASSES = 2;
static const uint8_t EUCHRE_TABLE[EUCHRE_PASSES][EUCHRE_PLAYERS] = {
  {3, 2, 3, 2},
  {2, 3, 2, 3},
};

uint8_t DealPlan::totalCards() const {
  uint8_t sum = 0;
  for (uint8_t p = 0; p < passes; p++)
    for (uint8_t i = 0; i < players; i++) sum += cardsAt(p, i);
  return sum;
}

DealPlan DealPlan::classic(uint8_t players, uint8_t cardsEach) {
  DealPlan plan;
  plan.players = players;
  plan.passes = cardsEach;  // one card per player per pass, like a human dealer
  return plan;
}

DealPlan DealPlan::euchreDeal() {
  DealPlan plan;
  plan.players = EUCHRE_PLAYERS;
  plan.passes = EUCHRE_PASSES;
  plan.table = &EUCHRE_TABLE[0][0];
  plan.euchre = true;
  return plan;
}

// ---------------------------------------------------------------------------
// Dealer
// ---------------------------------------------------------------------------

// The rotate hardware is chosen in config.h; this is the only place that
// knows which class that means.
#if defined(ROTATE_FEEDBACK360)
static Feedback360Axis rotateAxis;
#elif defined(ROTATE_DC_HALL)
static DcHallAxis rotateAxis;
#endif

void Dealer::begin() {
  rotate_ = &rotateAxis;
  feed_.begin();
  rotate_->begin();
  status_ = DEALER_IDLE;
}

void Dealer::start(const DealPlan &plan, uint32_t now) {
  plan_ = plan;
  pass_ = 0;
  player_ = 0;
  cardInStep_ = 0;
  dealt_ = 0;
  status_ = DEALER_RUNNING;
  rotate_->setPlayerCount(plan_.players);
  // Usually a no-op; after an aborted deal this brings the head back first.
  phase_ = HOMING_START;
  rotate_->homeRotation(now);
  DBG_PRINT(F("Deal start, cards=")); DBG_PRINTLN(plan_.totalCards());
}

void Dealer::advance() {
  cardInStep_ = 0;
  player_++;
  if (player_ >= plan_.players) {
    player_ = 0;
    pass_++;
  }
}

void Dealer::beginStep(uint32_t now) {
  // Skip seats that get nothing this pass so we don't turn to them for no reason.
  while (pass_ < plan_.passes && plan_.cardsAt(pass_, player_) == 0) advance();

  if (pass_ >= plan_.passes) {
    // End facing player 1 so the LCD (which rides on the head) faces
    // whoever started the deal.
    phase_ = HOMING_END;
    rotate_->homeRotation(now);
    return;
  }
  phase_ = ROTATING;
  rotate_->rotateToPlayer(player_, now);
}

bool Dealer::rotateDone() {
  if (rotate_->busy()) return false;
  if (rotate_->faulted()) {
    status_ = DEALER_JAMMED;
    jamCause_ = JAM_ROTATE;
    DBG_PRINTLN(F("ROTATE timeout"));
    return false;
  }
  return true;
}

void Dealer::update(uint32_t now) {
  rotate_->update(now);
  feed_.update(now);
  if (status_ != DEALER_RUNNING) return;

  // Motors run one at a time (rotate, then feed) so their startup currents
  // never stack — see docs/power-budget.md.
  switch (phase_) {
    case HOMING_START:
      if (rotateDone()) beginStep(now);
      break;

    case ROTATING:
      if (rotateDone()) {
        feed_.feedOne(now);
        phase_ = FEEDING;
      }
      break;

    case FEEDING:
      if (feed_.result() == FEED_OK) {
        dealt_++;
        cardInStep_++;
        phaseStart_ = now;
        phase_ = SETTLING;
        DBG_PRINT(F("Card ")); DBG_PRINT(dealt_);
        DBG_PRINT(F(" -> player ")); DBG_PRINTLN(player_ + 1);
      } else if (feed_.result() == FEED_JAM) {
        status_ = DEALER_JAMMED;
        jamCause_ = JAM_FEED;
        DBG_PRINT(F("JAM on card ")); DBG_PRINTLN(dealt_ + 1);
      }
      break;

    case SETTLING:
      if (now - phaseStart_ < CARD_SETTLE_MS) break;
      if (cardInStep_ < plan_.cardsAt(pass_, player_)) {
        feed_.feedOne(now);  // same player, next card (euchre 2s and 3s)
        phase_ = FEEDING;
      } else {
        advance();
        beginStep(now);
      }
      break;

    case HOMING_END:
      if (rotateDone()) {
        status_ = DEALER_FINISHED;
        DBG_PRINTLN(F("Deal finished"));
      }
      break;
  }
}

void Dealer::retry(uint32_t now) {
  if (status_ != DEALER_JAMMED) return;
  status_ = DEALER_RUNNING;
  if (jamCause_ == JAM_FEED) {
    phase_ = FEEDING;
    feed_.feedOne(now);
  } else if (phase_ == ROTATING) {
    rotate_->rotateToPlayer(player_, now);  // absolute target, so it picks up from wherever it stopped
  } else {
    rotate_->homeRotation(now);             // HOMING_START / HOMING_END
  }
}

void Dealer::abort() {
  feed_.stop();
  rotate_->stop();
  status_ = DEALER_IDLE;
  DBG_PRINTLN(F("Deal aborted"));
}
