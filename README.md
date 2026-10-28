# Automatic Card Dealer

Theta Tau pledge-class project: a battery-powered box that deals cards to
everyone at the table on game night. Pick a game on the LCD, press SELECT, and
the head turns to each player and pushes out exactly the right number of cards.

- **Classic** — 2–8 players, 1–26 cards each (up to a 52-card deck).
- **Euchre** — 4 players, 5 cards each, dealt 3-2 / 2-3 in two passes.

The whole head (card box, Arduino, battery, LCD, buttons) rotates on an
internal ring gear, so no wires cross the rotating joint.

## Hardware

| Part | Notes |
|---|---|
| Arduino Uno R3 | 5 V logic |
| 16×2 LCD + I2C backpack | `LiquidCrystal_I2C`, address `0x27` |
| 4 pushbuttons | UP, DOWN, SELECT, HOME — each pin to GND |
| Adafruit Motor Shield v2 | I2C `0x60`. M1 = feed motor, M2 = rotate DC motor (if used) |
| Feed | Two rubber wheels on one axle push the bottom card through a one-card slot |
| IR break-beam | At the slot exit; LOW while a card passes — how we know one card left |
| Rotate | **TBD:** Parallax Feedback 360 servo *or* DC gearmotor + hall sensor. Chosen in `config.h` |
| Power | 2S Li-ion + BMS. Logic from Uno VIN, motors from the shield's external terminal |

Details: [docs/wiring.md](docs/wiring.md) · [docs/power-budget.md](docs/power-budget.md) · [docs/mech-interface.md](docs/mech-interface.md)

## Repo layout

```
firmware/card_dealer/
  card_dealer.ino      setup()/loop() — just wires the modules together
  config.h             every pin, I2C address, timing and tuning constant
  Buttons.h/.cpp       debounced buttons, hold-to-repeat on UP/DOWN
  Display.h/.cpp       all LCD text lives here
  Motors.h/.cpp        feed motor + break-beam, rotate axis (both options)
  Dealer.h/.cpp        DealPlan (what to deal) + the dealing loop
  StateMachine.h/.cpp  menu flow: HOME → MODE → … → DEALING → DONE
docs/                  wiring, power budget, mech interface
```

How it fits together: `loop()` calls `StateMachine::update(millis())` and
nothing in the firmware ever calls `delay()` after `setup()`. Every module
has an `update(now)` that does a little work and returns; long actions (a
rotation, a card feed) are started, then polled until done.

## Required libraries

| Library | Install name | Needed when |
|---|---|---|
| LiquidCrystal I2C (Frank de Brabander) | `LiquidCrystal I2C` | always |
| Adafruit Motor Shield V2 | `Adafruit Motor Shield V2 Library` (pulls in Adafruit BusIO) | `SIMULATE_MOTORS 0` |
| Servo | `Servo` (bundled with the Arduino IDE) | `SIMULATE_MOTORS 0` + `ROTATE_FEEDBACK360` |

## Build and flash

### Arduino IDE 2.x

1. **Tools → Board → Arduino AVR Boards → Arduino Uno.**
2. **Sketch → Include Library → Manage Libraries…**, install the libraries above.
3. **File → Open…** → `firmware/card_dealer/card_dealer.ino`.
4. Pick the port under **Tools → Port**, then **Upload** (→).
5. **Tools → Serial Monitor** at **115200** baud to see debug output.

### arduino-cli

```bash
arduino-cli core install arduino:avr
```

```bash
arduino-cli lib install "LiquidCrystal I2C" "Adafruit Motor Shield V2 Library" "Servo"
```

```bash
arduino-cli compile --fqbn arduino:avr:uno firmware/card_dealer
```

```bash
arduino-cli board list
```

```bash
arduino-cli upload -p /dev/cu.usbmodem1101 --fqbn arduino:avr:uno firmware/card_dealer
```

```bash
arduino-cli monitor -p /dev/cu.usbmodem1101 -c baudrate=115200
```

(Replace the port with the one `board list` shows — `COM3` on Windows,
`/dev/ttyACM0` on Linux.)

Build every motor/rotate configuration without editing `config.h`
(handy before pushing):

```bash
arduino-cli compile --fqbn arduino:avr:uno --build-property "compiler.cpp.extra_flags=-DSIMULATE_MOTORS=0 -DROTATE_DC_HALL" firmware/card_dealer
```

## Configuration

Everything is in [`config.h`](firmware/card_dealer/config.h). The ones you'll touch first:

| Setting | What it does |
|---|---|
| `SIMULATE_MOTORS` | `1` = no motors needed (see below). `0` = real hardware |
| `ROTATE_FEEDBACK360` / `ROTATE_DC_HALL` | Which rotate hardware to build for (default Feedback 360) |
| `FEED_PWM`, `ROTATE_DC_PWM` | Motor speed caps |
| `FEED_TIMEOUT_MS` | No card through the beam within this → stop, show `Jam?` |
| `FEED_RECOIL_MS` | After a card clears the beam, reverse the wheels this long so the next card re-seats |
| `RING_TEETH`, `PINION_TEETH` | Gear ratio — get from mech |
| `SEAT_COUNT` | Fixed stops around the ring (one magnet each for the hall option) |

## Testing with no motors attached

You only need a Uno, the LCD on I2C, and the four buttons.

1. Leave `#define SIMULATE_MOTORS 1` in `config.h` (the default). The motor
   layer then skips the shield entirely and fakes the hardware:
   - the break-beam "sees" a card `SIM_BEAM_BLOCK_AT_MS` after each feed starts;
   - Feedback 360: a fake servo turns at a speed proportional to the command;
   - DC + hall: a fake magnet passes every `SIM_MS_PER_SEAT`.
2. Wire LCD SDA→A4, SCL→A5, VCC→5V, GND→GND. Buttons from D4/D5/D6/D7 to GND.
3. Flash and open the Serial Monitor at 115200 baud.
4. Walk the flow: SELECT → choose Classic → set players and cards (hold UP to
   repeat; try 8 players × 7 cards to see `Too many`) → SELECT → SELECT. The
   LCD counts cards, the serial log prints each card and seat, and it ends on
   `Done!`.
5. Press HOME mid-deal to check abort.
6. To test the jam screen, set `SIM_JAM_EVERY_N` to e.g. `5`: every 5th card
   never reaches the beam, the LCD shows `Jam? card N/M`, and SELECT retries.

## Calibration (once motors are on)

1. Set `SIMULATE_MOTORS 0`, pick the rotate option, and **point the head at
   player 1 before switching on** (there's no home sensor yet).
2. Feed: deal one card at a time and adjust `FEED_PWM`, then `FEED_RECOIL_MS`
   / `FEED_RECOIL_PWM` until the next card stays seated without pulling back
   too far.
3. Feedback 360: if the head runs away and shows `Rotate stuck?`, flip
   `SERVO_REVERSED`. Then set `RING_TEETH`/`PINION_TEETH` and tighten
   `SERVO_TOLERANCE_DEG`.
4. DC + hall: tune `ROTATE_DC_PWM` and `ROTATE_DC_APPROACH_PWM` until the
   head stops consistently on each magnet.

## Known TODOs

- Home sensor so the head finds seat 0 at power-on (see mech-interface.md §3).
- Battery voltage sense on A0 + low-battery lockout.
- Double-feed detection (two overlapping cards look like one long beam pulse).
- Feedback 360 angle read uses `pulseIn` (~2.5 ms blocking while moving);
  move to a pin-change interrupt if it ever matters.
