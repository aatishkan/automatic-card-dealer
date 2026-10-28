# Power budget

> **These are estimates, not measurements.** They're typical datasheet /
> hobby-part numbers so we can size the battery now. Once we have parts,
> measure each row with a USB/bench meter and update this table.

## Supply architecture

```
2S Li-ion (6.0–8.4 V, 7.4 V nominal) ── BMS ── switch ──┬── Uno VIN → onboard 5 V linear regulator → logic
                                                        ├── Motor Shield EXT PWR (VIN jumper removed) → M1 / M2
                                                        └── 6 V BEC → Feedback 360 servo (if chosen)
```

The Uno's regulator is linear, so the battery supplies the same current the
5 V side draws; the rest of the voltage is burned as heat (~0.25 W at 100 mA
from 7.4 V — fine without a heatsink).

## Current draw per component

| Component | Supply | Running (est.) | Peak / stall (est.) | Duty during game night |
|---|---|---|---|---|
| Arduino Uno R3 (328P + USB chip + LEDs + regulator) | VIN | 50 mA | 50 mA | 100% |
| 16×2 LCD + backlight + PCF8574 backpack | 5 V | 25 mA | 30 mA | 100% |
| IR break-beam (emitter + receiver) | 5 V | 15 mA | 15 mA | 100% |
| Motor Shield v2 logic (PCA9685 + TB6612 Vcc) | 5 V | 10 mA | 10 mA | 100% |
| Hall sensor (A3144-type) — `ROTATE_DC_HALL` | 5 V | 5 mA | 5 mA | 100% |
| Buttons (35 kΩ internal pull-ups) | 5 V | ~0 | 0.15 mA each while held | ~0% |
| **Logic subtotal** | | **≈ 105 mA** | | |
| Feed motor, N20/TT 6 V class @ PWM 170/255 | EXT PWR | 150–250 mA | 1.0–1.5 A stall | ~0.5 s per card |
| Rotate DC gearmotor @ PWM 150/255 — `ROTATE_DC_HALL` | EXT PWR | 200–300 mA | ~1.5 A stall | ~0.7 s per seat move |
| Parallax Feedback 360 — `ROTATE_FEEDBACK360` | 6 V BEC | 150–250 mA moving, low idle | ~1–2 A stall (**confirm from datasheet**) | ~0.7 s per seat move |

Notes:
- The TB6612 driver on the shield is rated 1.2 A continuous / 3 A peak per
  channel. Stall current is near that limit — which is one more reason the
  2 s feed timeout and the per-seat rotate timeout exist.
- Motors never run at the same time: the dealer rotates, *then* feeds.
  Peak battery current is therefore one motor's stall + logic, ≈ 1.6 A.

## 6-hour battery sizing

Assumptions (deliberately pessimistic):
- 6 hours on, logic powered the whole time.
- 12 full-deck deals per hour (one every 5 min) — more than any real game.
- Worst-case deal: 52 cards, 52 seat moves.

| Item | Calculation | Per hour |
|---|---|---|
| Logic | 105 mA × 1 h | 105 mAh |
| Feed | 52 cards × 0.5 s × 0.25 A = 1.8 mAh/deal × 12 | 22 mAh |
| Rotate | 52 moves × 0.7 s × 0.3 A = 3.0 mAh/deal × 12 | 36 mAh |
| **Total** | | **≈ 165 mAh/h** |

6 hours × 165 mAh ≈ **1.0 Ah consumed.**

Margins:
- × 1.25 for estimate error and things we haven't counted (ramps, retries).
- ÷ 0.8 usable capacity: the Uno's regulator needs VIN ≳ 6.5 V (≈ 3.25 V/cell),
  so we can't use the bottom of the discharge curve.
- ÷ 0.8 for cell aging and a cold room.

Required rated capacity ≈ 1.0 × 1.25 / 0.8 / 0.8 ≈ **1.95 Ah**.

**Recommendation:** 2S1P of good 18650s, **≥ 3000 mAh** each (e.g. genuine
Samsung/LG/Molicel — avoid no-name "9900 mAh" cells). Estimated runtime:
3000 × 0.8 × 0.8 / 165 / 1.25 ≈ **9 hours**, i.e. ~50% headroom over 6 h.

BMS: 2S, **≥ 5 A overcurrent trip** with ≥ 3 A continuous. A BMS that trips
on a motor stall cuts *everything*, which looks exactly like a brownout.

## Brownout mitigations

What can go wrong: a motor start or stall pulls the battery voltage down,
VIN dips below what the 5 V regulator needs, and the Uno resets mid-deal.

1. **Soft-start ramps** (`MOTOR_RAMP_MS`, 0 → target over ~100 ms). A DC motor
   at rest draws stall current until it spins up; ramping cuts that spike.
   The Feedback 360 gets the same treatment via a ramped speed cap.
2. **One motor at a time.** `Dealer` never rotates and feeds at once, and the
   feed brakes for `FEED_BRAKE_GAP_MS` before reversing for the recoil
   (reversing straight from forward is ~2× stall current).
3. **Separate motor power path.** Shield VIN jumper removed; motor current
   goes battery → shield terminal, not through the Uno or its regulator.
4. **Servo on its own BEC**, never on the Uno 5 V pin (the onboard regulator
   is ~1 A max and would brown out on the first servo stall).
5. **Bulk capacitance:** 470–1000 µF across the shield's EXT PWR, plus
   0.1 µF ceramic across each DC motor.
6. **Timeouts** (`FEED_TIMEOUT_MS`, `ROTATE_TIMEOUT_PER_SEAT_MS`) so a jam
   can't hold a motor at stall current.
7. **PWM caps** (`FEED_PWM`, `ROTATE_DC_PWM`) keep 6 V motors from seeing a
   full 8.4 V — less current, less heat, longer motor life.
8. **Battery sense (TODO):** divider on A0, refuse to start a deal below
   ~6.8 V and show "Charge me". Until then, charge between game nights.
9. The ATmega328P's brown-out detector (Uno default fuse: 2.7 V) resets the
   chip cleanly instead of letting it run corrupted — leave it enabled.
10. Low-ESR, reasonably thick (≥ 22 AWG) battery and motor wiring; every
    10 mΩ of wiring is 15 mV of sag at 1.5 A.
