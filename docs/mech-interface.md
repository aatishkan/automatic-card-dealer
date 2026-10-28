# What electrical needs from mech

A checklist for the mech subteam. Each item says what we need, why, and the
value the firmware currently assumes (in `config.h`) so you can see what
changes if your number is different. Fill in the **Mech answer** column and
ping electrical when something changes.

## 1. Motor shafts and mounts

| Item | What we need | Current assumption | Mech answer |
|---|---|---|---|
| Feed motor | Motor model, shaft diameter + flat (D-shaft?) so the wheel axle coupler / wheel bore fits | N20 or TT-class 6 V gearmotor (N20: Ø3 mm D-shaft; TT: ~5.4 mm double-flat) | |
| Feed wheels | Wheel diameter + durometer, axle diameter, how the axle couples to the motor | Two rubber wheels on one axle | |
| Rotate — Feedback 360 option | Pinion mount to the servo's spline (standard servo horn pattern — **confirm against the Parallax datasheet**), servo mounting tabs on the head | Parallax #900-00360 | |
| Rotate — DC option | Gearmotor model, shaft size, pinion bore | DC gearmotor on M2 | |
| Gear ratio | **Ring gear tooth count and pinion tooth count** | `RING_TEETH = 120`, `PINION_TEETH = 12` | |
| Head rotation speed target | Max comfortable head speed (deg/s) so cards don't fly | ~0.7 s per seat move | |
| Motor wiring access | Path for two leads per motor to the shield terminals, ≥ 22 AWG | | |

## 2. Break-beam at the slot exit

| Item | What we need | Current assumption | Mech answer |
|---|---|---|---|
| Sensor | Mount for a 3 mm or 5 mm IR break-beam pair (emitter + receiver) | Adafruit-style 3 mm pair | |
| **Orientation** | Beam must cross the card path **perpendicular to the card face** (emitter above, receiver below). A beam in the plane of the card only sees a 0.3 mm edge and won't trigger reliably. | | |
| Position | Just past the slot exit, centered across the card width, and **after** the point where the card leaves the feed wheels (so the recoil can't pull the dealt card back) | | |
| Distance wheels → beam | Needed to tune `FEED_TIMEOUT_MS` and the sim timings | | |
| Light shielding | A hood so room light / sunlight doesn't reach the receiver | | |
| Alignment | Emitter/receiver aligned within ~±1 mm; ≤ 25 mm apart | | |

## 3. Hall sensor + magnets (`ROTATE_DC_HALL` option)

> The sketch says "hall sensor on the base". Since nothing electrical crosses
> the rotating joint, we need it the other way round: **sensor on the head,
> magnets on the stationary base**. Please confirm.

| Item | What we need | Current assumption | Mech answer |
|---|---|---|---|
| Magnet count + spacing | One per seat, evenly spaced | `SEAT_COUNT = 8` (every 45°). 2–8 players are spread over these. | |
| Magnet radius | All magnets on one circle; sensor on the head at the same radius | | |
| Air gap | Sensor face to magnet face | ≤ 3 mm for a Ø6–10 mm neodymium disc | |
| Polarity | Same pole facing up on every magnet (A3144-type sensors only trigger on one pole — usually **south**) | | |
| Angular offset | Firmware stops on the *leading edge* of each magnet (always approaching from the same side). Shift the magnets so that point lines the slot up with the seat. | | |
| Home marker (future) | Room for a **second** magnet track at a different radius with one magnet at seat 0, plus a second sensor on the head. Lets us find home at power-on instead of "point it at player 1 before switching on". Useful for the Feedback 360 option too. | Not implemented | |

## 4. Battery bay and electronics on the head

| Item | What we need | Current assumption | Mech answer |
|---|---|---|---|
| Battery | Bay for a 2S1P 18650 pack + holder: ≈ **77 × 41 × 21 mm** (side-by-side holder) | 2 × 18650, ~95 g | |
| BMS | Space for the 2S BMS board ≈ 45 × 16 × 4 mm, next to the pack | | |
| Balance | The pack is the heaviest part — place it to balance the head about the rotation axis | | |
| Access | Charge port and power switch reachable without disassembly | | |
| Arduino + shield | Uno footprint 68.6 × 53.4 mm on its 4 mounting holes; ~30 mm stack height with the shield; USB-B port reachable for flashing | | |
| 6 V BEC (servo option) | ≈ 25 × 15 × 8 mm | | |
| LCD | 16×2 module ≈ 80 × 36 mm board, viewable window ≈ 64.5 × 14.5 mm, + ~12 mm depth for the I2C backpack | | |
| Buttons | Four 12 mm tactile or panel-mount buttons (UP, DOWN, SELECT, HOME) | | |
| Ventilation | The Uno regulator dissipates ~0.25 W; don't seal it in foam | | |
