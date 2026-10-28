# Wiring

Source of truth for pin numbers is [`firmware/card_dealer/config.h`](../firmware/card_dealer/config.h).
If this table and `config.h` disagree, `config.h` wins — then fix this file.

## Mechanical context: everything rotates

The whole head (card box, Arduino + shield, battery, LCD, buttons) turns on
an internal ring gear driven by a pinion on the head. **No wires cross the
rotating joint** — no slip ring, no cable to wind up. Consequences for us:

- The firmware only ever rotates *forward*. Without a cable to unwind there's
  no reason to reverse, and always approaching a seat from the same side
  takes gear backlash out of the stopping position.
- Anything that has to sense the *stationary* world (the hall sensor) is
  mounted on the head and looks down at targets (magnets) fixed to the base.
  > Note: the mech sketch says "hall sensor on the base". That would need a
  > wire across the joint. We're assuming **sensor on the head, magnets on the
  > base** — see [mech-interface.md](mech-interface.md).
- The LCD and buttons spin with the head. The dealer returns to seat 0
  (player 1) at the end of every deal so the screen faces whoever started it.
- A USB cable to a laptop *does* cross the joint. Only debug over USB with
  `SIMULATE_MOTORS 1` or with the pinion disengaged.

## Pin table (Arduino Uno R3)

| Pin      | Direction | Connects to                          | Notes |
|----------|-----------|--------------------------------------|-------|
| A4 / SDA | I2C       | LCD backpack SDA, Motor Shield SDA   | Shield also uses the dedicated SDA/SCL header pins — same signal |
| A5 / SCL | I2C       | LCD backpack SCL, Motor Shield SCL   | |
| D2       | in        | IR break-beam receiver output        | `INPUT_PULLUP`; LOW = card in beam |
| D3       | in        | Hall sensor output (`ROTATE_DC_HALL`) | `INPUT_PULLUP`, interrupt on FALLING; LOW = over a magnet |
| D4       | in        | UP button → GND                      | `INPUT_PULLUP` |
| D5       | in        | DOWN button → GND                    | `INPUT_PULLUP` |
| D6       | in        | SELECT button → GND                  | `INPUT_PULLUP` |
| D7       | in        | HOME button → GND                    | `INPUT_PULLUP` |
| D8       | in        | Feedback 360 yellow (feedback)       | `ROTATE_FEEDBACK360` only |
| D9       | out       | Feedback 360 white (control)         | Servo lib uses Timer1 → no `analogWrite` on D9/D10 |
| A0       | in        | *(reserved)* battery voltage divider | Not used yet — keep free |
| D0/D1    | —         | USB serial                           | Leave free for debug output |
| VIN      | power     | Battery + (after BMS + switch)       | Uno's regulator makes 5 V |
| 5V       | power out | LCD, break-beam, hall sensor         | ~100 mA total — **not** the servo, **not** the motors |
| GND      | —         | Common ground for everything         | Battery −, shield, BEC, sensors |

I2C addresses: LCD backpack `0x27` (some are `0x3F`), Motor Shield v2 `0x60`.

Motor Shield v2 terminals:

| Terminal | Connects to | Notes |
|----------|-------------|-------|
| M1       | Feed motor (two rubber wheels on one axle) | Swap leads if it feeds backwards |
| M2       | Rotate DC gearmotor (`ROTATE_DC_HALL` only) | Unused with Feedback 360 |
| EXT PWR  | Battery + / − (after BMS + switch) | **Remove the shield's VIN jumper** so motor current never flows through the Uno |

## Text wiring diagram

```
                ROTATING HEAD — every part below rides on it
 ┌────────────────────────────────────────────────────────────────────────┐
 │                                                                        │
 │  [2S Li-ion]──[BMS]──[power switch]──+BATT (6.0–8.4 V)                 │
 │                                        │                               │
 │            ┌───────────────────────────┼─────────────────────┐         │
 │            │                           │                     │         │
 │            ▼                           ▼                     ▼         │
 │      Uno VIN                Shield EXT PWR +           6 V BEC in      │
 │      (5 V regulator)        (VIN jumper REMOVED)       (F360 option)   │
 │            │                     │        │                  │         │
 │      Uno 5V ──┬── LCD VCC        M1       M2                 └─► Servo │
 │               ├── Beam TX/RX +   │        │                     red    │
 │               └── Hall VCC     [Feed]  [Rotate DC]                     │
 │                                 motor   (DC_HALL option)               │
 │                                                                        │
 │      Uno A4/A5 ──── I2C bus ──┬── LCD backpack (0x27)                  │
 │                               └── Motor Shield (0x60, stacked)         │
 │                                                                        │
 │      D2 ◄── Break-beam RX out      (emitter + receiver at slot exit)   │
 │      D3 ◄── Hall sensor out        (looks DOWN at magnets on base)     │
 │      D4..D7 ◄── UP / DOWN / SELECT / HOME ── each to GND               │
 │      D8 ◄── Servo yellow (feedback)                                    │
 │      D9 ──► Servo white  (control)                                     │
 │                                                                        │
 │      GND: battery −, Uno, shield, BEC −, servo black, all sensors      │
 └────────────────────────────────────────────────────────────────────────┘
        ║ rotating joint — NOTHING electrical crosses ║
 ┌────────────────────────────────────────────────────────────────────────┐
 │  STATIONARY BASE: internal ring gear + one magnet per seat (8)         │
 └────────────────────────────────────────────────────────────────────────┘
```

## Wiring notes

- **Break-beam orientation matters.** A card is ~0.3 mm thick, so a beam that
  crosses the slot edge-on (in the plane of the card) barely gets blocked.
  The beam must pass *through the card's face*: emitter above the card path,
  receiver below (or vice versa).
- Twist each motor's two leads together and route them away from the I2C
  wires. Add a 0.1 µF ceramic cap across each DC motor's terminals.
- Put a 470–1000 µF / 16 V electrolytic across the shield's EXT PWR terminal.
- Keep the I2C run to the LCD short (< 30 cm).
- Only one rotate option is wired at a time; `config.h` says which.
