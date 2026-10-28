#include "StateMachine.h"

#if DEBUG_SERIAL
static const char *stateName(UiState s) {
  switch (s) {
    case ST_HOME:            return "HOME";
    case ST_MODE_SELECT:     return "MODE_SELECT";
    case ST_CLASSIC_PLAYERS: return "CLASSIC_PLAYERS";
    case ST_CLASSIC_CARDS:   return "CLASSIC_CARDS";
    case ST_EUCHRE:          return "EUCHRE";
    case ST_CONFIRM:         return "CONFIRM";
    case ST_DEALING:         return "DEALING";
    case ST_JAM:             return "JAM";
    case ST_DONE:            return "DONE";
  }
  return "?";
}
#endif

void StateMachine::begin() {
  enter(ST_HOME);
  render();
}

void StateMachine::enter(UiState s) {
  state_ = s;
  // A press that moved us here shouldn't also count on the new screen.
  buttons_.clear();
  DBG_PRINT(F("-> ")); DBG_PRINTLN(stateName(s));
}

void StateMachine::update(uint32_t now) {
  buttons_.update(now);
  dealer_.update(now);
  handleInput(now);
  // Called every loop; Display only touches the LCD when the text changes.
  render();
}

void StateMachine::handleInput(uint32_t now) {
  // HOME is a global escape hatch, and the only way to stop a deal.
  if (buttons_.consume(BTN_HOME)) {
    if (state_ == ST_DEALING || state_ == ST_JAM) dealer_.abort();
    if (state_ != ST_HOME) enter(ST_HOME);
    return;
  }

  bool up = buttons_.consume(BTN_UP);
  bool down = buttons_.consume(BTN_DOWN);
  bool select = buttons_.consume(BTN_SELECT);

  switch (state_) {
    case ST_HOME:
      if (select) enter(ST_MODE_SELECT);
      break;

    case ST_MODE_SELECT:
      if (up || down) euchre_ = !euchre_;  // only two modes, so either key toggles
      if (select) enter(euchre_ ? ST_EUCHRE : ST_CLASSIC_PLAYERS);
      break;

    case ST_CLASSIC_PLAYERS:
      if (up && players_ < MAX_PLAYERS) players_++;
      if (down && players_ > MIN_PLAYERS) players_--;
      if (select) enter(ST_CLASSIC_CARDS);
      break;

    case ST_CLASSIC_CARDS:
      // Let the user scroll past the deck limit and see "Too many" rather than
      // silently stopping — otherwise it looks like the button is broken.
      if (up && cards_ < MAX_CARDS) cards_++;
      if (down && cards_ > MIN_CARDS) cards_--;
      if (select && (uint16_t)players_ * cards_ <= DECK_SIZE) {
        plan_ = DealPlan::classic(players_, cards_);
        enter(ST_CONFIRM);
      }
      break;

    case ST_EUCHRE:
      if (select) {
        plan_ = DealPlan::euchreDeal();
        enter(ST_CONFIRM);
      }
      break;

    case ST_CONFIRM:
      if (select) {
        dealer_.start(plan_, now);
        enter(ST_DEALING);
      }
      break;

    case ST_DEALING:
      if (dealer_.status() == DEALER_JAMMED) enter(ST_JAM);
      else if (dealer_.status() == DEALER_FINISHED) enter(ST_DONE);
      break;

    case ST_JAM:
      if (select) {
        dealer_.retry(now);
        enter(ST_DEALING);
      }
      break;

    case ST_DONE:
      if (select) enter(ST_HOME);
      break;
  }
}

void StateMachine::render() {
  switch (state_) {
    case ST_HOME:            display_.showHome(); break;
    case ST_MODE_SELECT:     display_.showModeSelect(euchre_); break;
    case ST_CLASSIC_PLAYERS: display_.showPlayers(players_); break;
    case ST_CLASSIC_CARDS:   display_.showCards(players_, cards_); break;
    case ST_EUCHRE:          display_.showEuchre(); break;
    case ST_CONFIRM:         display_.showConfirm(plan_.players, plan_.totalCards(), plan_.euchre); break;
    case ST_DEALING:         display_.showDealing(dealer_.dealt(), dealer_.total(), dealer_.currentPlayer()); break;
    case ST_JAM:             display_.showJam(dealer_.dealt(), dealer_.total(), dealer_.jamCause() == JAM_ROTATE); break;
    case ST_DONE:            display_.showDone(dealer_.total()); break;
  }
}
