// Motors — feed motor + break-beam, and the rotate axis.
//
// Everything above this layer (Dealer, StateMachine) only talks to FeedMotor
// and RotateAxis. Nothing here blocks: start a move, then call update() every
// loop and poll busy()/result().
#pragma once
#include <Arduino.h>
#include "config.h"

// Must be called once from setup() before using any motor object.
// Returns false if the shield didn't answer on I2C (always true in simulation).
bool motorsBegin();

// One DC motor channel on the shield, with a soft-start ramp.
class DcChannel {
public:
  explicit DcChannel(uint8_t port) : port_(port) {}
  void begin();
  // Ramps up from 0 to targetPwm over rampMs.
  void run(uint8_t targetPwm, bool forward, uint32_t now, uint16_t rampMs = MOTOR_RAMP_MS);
  void setSpeed(uint8_t targetPwm);  // change speed mid-run (e.g. slow approach)
  void brake();    // stop hard (short the windings)
  void release();  // coast, no holding current
  void update(uint32_t now);

private:
  void writePwm(uint8_t pwm);
  uint8_t  port_;
  void    *motor_ = nullptr;  // Adafruit_DCMotor*, kept opaque so this header doesn't need the library
  bool     running_ = false;
  uint8_t  target_ = 0;
  uint8_t  current_ = 0;
  uint16_t rampMs_ = MOTOR_RAMP_MS;
  uint32_t rampStart_ = 0;
  uint32_t lastStep_ = 0;
};

// ---------------------------------------------------------------------------
// Feed: push exactly one card out, confirm it with the break-beam, then
// recoil so the next card re-seats.
// ---------------------------------------------------------------------------
enum FeedResult : uint8_t {
  FEED_IDLE,     // nothing requested
  FEED_BUSY,     // pushing, or recoiling
  FEED_OK,       // a card entered and then cleared the beam, recoil done
  FEED_JAM       // timeout, or the beam was already blocked when we started
};

class FeedMotor {
public:
  FeedMotor() : motor_(FEED_MOTOR_PORT) {}
  void begin();
  void feedOne(uint32_t now);
  void update(uint32_t now);
  void stop();
  FeedResult result() const { return result_; }

private:
  bool beamBlocked(uint32_t now);
  void jam();

  enum Phase : uint8_t { WAIT_ENTER, WAIT_CLEAR, BRAKE_GAP, RECOIL };
  DcChannel  motor_;
  FeedResult result_ = FEED_IDLE;
  Phase      phase_ = WAIT_ENTER;
  uint32_t   startedAt_ = 0;
  uint32_t   phaseAt_ = 0;
#if SIMULATE_MOTORS
  uint8_t    simCount_ = 0;
  bool       simWillJam_ = false;
#endif
};

// ---------------------------------------------------------------------------
// Rotate: point the dealing head at a player.
//
// The public calls (homeRotation / rotateToPlayer / rotateByPlayers) are the
// same whichever hardware we pick. They're written once here in terms of
// "move forward N seats"; each hardware option only has to implement that
// primitive plus update()/stop(). config.h picks which one gets built.
// ---------------------------------------------------------------------------
class RotateAxis {
public:
  virtual ~RotateAxis() {}
  virtual void begin() = 0;
  virtual void update(uint32_t now) = 0;
  virtual void stop() = 0;
  virtual bool busy() const = 0;
  bool faulted() const { return faulted_; }  // last move timed out

  // How many players are at the table, so they can be spread over the seats.
  void setPlayerCount(uint8_t players);

  void homeRotation(uint32_t now);                  // back to seat 0 (player 1)
  void rotateToPlayer(uint8_t index, uint32_t now); // 0-based player index
  void rotateByPlayers(int8_t n, uint32_t now);     // relative; negative still turns forward

protected:
  // Implementations: start a forward move of `seats` seats (may be 0).
  virtual void moveForwardSeats(uint8_t seats, uint32_t now) = 0;
  // Implementations: which seat (0..SEAT_COUNT-1) the head is at right now.
  virtual uint8_t currentSeat() const = 0;

  bool faulted_ = false;

private:
  uint8_t seatOf(uint8_t player) const;
  uint8_t players_ = 1;
  uint8_t targetPlayer_ = 0;
};

#if defined(ROTATE_FEEDBACK360)
// Parallax Feedback 360 high-speed continuous servo. Closed loop on the
// servo's own angle sensor, unwrapped into a running total so we can follow
// it through the many servo turns one head revolution takes.
class Feedback360Axis : public RotateAxis {
public:
  void begin() override;
  void update(uint32_t now) override;
  void stop() override;
  bool busy() const override { return moving_; }

protected:
  void moveForwardSeats(uint8_t seats, uint32_t now) override;
  uint8_t currentSeat() const override;

private:
  int16_t readServoDeg();             // 0..359, or -1 if the feedback read failed
  void    trackAngle();               // fold the latest reading into servoTotalDeg_
  void    writeCommand(int16_t deltaUs);
  int32_t servoDegForSeat(int32_t seatAbs) const;
  int32_t seatAbsNow() const;

  int32_t  servoTotalDeg_ = 0;  // servo degrees turned since power-on (unwrapped)
  int16_t  lastServoDeg_ = 0;
  int32_t  targetDeg_ = 0;
  int32_t  targetSeatAbs_ = 0;  // seats since power-on, never wraps
  bool     moving_ = false;
  uint32_t moveStart_ = 0;
  uint32_t timeoutMs_ = 0;
#if SIMULATE_MOTORS
  int32_t  simMilliDeg_ = 0;
  int16_t  simCmdUs_ = 0;
  uint32_t simLast_ = 0;
#endif
};
#endif

#if defined(ROTATE_DC_HALL)
// DC gearmotor on shield M2. A hall sensor on the head passes over one magnet
// per seat on the fixed base; we count magnets, not time.
class DcHallAxis : public RotateAxis {
public:
  DcHallAxis() : motor_(ROTATE_MOTOR_PORT) {}
  void begin() override;
  void update(uint32_t now) override;
  void stop() override;
  bool busy() const override { return moving_; }

protected:
  void moveForwardSeats(uint8_t seats, uint32_t now) override;
  uint8_t currentSeat() const override { return seatsPassed_ % SEAT_COUNT; }

private:
  uint8_t takeFires();  // read + reset the ISR counter atomically

  DcChannel motor_;
  uint32_t  seatsPassed_ = 0;  // magnets passed since power-on
  uint8_t   seatsLeft_ = 0;
  bool      moving_ = false;
  uint32_t  lastProgress_ = 0;
#if SIMULATE_MOTORS
  uint32_t  simLastFire_ = 0;
#endif
};
#endif
