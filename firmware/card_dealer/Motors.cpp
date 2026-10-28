#include "Motors.h"

// Hardware libraries are only pulled in for real hardware, so a breadboard
// build with SIMULATE_MOTORS doesn't even need them installed.
#if !SIMULATE_MOTORS
#include <Adafruit_MotorShield.h>
static Adafruit_MotorShield shield(SHIELD_I2C_ADDR);
#if defined(ROTATE_FEEDBACK360)
#include <Servo.h>
static Servo rotateServo;
#endif
#endif

bool motorsBegin() {
#if SIMULATE_MOTORS
  return true;
#else
  return shield.begin();  // default 1.6 kHz PWM
#endif
}

// ===========================================================================
// DcChannel
// ===========================================================================

void DcChannel::begin() {
#if !SIMULATE_MOTORS
  Adafruit_DCMotor *m = shield.getMotor(port_);
  motor_ = m;
  m->setSpeed(0);
  m->run(RELEASE);
#endif
  running_ = false;
}

void DcChannel::writePwm(uint8_t pwm) {
  current_ = pwm;
#if !SIMULATE_MOTORS
  static_cast<Adafruit_DCMotor *>(motor_)->setSpeed(pwm);
#endif
}

void DcChannel::run(uint8_t targetPwm, bool forward, uint32_t now, uint16_t rampMs) {
  target_ = targetPwm;
  rampMs_ = rampMs;
  rampStart_ = now;
  lastStep_ = now;
  running_ = true;
  // Always start from 0: a DC motor at rest looks like a short circuit, so
  // jumping straight to full PWM pulls stall current and can sag the battery
  // enough to reset the Uno.
  writePwm(0);
#if !SIMULATE_MOTORS
  static_cast<Adafruit_DCMotor *>(motor_)->run(forward ? FORWARD : BACKWARD);
#else
  (void)forward;
#endif
}

void DcChannel::setSpeed(uint8_t targetPwm) {
  target_ = targetPwm;
  // Slowing down is safe to do at once; speeding up still goes through the ramp.
  if (running_ && current_ > target_) writePwm(target_);
}

void DcChannel::update(uint32_t now) {
  if (!running_ || current_ >= target_) return;
  // Rate-limit: each setSpeed() is an I2C transaction shared with the LCD.
  if (now - lastStep_ < MOTOR_RAMP_STEP_MS) return;
  lastStep_ = now;

  uint32_t elapsed = now - rampStart_;
  uint8_t pwm = (rampMs_ == 0 || elapsed >= rampMs_)
                    ? target_
                    : (uint8_t)((uint32_t)target_ * elapsed / rampMs_);
  writePwm(pwm);
}

void DcChannel::brake() {
  writePwm(0);
#if !SIMULATE_MOTORS
  static_cast<Adafruit_DCMotor *>(motor_)->run(BRAKE);
#endif
  running_ = false;
}

void DcChannel::release() {
  writePwm(0);
#if !SIMULATE_MOTORS
  static_cast<Adafruit_DCMotor *>(motor_)->run(RELEASE);
#endif
  running_ = false;
}

// ===========================================================================
// FeedMotor
// ===========================================================================

void FeedMotor::begin() {
  // Most IR break-beam receivers are open-collector: they pull LOW and need
  // a pull-up to read HIGH. The internal one is enough on short wires.
  pinMode(PIN_BREAK_BEAM, INPUT_PULLUP);
  motor_.begin();
  result_ = FEED_IDLE;
}

bool FeedMotor::beamBlocked(uint32_t now) {
#if SIMULATE_MOTORS
  if (result_ != FEED_BUSY || simWillJam_) return false;
  uint32_t t = now - startedAt_;
  return t >= SIM_BEAM_BLOCK_AT_MS && t < (uint32_t)SIM_BEAM_BLOCK_AT_MS + SIM_BEAM_BLOCK_LEN_MS;
#else
  (void)now;
  // TODO: add a 1–2 ms glitch filter if the beam turns out to be noisy with
  // motors running.
  return digitalRead(PIN_BREAK_BEAM) == LOW;
#endif
}

void FeedMotor::feedOne(uint32_t now) {
  result_ = FEED_BUSY;
  phase_ = WAIT_ENTER;
  startedAt_ = now;
#if SIMULATE_MOTORS
  simCount_++;
  simWillJam_ = (SIM_JAM_EVERY_N != 0) && (simCount_ % SIM_JAM_EVERY_N == 0);
#endif

  // A blocked beam before we've pushed anything means a card is stuck in the
  // slot. Feeding now would jam a second card behind it.
  if (beamBlocked(now)) {
    jam();
    return;
  }
  motor_.run(FEED_PWM, true, now);
}

void FeedMotor::update(uint32_t now) {
  motor_.update(now);
  if (result_ != FEED_BUSY) return;

  switch (phase_) {
    case WAIT_ENTER:
    case WAIT_CLEAR: {
      bool blocked = beamBlocked(now);
      // We count a card on the trailing edge (beam clears), not the leading
      // edge, so the card is fully out of the slot before we recoil — reversing
      // earlier would pull the card being dealt back in.
      // TODO: a double-feed (two overlapping cards) looks like one long pulse.
      // If that's a problem, flag pulses much longer than one card.
      if (phase_ == WAIT_ENTER && blocked) {
        phase_ = WAIT_CLEAR;
      } else if (phase_ == WAIT_CLEAR && !blocked) {
        // Brake rather than coast so the wheels stop dragging the next card.
        motor_.brake();
        phase_ = BRAKE_GAP;
        phaseAt_ = now;
        break;
      }
      if (now - startedAt_ >= FEED_TIMEOUT_MS) jam();
      break;
    }

    case BRAKE_GAP:
      if (now - phaseAt_ < FEED_BRAKE_GAP_MS) break;
      if (FEED_RECOIL_MS == 0) {
        result_ = FEED_OK;
        break;
      }
      motor_.run(FEED_RECOIL_PWM, false, now, FEED_RECOIL_RAMP_MS);
      phase_ = RECOIL;
      phaseAt_ = now;
      break;

    case RECOIL:
      if (now - phaseAt_ >= FEED_RECOIL_MS) {
        motor_.brake();
        result_ = FEED_OK;
      }
      break;
  }
}

void FeedMotor::jam() {
  motor_.brake();
  result_ = FEED_JAM;
}

void FeedMotor::stop() {
  motor_.brake();
  result_ = FEED_IDLE;
}

// ===========================================================================
// RotateAxis — shared seat math
// ===========================================================================

void RotateAxis::setPlayerCount(uint8_t players) {
  players_ = constrain(players, 1, SEAT_COUNT);
}

uint8_t RotateAxis::seatOf(uint8_t player) const {
  // Spread players evenly over the seats, rounded: 4 of 8 -> 0,2,4,6;
  // 3 of 8 -> 0,3,5.
  return (uint8_t)(((uint16_t)player * SEAT_COUNT + players_ / 2) / players_);
}

void RotateAxis::rotateToPlayer(uint8_t index, uint32_t now) {
  targetPlayer_ = index % players_;
  faulted_ = false;
  uint8_t delta = (seatOf(targetPlayer_) + SEAT_COUNT - currentSeat()) % SEAT_COUNT;
  moveForwardSeats(delta, now);
}

void RotateAxis::homeRotation(uint32_t now) {
  rotateToPlayer(0, now);
}

void RotateAxis::rotateByPlayers(int8_t n, uint32_t now) {
  int16_t p = ((int16_t)targetPlayer_ + n) % players_;
  if (p < 0) p += players_;
  rotateToPlayer((uint8_t)p, now);
}

// ===========================================================================
// Feedback360Axis
// ===========================================================================
#if defined(ROTATE_FEEDBACK360)

void Feedback360Axis::begin() {
#if !SIMULATE_MOTORS
  pinMode(PIN_SERVO_FEEDBACK, INPUT);
  rotateServo.attach(PIN_SERVO_SIGNAL);
#endif
  writeCommand(0);
  // TODO: the servo only knows its own angle, not which seat the head faces
  // (many servo turns per head turn). Rule for now: point the head at
  // player 1 before power-on. A home magnet + hall sensor would fix this.
  int16_t d = -1;
  for (uint8_t i = 0; i < 5 && d < 0; i++) d = readServoDeg();
  lastServoDeg_ = d < 0 ? 0 : d;
  servoTotalDeg_ = 0;
  moving_ = false;
}

int16_t Feedback360Axis::readServoDeg() {
#if SIMULATE_MOTORS
  int32_t deg = (simMilliDeg_ / 1000) % 360;
  return (int16_t)(deg < 0 ? deg + 360 : deg);
#else
  // The feedback wire is a ~910 Hz PWM whose duty cycle (2.9%–97.1%) encodes
  // the angle. pulseIn blocks for up to ~2 cycles (~2.5 ms) — short enough
  // for our loop, and only called while the head is moving.
  // TODO: move to a pin-change interrupt if the blocking ever matters.
  uint32_t hi = pulseIn(PIN_SERVO_FEEDBACK, HIGH, 2500);
  uint32_t lo = pulseIn(PIN_SERVO_FEEDBACK, LOW, 2500);
  uint32_t cycle = hi + lo;
  if (hi == 0 || lo == 0 || cycle < 1000 || cycle > 1200) return -1;

  // Parallax's formula, in per-mille so it stays integer math.
  int32_t duty = 1000L * hi / cycle;
  int32_t deg = 359 - ((duty - 29) * 360) / (971 - 29 + 1);
  return (int16_t)constrain(deg, 0, 359);
#endif
}

void Feedback360Axis::trackAngle() {
#if SIMULATE_MOTORS
  // Fake servo: speed proportional to the command we last sent.
  uint32_t now = millis();
  simMilliDeg_ += (int32_t)simCmdUs_ * SIM_SERVO_DEG_PER_S_AT_MAX * (int32_t)(now - simLast_) / SERVO_MAX_DELTA_US;
  simLast_ = now;
#endif
  int16_t d = readServoDeg();
  if (d < 0) return;  // skip a bad read; the next one will catch up
  // Readings wrap 359 -> 0. Take the short way round; fine as long as we
  // sample before the servo turns 180° (it would need >2 rev/s at 4 ms loops).
  int16_t delta = d - lastServoDeg_;
  if (delta > 180) delta -= 360;
  if (delta < -180) delta += 360;
  servoTotalDeg_ += delta;
  lastServoDeg_ = d;
}

void Feedback360Axis::writeCommand(int16_t deltaUs) {
#if SIMULATE_MOTORS
  simCmdUs_ = deltaUs;
  simLast_ = millis();
#else
  rotateServo.writeMicroseconds(SERVO_STOP_US + (SERVO_REVERSED ? -deltaUs : deltaUs));
#endif
}

int32_t Feedback360Axis::servoDegForSeat(int32_t seatAbs) const {
  // Computed from the absolute seat count each time so rounding never accumulates.
  return seatAbs * 360L * RING_TEETH / ((int32_t)PINION_TEETH * SEAT_COUNT);
}

int32_t Feedback360Axis::seatAbsNow() const {
  // Inverse of servoDegForSeat(), rounded to the nearest seat.
  const int32_t den = 360L * RING_TEETH;
  return (servoTotalDeg_ * (int32_t)PINION_TEETH * SEAT_COUNT + den / 2) / den;
}

uint8_t Feedback360Axis::currentSeat() const {
  int32_t s = seatAbsNow() % SEAT_COUNT;
  return (uint8_t)(s < 0 ? s + SEAT_COUNT : s);
}

void Feedback360Axis::moveForwardSeats(uint8_t seats, uint32_t now) {
  trackAngle();
  // Target is an absolute seat, so a move that was cut short (abort, jam)
  // corrects itself on the next one.
  targetSeatAbs_ = seatAbsNow() + seats;
  targetDeg_ = servoDegForSeat(targetSeatAbs_);
  moveStart_ = now;
  timeoutMs_ = (uint32_t)ROTATE_TIMEOUT_PER_SEAT_MS * (seats ? seats : 1);
  moving_ = true;  // even for 0 seats: update() trims any small offset, then stops
}

void Feedback360Axis::update(uint32_t now) {
  if (!moving_) return;
  trackAngle();

  int32_t err = targetDeg_ - servoTotalDeg_;
  if (abs(err) <= SERVO_TOLERANCE_DEG) {
    writeCommand(0);
    moving_ = false;
    return;
  }
  if (now - moveStart_ >= timeoutMs_) {
    writeCommand(0);
    moving_ = false;
    faulted_ = true;
    return;
  }

  // Proportional control: fast when far, slow near the seat.
  int32_t mag = min(abs(err) * SERVO_KP_US_PER_DEG, (int32_t)SERVO_MAX_DELTA_US);
  // Soft start: let the speed cap rise over MOTOR_RAMP_MS.
  uint32_t elapsed = now - moveStart_;
  if (elapsed < MOTOR_RAMP_MS) {
    int32_t cap = SERVO_MIN_DELTA_US + (int32_t)(SERVO_MAX_DELTA_US - SERVO_MIN_DELTA_US) * elapsed / MOTOR_RAMP_MS;
    mag = min(mag, cap);
  }
  mag = max(mag, (int32_t)SERVO_MIN_DELTA_US);
  writeCommand(err > 0 ? (int16_t)mag : -(int16_t)mag);
}

void Feedback360Axis::stop() {
  writeCommand(0);
  moving_ = false;
}

#endif  // ROTATE_FEEDBACK360

// ===========================================================================
// DcHallAxis
// ===========================================================================
#if defined(ROTATE_DC_HALL)

static volatile uint8_t  hallFires = 0;

#if !SIMULATE_MOTORS
static volatile uint32_t hallLastMs = 0;

// Counted in an interrupt so a magnet can't slip past while the main loop is
// busy writing to the LCD.
static void onHallEdge() {
  uint32_t now = millis();
  if (now - hallLastMs >= HALL_MIN_GAP_MS) {
    hallFires++;
    hallLastMs = now;
  }
}
#endif

void DcHallAxis::begin() {
  motor_.begin();
#if !SIMULATE_MOTORS
  pinMode(PIN_HALL, INPUT_PULLUP);  // A3144-style sensors are open-collector
  attachInterrupt(digitalPinToInterrupt(PIN_HALL), onHallEdge, FALLING);
#endif
  // TODO: every magnet looks the same, so we can't tell seat 0 from the
  // others. Rule for now: point the head at player 1 before power-on.
  // Fix: a separate home magnet on a second track + a second hall sensor.
  seatsPassed_ = 0;
  moving_ = false;
}

uint8_t DcHallAxis::takeFires() {
  noInterrupts();
  uint8_t f = hallFires;
  hallFires = 0;
  interrupts();
  return f;
}

void DcHallAxis::moveForwardSeats(uint8_t seats, uint32_t now) {
  takeFires();  // drop any stale edges from before this move
  if (seats == 0) {
    moving_ = false;
    return;
  }
  seatsLeft_ = seats;
  moving_ = true;
  lastProgress_ = now;
#if SIMULATE_MOTORS
  simLastFire_ = now;
#endif
  // Starting while already over a magnet is fine: we count FALLING edges,
  // i.e. arriving at a magnet, not leaving one.
  motor_.run(seatsLeft_ == 1 ? ROTATE_DC_APPROACH_PWM : ROTATE_DC_PWM, true, now);
}

void DcHallAxis::update(uint32_t now) {
  motor_.update(now);
  if (!moving_) return;

#if SIMULATE_MOTORS
  uint8_t fires = 0;
  if (now - simLastFire_ >= SIM_MS_PER_SEAT) {
    fires = 1;
    simLastFire_ = now;
  }
#else
  uint8_t fires = takeFires();
#endif

  if (fires) {
    uint8_t n = min(fires, seatsLeft_);
    seatsLeft_ -= n;
    seatsPassed_ += n;
    lastProgress_ = now;
    if (seatsLeft_ == 0) {
      // Stop on the leading edge of the magnet. Because we always arrive from
      // the same direction, that's the same spot every time — mech can shift
      // the magnets to line the slot up with the seat.
      motor_.brake();
      moving_ = false;
      return;
    }
    // Slow for the last seat so braking lands on the magnet, not past it.
    if (seatsLeft_ == 1) motor_.setSpeed(ROTATE_DC_APPROACH_PWM);
  }

  if (now - lastProgress_ >= ROTATE_TIMEOUT_PER_SEAT_MS) {
    motor_.brake();
    moving_ = false;
    faulted_ = true;
  }
}

void DcHallAxis::stop() {
  motor_.brake();
  moving_ = false;
}

#endif  // ROTATE_DC_HALL
