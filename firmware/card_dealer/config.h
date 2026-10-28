// config.h — every pin, I2C address and tunable lives here.
// If you find yourself typing a magic number in another file, it belongs here.
#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Build flags
// ---------------------------------------------------------------------------

// 1 = no motor shield needed. The motor layer skips all shield I2C traffic and
// fakes the break-beam a short time after each feed starts, so the whole menu
// -> deal -> done flow runs on a bare Uno + LCD + buttons.
// 0 = real hardware (requires the Adafruit Motor Shield V2 library).
// (#ifndef so CI / arduino-cli can override with -DSIMULATE_MOTORS=0.)
#ifndef SIMULATE_MOTORS
#define SIMULATE_MOTORS 1
#endif

// Rotate hardware is still being decided. Pick exactly ONE:
//   ROTATE_FEEDBACK360 — Parallax Feedback 360 servo; angle read from its
//                        feedback pin. Doesn't use a shield channel.
//   ROTATE_DC_HALL     — DC gearmotor on shield M2; a hall sensor on the head
//                        counts magnets on the base, one per seat.
#if !defined(ROTATE_FEEDBACK360) && !defined(ROTATE_DC_HALL)
#define ROTATE_FEEDBACK360
#endif
#if defined(ROTATE_FEEDBACK360) && defined(ROTATE_DC_HALL)
#error "Define only one of ROTATE_FEEDBACK360 / ROTATE_DC_HALL"
#endif

// 1 = print state changes and dealer events to Serial at SERIAL_BAUD.
#define DEBUG_SERIAL 1
#define SERIAL_BAUD 115200

#if DEBUG_SERIAL
  #define DBG_PRINT(x)   Serial.print(x)
  #define DBG_PRINTLN(x) Serial.println(x)
#else
  #define DBG_PRINT(x)   do {} while (0)
  #define DBG_PRINTLN(x) do {} while (0)
#endif

// ---------------------------------------------------------------------------
// Power assumptions (see docs/power-budget.md)
// ---------------------------------------------------------------------------
// 2S Li-ion (6.0–8.4 V) through a BMS. Everything (battery, Uno, LCD,
// buttons, motors) rides on the rotating head, so no wires cross the joint.
//  - Logic: battery -> Uno VIN -> onboard 5 V regulator. The regulator needs
//    roughly VIN >= 6.5 V to hold 5 V, so the Uno browns out before a typical
//    BMS cutoff — that's why we want a battery sense pin eventually.
//  - Motors: battery -> Motor Shield external power terminal, with the shield's
//    VIN jumper REMOVED so motor current never flows through the Uno.
//  - Feedback 360 servo (if chosen): its own 6 V BEC from the battery, never
//    the Uno 5 V pin. Ground shared with the Uno.

// ---------------------------------------------------------------------------
// Pins (Arduino Uno)
// ---------------------------------------------------------------------------
// A4/A5 (SDA/SCL) are taken by the I2C bus: LCD backpack + Motor Shield.
// D0/D1 are left free for USB serial.
const uint8_t PIN_BREAK_BEAM = 2;   // IR receiver output, LOW while a card blocks the beam
const uint8_t PIN_HALL       = 3;   // ROTATE_DC_HALL only. Interrupt pin so no magnet is missed
                                    // while the LCD is busy. Open-collector, LOW over a magnet.
const uint8_t PIN_BTN_UP     = 4;   // all buttons: pin -> button -> GND, INPUT_PULLUP
const uint8_t PIN_BTN_DOWN   = 5;
const uint8_t PIN_BTN_SELECT = 6;
const uint8_t PIN_BTN_HOME   = 7;
const uint8_t PIN_SERVO_FEEDBACK = 8; // ROTATE_FEEDBACK360 only: yellow feedback wire
const uint8_t PIN_SERVO_SIGNAL   = 9; // ROTATE_FEEDBACK360 only: white control wire.
                                      // Servo lib takes Timer1, so no analogWrite on D9/D10.
// Reserved, not used yet: battery voltage divider (e.g. 10k/4.7k) for a
// low-battery lockout. Keep A0 free for it.
const uint8_t PIN_VBAT_SENSE = A0;

// ---------------------------------------------------------------------------
// I2C
// ---------------------------------------------------------------------------
const uint8_t LCD_I2C_ADDR   = 0x27;  // common PCF8574 backpack; some are 0x3F
const uint8_t LCD_COLS       = 16;
const uint8_t LCD_ROWS       = 2;
const uint8_t SHIELD_I2C_ADDR = 0x60; // Adafruit Motor Shield v2 default (no address jumpers)

// Motor Shield channels (M1..M4 screw terminals)
const uint8_t FEED_MOTOR_PORT   = 1;
const uint8_t ROTATE_MOTOR_PORT = 2;  // ROTATE_DC_HALL only

// ---------------------------------------------------------------------------
// Buttons
// ---------------------------------------------------------------------------
const uint16_t BTN_DEBOUNCE_MS     = 30;
const uint16_t BTN_REPEAT_DELAY_MS = 400; // hold this long before UP/DOWN starts repeating
const uint16_t BTN_REPEAT_RATE_MS  = 120; // then one step every this many ms

// ---------------------------------------------------------------------------
// Game limits
// ---------------------------------------------------------------------------
const uint8_t DECK_SIZE   = 52;
const uint8_t MIN_PLAYERS = 2;
const uint8_t MAX_PLAYERS = 8;
const uint8_t MIN_CARDS   = 1;
const uint8_t MAX_CARDS   = 26;

const uint8_t DEFAULT_PLAYERS = 4;
const uint8_t DEFAULT_CARDS   = 5;

// ---------------------------------------------------------------------------
// Motors — common
// ---------------------------------------------------------------------------
// Soft start: ramp 0 -> target over this long to limit inrush current
// (a DC motor at rest draws its full stall current until it spins up).
const uint16_t MOTOR_RAMP_MS      = 100;
const uint8_t  MOTOR_RAMP_STEP_MS = 10;  // how often we push a new PWM value over I2C

// ---------------------------------------------------------------------------
// Feed (M1): two rubber wheels push the bottom card through a one-card slot
// ---------------------------------------------------------------------------
// PWM is 0..255 of battery voltage. A full 2S pack is 8.4 V and small "TT" /
// N20 gearmotors are rated ~6 V, so we cap well below 255.
const uint8_t FEED_PWM = 170;

// If the break-beam hasn't seen a card enter AND leave within this window,
// stop the feed motor and report a jam. Also keeps the motor/driver from
// sitting at stall current.
const uint16_t FEED_TIMEOUT_MS = 2000;

// After the card clears the beam, brake, then spin the wheels backwards
// briefly so the next card (dragged forward by friction) re-seats behind
// the slot instead of half-poking out.
const uint16_t FEED_BRAKE_GAP_MS   = 20;  // never reverse straight from forward: that's a current spike
const uint16_t FEED_RECOIL_MS      = 80;
const uint8_t  FEED_RECOIL_PWM     = 140;
const uint16_t FEED_RECOIL_RAMP_MS = 20;  // short ramp, or an 80 ms pulse would never reach speed

// Pause after each card so it can land before the next one is pushed / the
// head starts turning.
const uint16_t CARD_SETTLE_MS = 150;

// ---------------------------------------------------------------------------
// Rotate — common
// ---------------------------------------------------------------------------
// Fixed stops around the ring (one magnet each for ROTATE_DC_HALL). Players
// are spread evenly over these: 4 players on 8 seats use every other seat.
const uint8_t SEAT_COUNT = 8;

// Every rotation goes forward only. Nothing crosses the rotating joint, so
// there's no cable to wind up, and always approaching a seat from the same
// side takes gear backlash out of the stopping position.

// ---------------------------------------------------------------------------
// Rotate — ROTATE_FEEDBACK360
// ---------------------------------------------------------------------------
// Pinion on the servo walks around the fixed internal ring gear. The servo
// turns RING/PINION times per head revolution; we track its turns to know
// where the head is. Get the real tooth counts from mech.
const uint16_t RING_TEETH   = 120;
const uint16_t PINION_TEETH = 12;

// Feedback 360 control: 1500 us = stop, ±~220 us = full speed, ±~20 us deadband.
const uint16_t SERVO_STOP_US      = 1500;
const uint16_t SERVO_MAX_DELTA_US = 120; // speed cap; well under full speed for gentle starts/stops
const uint16_t SERVO_MIN_DELTA_US = 30;  // just outside the deadband so small errors still move
// Proportional gain: us of command per servo-degree of error.
const uint8_t  SERVO_KP_US_PER_DEG = 1;
// Stop when within this many SERVO degrees (head degrees x RING/PINION).
const uint8_t  SERVO_TOLERANCE_DEG = 8;
// Flip if the head turns the wrong way (depends on which side the pinion sits).
const bool     SERVO_REVERSED = false;

// ---------------------------------------------------------------------------
// Rotate — ROTATE_DC_HALL
// ---------------------------------------------------------------------------
const uint8_t  ROTATE_DC_PWM        = 150;
const uint8_t  ROTATE_DC_APPROACH_PWM = 90;   // slow down for the last seat so we stop on the magnet
const uint16_t HALL_MIN_GAP_MS      = 40;     // ignore re-triggers closer than this (edge chatter)

// Both: if a single seat-to-seat move takes longer than this, stop and
// report "Rotate stuck?" (stalled gear, servo running backwards, missed magnet).
const uint16_t ROTATE_TIMEOUT_PER_SEAT_MS = 1500;

// ---------------------------------------------------------------------------
// Simulation (only used when SIMULATE_MOTORS == 1)
// ---------------------------------------------------------------------------
const uint16_t SIM_BEAM_BLOCK_AT_MS  = 250; // fake card reaches the beam this long after feed starts
const uint16_t SIM_BEAM_BLOCK_LEN_MS = 120; // and blocks it for this long
// Every Nth simulated card never reaches the beam, to exercise the "Jam?"
// screen. 0 = never jam.
const uint8_t  SIM_JAM_EVERY_N = 0;
const uint16_t SIM_MS_PER_SEAT = 300;       // DC_HALL sim: a fake magnet passes this often
const uint16_t SIM_SERVO_DEG_PER_S_AT_MAX = 720; // FEEDBACK360 sim: fake servo speed at SERVO_MAX_DELTA_US
