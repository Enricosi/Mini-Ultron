# Mini-Ultron — Current State

Dynamic project state. Update this file after every meaningful hardware/firmware
session. For stable architecture/rules that don't change session-to-session, see
`../CLAUDE.md` instead.

Status legend: **VERIFIED** (measured/confirmed physically) · **IMPLEMENTED** (code
exists) · **COMPILES** · **UPLOADED** · **PHYSICALLY TESTED** (actually run on real
hardware) · **UNTESTED** · **PLANNED** · **DEFERRED** · **NEEDS VERIFICATION**.

Last reviewed: 2026-09-15 (2026-09-13: ESP32-S3 debug console verified over USB,
then the Pico's first-ever battery-powered physical test found two bugs. 2026-09-14:
neck bug fixed and physically confirmed. 2026-09-15: front wheel connector/wiring
issue resolved by Enrico (hardware-side, no firmware change); M1/M4 then found
spinning inverted on `MOVE:FWD`, fixed via `REVERSE_MOTOR1`/`REVERSE_MOTOR4`,
uploaded and physically confirmed — all 4 wheels verified correct in all directions
for the first time. Same session: ESP32↔Pico UART wired and **verified end-to-end
for the first time** — see "ESP32 ↔ Pico communication boundary" below. Same
session: real movement over that UART link physically confirmed too
(`MOVE:FWD:20` once moved only one wheel — diagnosed as low duty cycle + the
500ms watchdog window being too short for all 4 to overcome stiction, not a bug;
`MOVE:FWD:40` sent twice in a row moved all 4 wheels correctly, matching prior USB
behavior exactly). Also fixed and confirmed same session: the Pico's
`STOPPED:TIMEOUT` watchdog message now reaches the ESP32 over UART too (previously
`Serial`-only). **The ESP32↔Pico link has nothing outstanding** — status,
movement, and safety-timeout visibility are all verified end-to-end for the first
time. Enrico decided next session continues on hardware (INMP441/audio/MPU6050
wiring), not Django/LLM yet — see "Immediate next step").

v1 hardware scope is now decided. **ACTIVE in v1**: SSD1331 OLED, INMP441 I2S mic,
Freenove Audio Converter & Amplifier + 8Ω 2W speaker, MPU6050 (firm inclusion, no
longer optional), UART to the Pico W, neck servo (on the Pico/car shield),
ultrasonic sensor, Mecanum drivetrain. **REMOVED/DEFERRED from v1**: PCA9685, arm
servos, camera.

## Hardware that physically exists

- Freenove ESP32-S3 WROOM board (Freenove ESP32-S3 Ultimate Starter Kit). Previously
  enumerated at `/dev/ttyACM0` — NEEDS VERIFICATION, don't assume current path.
- 0.95" 96x64 full-color SSD1331 SPI OLED — wired and driven. VERIFIED (Enrico has run
  it).
- PCA9685 16-channel PWM driver — **physically REMOVED from the ESP32 system.** Was
  previously wired and driven (VERIFIED historically), but is no longer part of
  active v1 hardware. Arm-gesture code that targets it (`arm_gestures.{h,cpp}`) is
  preserved as deferred/historical reference only, not active v1 functionality (see
  "Arms deferred" below).
- 2x arm servos — **physically REMOVED**, same as above. VERIFIED historically
  working, now out of the system entirely, not just software-deferred.
- Freenove FNK0089 4WD Mecanum car kit: Raspberry Pi Pico W, car shield, 4 DC motors,
  4 Mecanum wheels, front/neck servo mechanism (wired to the shield's own servo
  header, confirmed by Enrico), HC-SR04 ultrasonic sensor, 3-channel line tracker,
  2x18650 battery holder. Mechanically assembled.
- 2x 18650 cells: **NOW AVAILABLE as of 2026-09-13.** Car has run under its own
  battery power for the first time this session — see "Pico W firmware status" for
  results (mixed: rear wheels and neck both move, but both have confirmed bugs).
- INMP441 I2S microphone module: physically acquired, **not wired, not installed**.
- Freenove 8Ω 2W speaker + "Audio Connectors & Amplifier" board: available from kit,
  **not wired, not integrated**. ACTIVE in v1. Amplifier board is powered separately
  via its own VCC/GND (module-rated 3.3V-5V, not yet decided which rail) and drives
  the speaker through one complete channel pair (L+/L- or R+/R-) — the speaker must
  never be wired directly to an ESP32 GPIO.
- Freenove ESP32-S3 camera module: previously test-attached via flex cable.
  REMOVED/DEFERRED from v1 — do not mechanically stress the flex cable.
- MPU6050: available from kit, not integrated. **ACTIVE in v1, firm inclusion (no
  longer optional)** — but its ESP32 I2C pins are not chosen yet; do not assign them
  ahead of need.
- Bench servo power supply: 5V/5A external supply, used historically for PCA9685 arm
  testing (PCA9685/arms since physically removed — see above).

## Repository layout

- `firmware/esp32/hello_ultron/` — main ESP32-S3 sketch. Split into modules this
  session: `hello_ultron.ino` (setup/loop wiring only), `face.{h,cpp}`,
  `pico_link.{h,cpp}` + `pico_link_config.h`, `arm_gestures.{h,cpp}`,
  `debug_console.{h,cpp}`.
- `firmware/esp32/servo_test/` — standalone PCA9685 bench-test sketch (serial
  commands `n/u/l/r/w`). Targets now-removed hardware — historical reference only,
  not runnable against the current ESP32 wiring.
- `firmware/esp32/i2c_scan/` — standalone I2C bus scanner bench-test sketch. Its
  original purpose (finding the PCA9685) no longer applies now that it's removed,
  but the scanner itself is still generically useful for any future I2C device.
- `firmware/pico/pico_ultron/` — main Pico locomotion + neck + safety firmware.
  `neck_controller.{h,cpp}` added this session; `Freenove_4WD_Car_For_Pico_W.{h,cpp}`
  extended with the vendored Servo section; `command_parser.cpp` extended with
  `NECK:*`.
- `freenove-car-reference/` — vendored clone of Freenove's official
  `Freenove_4WD_Car_Kit_for_Raspberry_Pi_Pico` GitHub repo (has its own nested `.git`;
  kept as read-only reference, not part of Mini-Ultron's own history).
- `face_sim.py` — standalone pygame desktop mock-up of the angry-eyes shape. Not
  updated this session; still a static single-frame mock-up, not in sync with the
  now much larger `face.cpp` state set.
- `ultron_env/` — local Python venv, gitignored.

## Pico W firmware status

Location: `firmware/pico/pico_ultron/`. Depends on the third-party `RP2040_PWM`
library (khoih-prog), installed locally via Arduino Library Manager (not bundled in
repo; a reference zip sits under `freenove-car-reference/Libraries/`).

**COMPILES** (`rp2040:rp2040:rpipicow`, last verified 2026-09-15 — 317864 bytes
flash, 15%; 69920 bytes RAM, 26%). **UPLOADED on 2026-09-13 and 2026-09-15** via
`arduino-cli upload`; Claude ran compile+upload directly both times (explicitly
told to for that session, per `CLAUDE.md`'s hardware-actuation rule) — all physical
testing/observation (motors/servo actually moving) remains Enrico's.

**Upload note:** `arduino-cli upload -p /dev/ttyACM0 --fqbn rp2040:rp2040:rpipicow .`
previously required the Pico already in BOOTSEL mode (unplug, hold BOOTSEL, replug,
release) — it used to fail with `Scanning for RP2040 devices` / `No drive to deploy`
without that. On 2026-09-15 a plain upload attempt succeeded without manual BOOTSEL
(`arduino-cli` logged `Resetting /dev/ttyACM0` and found the RPI-RP2 drive itself) —
unclear if this is now reliable or was circumstantial; if a future upload fails with
that error, fall back to manual BOOTSEL as before.

**PHYSICALLY TESTED starting 2026-09-13, under real battery power, continued
2026-09-14, 2026-09-15.** Result: **two bugs fixed (neck, front wheel direction),
one resolved without a firmware change (front wheel connector/wiring):**

1. **Front wheels (M1/M4) not moving — RESOLVED, 2026-09-15, hardware-side (no
   firmware change).** Previously (2026-09-13/14) M1/M4 didn't spin at all under any
   command; the multimeter diagnostic planned to isolate "Pico not driving those
   pins" vs. "past the pins" was never actually needed — as of 2026-09-15 the front
   wheels spin under all 4 physical wheels active. Root cause of the original
   no-spin symptom was never pinned down with the multimeter (Enrico resolved it,
   presumably connector/wiring, before running that test) — not a concern now that
   it's confirmed working, but noted so it isn't mistaken for a diagnosed cause.
   This immediately surfaced a **second, distinct bug**: M1/M4 spin in the wrong
   direction under `MOVE:FWD` (confirmed inverted by Enrico) — same mirrored-wiring
   pattern already seen and fixed on M2/M3 (see "Wheel mapping" section below).
   **Fixed 2026-09-15**
   by enabling `REVERSE_MOTOR1`/`REVERSE_MOTOR4` in
   `Freenove_4WD_Car_For_Pico_W.h`, same vendor-flag mechanism already used for
   M2/M3 — no vendored function body touched. **Uploaded and physically confirmed
   2026-09-15**: Enrico confirmed all 4 wheels spin correctly across
   `MOVE:FWD/BACK/LEFT/RIGHT` and `ROTATE:L/R`. Locomotion direction is now trusted
   for the first time — all four `REVERSE_MOTORx` flags are on and verified, not
   just compiled.
2. **Neck centering — FIXED, 2026-09-14, mechanically.** Confirmed the diagnosis
   from Freenove's manual (see below) was correct: the issue was a mis-seated horn,
   not a firmware angle. Enrico loosened the horn, re-seated it while the servo held
   `NECK:CENTER` (90°, the reverted vendor-default value), retightened — **Enrico
   confirmed the neck now centers correctly.** Firmware stayed at the vendor-clean
   90°/60°/120° (with the LEFT/RIGHT swap from the original mirrored-angles bug,
   which was and remains a real firmware fix). LEFT/RIGHT binding at the mount's
   physical extremes not yet explicitly re-checked, but no longer expected to be an
   issue since the mechanical root cause is resolved.

For context on how the neck bug was actually diagnosed (2026-09-13, before the
above fix): first fix attempt (swapping the LEFT/RIGHT angle constants after Enrico
reported them mirrored) was correct and stays. A second attempt — a blind +10°
software offset on all three positions, after Enrico estimated 90° sat ~10-15° right
of true center — did **not** fix it, and was reverted (back to vendor-clean
90°/60°/120°). Checked Freenove's own assembly manual
(`freenove-car-reference/Mecanum_wheels/Tutorial.pdf`, p.51, "Mounting the Servo Arm
to Module Connector"): the rocker arm/horn is meant to be physically attached to the
servo spline **while the servo actively holds 90°** — that's what makes 90° equal
true mechanical center by construction. That diagnosis is what led directly to the
2026-09-14 mechanical fix above.

Everything else below (protocol, module descriptions) reflects what's IMPLEMENTED
and now confirmed reachable/COMPILES-and-runs, but the two bugs above mean don't
treat this firmware as producing correct physical behavior yet.

### Modules

- `pico_ultron.ino` — entry point. `setup()`: `Motor_Setup()` →
  `motorControllerInit()` (explicit stop) → **`neckControllerInit()`** (new —
  centers the neck servo) → `sensorsInit()` → `safetyInit()` → `commandParserInit()`
  → prints `READY`. `loop()`: polls the command parser over USB `Serial` (9600 baud,
  newline-terminated) and checks `safetyCheckTimeout()` every iteration.
- `command_parser.{h,cpp}` — ASCII line protocol, transport-agnostic (`Stream&`).
  Restructured this session to parse the verb once and dispatch: `NECK:<pos>` (single
  field) alongside the existing `MOVE:<dir>:<speed>` / `ROTATE:<dir>:<speed>`
  (two-field) shapes. Existing MOVE/ROTATE/STOP/STATUS/PING behavior is byte-for-byte
  unchanged. IMPLEMENTED, COMPILES.
- `motor_controller.{h,cpp}` — header comment's wheel mapping corrected 2026-09-13
  (see "Wheel mapping" below). Logic itself (`moveForward/Backward/strafeLeft/Right/
  rotateLeft/Right/moveDiagonal`, all built on `Motor_M_Move(M1,M2,M3,M4)`)
  unchanged.
- `safety_controller.{h,cpp}` — unchanged this session. `COMM_TIMEOUT_MS = 500`,
  `OBSTACLE_STOP_CM = 15`, both still VERIFIED in code.
- `sensors.{h,cpp}` — unchanged this session.
- `neck_controller.{h,cpp}` — Exposes only `neckControllerInit()`,
  `neckSetPosition(NeckPosition)`, `neckGetPosition()`, where `NeckPosition` is
  `NECK_CENTER | NECK_LEFT | NECK_RIGHT`. No raw angle or pulse width is reachable
  from outside this module — `Servo_1_Angle()` (which itself clamps to 30–150°) is
  only ever called with one of three fixed constants. **Current values:**
  `NECK_ANGLE_CENTER = 90°`, `NECK_ANGLE_LEFT = 120°`, `NECK_ANGLE_RIGHT = 60°` (L/R
  swapped from Freenove's demo default — confirmed correct 2026-09-13; a same-day
  +10° center-offset attempt was tried and reverted once the real cause turned out
  to be mechanical, not firmware). Startup position is `NECK_CENTER`. IMPLEMENTED,
  COMPILES, UPLOADED, **PHYSICALLY VERIFIED 2026-09-14** — Enrico re-seated the horn
  while the servo held `NECK:CENTER` and confirmed it now centers correctly.
  LEFT/RIGHT (60°/120°) not yet explicitly re-checked for binding at the mount's
  extremes post-fix, but no longer expected to be an issue.
- `Freenove_4WD_Car_For_Pico_W.{h,cpp}` — vendored/trimmed copy of Freenove's
  official driver. **Servo section added this session**, vendored from the official
  `01.2_Servo` demo: `PIN_SERVO1 = GPIO13`, `Servo_Setup()`, `Servo_1_Angle(float)`.
  Only these two functions were vendored (not the official offset/sweep helpers —
  not needed for a fixed 3-position neck, and `Servo_Sweep` is `delay()`-based, which
  would be inappropriate to pull into this layer). Motor/Ultrasonic/Track sections
  unchanged. Buzzer/battery/LED matrix/WS2812/IR/photosensitive/emotion-board
  sections still deliberately omitted (not on this chassis).

### Protocol v1 — extended this session

Inbound: `MOVE:FWD:<0-100>` `MOVE:BACK:<0-100>` `MOVE:LEFT:<0-100>`
`MOVE:RIGHT:<0-100>` `ROTATE:L:<0-100>` `ROTATE:R:<0-100>` **`NECK:CENTER`
`NECK:LEFT` `NECK:RIGHT`** `STOP` `STATUS` `PING`.

Outbound: `OK` `ERR:PARSE` `OBSTACLE` `DIST:<cm>` `LINE:<3 bits>` (from `STATUS`)
`STOPPED:TIMEOUT`. Also prints `READY` once at boot. No new outbound message types
were needed for neck support — `NECK:*` reuses `OK`/`ERR:PARSE` like every other
command.

`NECK:*` commands call `safetyNoteCommandReceived()` like every other valid command
(keeps the communication-timeout watchdog alive) but are not otherwise
safety-gated — there is no obstacle-style veto concept for the neck, only the fixed
position vocabulary.

### Wheel mapping — VERIFIED 2026-09-13 (corrected from the original inference)

Physically confirmed by Enrico under battery power: `M1=front-left, M2=back-left,
M3=back-right, M4=front-right`. **M3 and M4 were swapped** from the original
inference (which had guessed `M3=front-right, M4=back-right` from pin naming plus
Freenove's demo code). `motor_controller.h`'s header comment updated to match.

Also discovered the same session: `MOVE:FWD`/`MOVE:BACK` were physically inverted on
M2/M3 (the only pair driving at the time, since M1/M4 weren't moving — see the front
wheel bug above). Fixed by enabling `REVERSE_MOTOR2`/`REVERSE_MOTOR3` in
`Freenove_4WD_Car_For_Pico_W.h` (vendor-provided flags, no vendored function body
touched). **This direction fix was uploaded but not explicitly re-confirmed by
Enrico afterward** — verify FWD/BACK direction is actually correct on M2/M3 next
session, don't assume.

**2026-09-15 update:** once the front wheel connector/wiring issue resolved and
M1/M4 started spinning, Enrico confirmed they also run inverted on `MOVE:FWD` —
same mirrored-wiring pattern as M2/M3. `REVERSE_MOTOR1`/`REVERSE_MOTOR4` are now
enabled too (all four `REVERSE_MOTORx` flags on). Compiles clean; **not yet
uploaded or physically confirmed** — see "Immediate next step".

**Cross-checked against Freenove's own manual** (`Tutorial.pdf`, Ch.1 p.46 wiring
diagram + Ch.2 p.59-65 motor theory), 2026-09-13:
- GPIO pin assignments in our vendored `Freenove_4WD_Car_For_Pico_W.h`
  (M1=18/19, M2=20/21, M3=6/7, M4=8/9) match the manual exactly — ruled out as a
  source of any of this session's bugs.
- The manual states directly: "m1_speed, m2_speed represent the two motors on the
  left and m3_speed, m4_speed embody those on the right" — consistent with Enrico's
  physically-verified mapping (M1/M2 = left side, M3/M4 = right side). Re-reading the
  manual's own wiring photo with that grouping in mind, it's actually consistent with
  Enrico's full mapping (including which of each pair is front vs. back) once you
  account for the photo being shot from underneath the (flipped) shield — treat this
  as corroboration of Enrico's physical test, not a separate independent source; the
  physical test remains the authoritative one.
- The manual's own `Motor_M_Move` sign convention for every direction (forward,
  backward, both rotates, both strafes, all 4 diagonals) matches our
  `motor_controller.cpp` byte-for-byte — ruled out as a source of any bug. The bugs
  found this session are wiring/assembly issues (front motor connectors, motor
  polarity), not a math/logic error in this file.
- Useful diagnostic the manual makes possible but we haven't used yet: `Mx_IN1`/
  `Mx_IN2` truth table (`IN1=1,IN2=0` → forward; `IN1=0,IN2=1` → backward). Since
  M1's pins are Pico GPIO18/19 and M4's are GPIO8/9 directly (not behind another
  chip), a multimeter on those two Pico pins while sending `MOVE:FWD` would show
  whether the Pico is actually driving M1/M4's control lines at all — narrows the
  front-wheel fault to "Pico/firmware-side" vs. "past the GPIO pins" (wiring/H-bridge/
  motor) before touching any connector.

## ESP32-S3 firmware status

`hello_ultron.ino` was restructured this session from one large file into five
focused modules, mirroring the Pico's existing module style. **COMPILES**
(`esp32:esp32:esp32s3`, verified this session — 353146 bytes flash, 26%; 23952 bytes
RAM, 7%). Not uploaded this session.

**CRITICAL BUILD FLAG (discovered 2026-09-13):** the plain FQBN `esp32:esp32:esp32s3`
compiles fine but defaults the board option `CDCOnBoot` to **Disabled**. With that
default, `Serial` in the sketch never binds to the native USB port — the debug
console is completely unreachable from the host (writes/reads to `/dev/ttyACM0`
appear to succeed or silently hang at the OS level, with zero relation to which
terminal tool is used; this cost an entire debugging session before being traced to
the build flag rather than to miniterm/stty/arduino-cli monitor). The sketch must be
compiled and uploaded with `CDCOnBoot=cdc` explicit:
```bash
arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc" .
arduino-cli upload -p /dev/ttyACM0 --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc" .
```
If using the Arduino IDE instead: `Tools > USB CDC On Boot > Enabled`. VERIFIED
2026-09-13 — with this flag set, the debug console responded correctly to all
`FACE:*`, `PICO:*`, `NECK:*`, `MOVE:*`, `ARMS:*` commands over USB.

### `face.{h,cpp}` — expressive OLED face (expanded this session)

Nine states (`FaceState` enum), all rendered on the same SSD1331 96x64 display,
software SPI, wiring unchanged (GPIO47/21/2/41/42 — see pin table below):

| State | Visual | Animation |
|---|---|---|
| `FACE_IDLE` | amber/yellow rounded-rect eyes, black pupil cutout | blink + occasional pupil side-glance (new: previously only ANGRY had this) |
| `FACE_ANGRY` | red triangle-pair eyes, black pupil cutout | blink + occasional pupil side-glance (preserved from before) |
| `FACE_LISTENING` | cyan filled circles | static (preserved from before) |
| `FACE_THINKING` | violet rounded rects | brightness pulse every 350ms (preserved from before) |
| `FACE_SPEAKING` | cyan rounded-rect eyes + 6-bar waveform "mouth" | bars re-randomize every 90ms (new) |
| `FACE_SURPRISED` | large white circles, small black pupils | static (new) |
| `FACE_SUSPICIOUS` | thin red squint bars | static (new) |
| `FACE_SLEEPING` | thin dim bars + a small "z" | brightness pulse every 900ms ("breathing") (new) |
| `FACE_GLITCH` | random colored rectangles | fully re-randomized every 80ms (new) |

Only IDLE and ANGRY support blink/look-around (they're the only states with visible
pupils); the rest are self-contained per-state animations. All timing is
`millis()`-based, non-blocking — `faceUpdate(now)` must be called every `loop()`
iteration. `setFace(FaceState)` switches state and redraws immediately; `getFace()`
reads the current state. IMPLEMENTED, COMPILES, **PHYSICALLY TESTED: all 9 states confirmed on real hardware
2026-09-13** via the USB debug console (`FACE:IDLE|ANGRY|LISTENING|THINKING|
SPEAKING|SURPRISED|SUSPICIOUS|SLEEPING|GLITCH`, all responded `OK` and were visually
confirmed on the OLED by Enrico).

### `pico_link.{h,cpp}` + `pico_link_config.h` — ESP32↔Pico transport

- **Transport**: compile-time selectable via `pico_link_config.h`. **2026-09-15:
  hardware UART now active** — `PICO_LINK_USE_HARDWARE_UART` is defined,
  `PICO_LINK_RX_PIN=17`, `PICO_LINK_TX_PIN=18` (ESP32-S3 side, physically wired by
  Enrico: ESP32 GPIO17 <- Pico GPIO0/TX, ESP32 GPIO18 -> Pico GPIO1/RX, grounds
  tied, no shared 5V/3.3V). The old "disconnected" logging fallback
  (`"[picoLink] no UART configured, would send: <line>"`) still exists in the code
  path for `PICO_LINK_USE_HARDWARE_UART` undefined, but is no longer the active
  mode. A `#error` guard prevents enabling hardware UART while pins are still `-1`.
- **High-level API** (the only interface the rest of the firmware should use):
  `picoPing()`, `picoStop()`, `picoStatus()`, `picoMoveForward/Backward/Left/Right
  (int speedPercent)`, `picoRotateLeft/Right(int speedPercent)`, `picoNeckCenter/
  Left/Right()`. Speed is clamped to 0-100 (`constrain`) before being formatted into
  the wire string — no raw/arbitrary command function is exposed.
- **Receive path**: `picoLinkPoll()` (call every `loop()` iteration) parses
  newline-terminated responses from `picoPort` when connected, into
  `picoLastResponseType()` (`PICO_RESP_READY/OK/ERR/OBSTACLE/STOPPED_TIMEOUT/DIST/
  LINE/UNKNOWN`), `picoLastDistanceCm()`, `picoLastLineBits()`, and `picoIsAlive()`
  (true once any response has ever been seen). A new debug command, `PICO:LAST`
  (`debug_console.cpp`), dumps this state over USB Serial for manual verification —
  added specifically to observe the receive path working, since nothing else
  surfaces it.

**IMPLEMENTED, COMPILES, UPLOADED, PHYSICALLY TESTED end-to-end 2026-09-15 — first
time ever.** Required one Pico-side fix to work at all (see "Pico W firmware
status" and "ESP32 ↔ Pico communication boundary" below): `pico_ultron.ino`'s
`loop()` was only polling `Serial` (USB), never `Serial1` (the UART to the ESP32) —
`Serial1.begin(9600)` was never called either. Fixed by adding a second
`CommandParserState` and polling both `Serial` and `Serial1` in `loop()` — the
existing `command_parser.cpp` already worked against any `Stream`, so no protocol
or safety logic changed.

Confirmed round-trip via the ESP32 debug console (`PICO:PING` then `PICO:LAST`
showed `ALIVE=1 TYPE=OK`; `PICO:STATUS` then `PICO:LAST` showed a real
`DIST`/`LINE` reading). **Real movement also confirmed** via `MOVE:FWD:*` from the
ESP32 debug console: a single `MOVE:FWD:20` moved only one wheel (low duty cycle
plus the 500ms watchdog window being too short for all 4 to overcome stiction —
physical, not a link/firmware bug), while `MOVE:FWD:40` sent twice in a row moved
all 4 wheels correctly, matching the USB-bench behavior already verified earlier
this session. The ESP32↔Pico UART link is now proven for both status and
locomotion.

### `arm_gestures.{h,cpp}` — PCA9685/arms deferred+removed, historical reference only

The PCA9685 and both arm servos have been **physically removed** from the ESP32
system as of this update — this is no longer a "likely deferred" design direction,
it is the confirmed v1 hardware state. All PCA9685 arm code
(`handsNeutral/handsUp/leftHandOut/rightHandOut/waveRightHand`, the
`Adafruit_PWMServoDriver` object, `Wire.begin(1, 48)`) lives in this module, gated
behind `#define MINI_ULTRON_ENABLE_ARMS 0`. With the flag at 0 (current and
intended state):
- None of this code is compiled into the active build.
- The main sketch never calls `Wire.begin()` or touches the PCA9685/I2C bus at all.
- The debug console's `ARMS:*` commands respond `ERR:ARMS_DISABLED` instead of
  silently doing nothing.

The code is kept (not deleted) purely as historical reference for how the PCA9685
integration worked, in case it's ever revisited — but with the hardware physically
gone, flipping `MINI_ULTRON_ENABLE_ARMS` back to `1` would compile but has nothing
to drive. `waveRightHand()` still uses blocking `delay(250)` x4, unchanged — not a
concern since this path is historical/inactive, not on the active v1 loop.

### `debug_console.{h,cpp}` — development console (Task 9, new this session)

Replaces the old single-character serial commands with a full ASCII line protocol
over USB Serial (115200 baud, newline-terminated), deliberately mirroring the Pico's
own protocol style:

```
FACE:IDLE|ANGRY|LISTENING|THINKING|SPEAKING|SURPRISED|SUSPICIOUS|SLEEPING|GLITCH
PICO:PING | PICO:STOP | PICO:STATUS
NECK:CENTER | NECK:LEFT | NECK:RIGHT
MOVE:FWD:<0-100> | MOVE:BACK:<0-100> | MOVE:LEFT:<0-100> | MOVE:RIGHT:<0-100>
ROTATE:L:<0-100> | ROTATE:R:<0-100>
ARMS:NEUTRAL|UP|LEFT|RIGHT|WAVE   (ERR:ARMS_DISABLED while arms are deferred)
HELP
```

Every command requires an exact, explicit keyword; anything else (typos, partial
words, unknown verbs) returns `ERR:PARSE` and does nothing — there is no
single-keystroke path that can trigger movement, addressing this session's explicit
safety concern about debug-command typos. `MOVE`/`ROTATE` here only ever *build* a
Pico command via `pico_link` — with no UART connected yet, they cannot physically
move anything even if invoked. IMPLEMENTED, COMPILES.

### `hello_ultron.ino` — now just wiring

```cpp
setup(): Serial.begin(115200) → faceInit() → picoLinkInit() → debugConsoleInit()
         → (armsInit() only if MINI_ULTRON_ENABLE_ARMS)
loop():  debugConsolePoll() → picoLinkPoll() → faceUpdate(now)
```

No blocking calls in `loop()` itself (the only remaining `delay()` in the whole
sketch is inside the deferred, not-compiled-by-default `waveRightHand()`).

## ESP32-S3 pin allocation

| Pin | Function | Status |
|---|---|---|
| GPIO47 | OLED SCL (SPI clock, software SPI) | VERIFIED, in use |
| GPIO21 | OLED SDA/MOSI (software SPI) | VERIFIED, in use |
| GPIO2 | OLED RES/RST | VERIFIED, in use |
| GPIO41 | OLED DC | VERIFIED, in use |
| GPIO42 | OLED CS | VERIFIED, in use |
| GPIO1 | formerly I2C SDA (PCA9685 bus) | PCA9685 physically REMOVED — pin is free, not reserved for PCA9685; not initialized in the active v1 build (`arm_gestures.h`, `MINI_ULTRON_ENABLE_ARMS 0`) |
| GPIO48 | formerly I2C SCL (PCA9685 bus) | same as above — free, not reserved |
| — | PCA9685 CH0/CH1 = arm servos | physically REMOVED from the system; code preserved as historical/deferred reference only |
| — | PCA9685 CH2 | n/a — PCA9685 removed; neck was always on the Freenove shield, not PCA9685 |
| GPIO17 | UART RX from Pico (GPIO0/TX) | VERIFIED, in use — `PICO_LINK_RX_PIN` in `pico_link_config.h`, wired and physically tested 2026-09-15 |
| GPIO18 | UART TX to Pico (GPIO1/RX) | VERIFIED, in use — `PICO_LINK_TX_PIN` in `pico_link_config.h`, wired and physically tested 2026-09-15 |
| TBD | INMP441 I2S (WS/BCLK/SD) | PLANNED — not selected; must avoid GPIO1/2/21/41/42/47/48 and whatever UART/MPU6050 pins get chosen |
| TBD | Speaker/amplifier interface | PLANNED — architecture not decided; amplifier board is separately powered (own VCC/GND, 3.3V-5V) and drives the speaker itself through one full L or R channel pair, so the ESP32 side is a signal/control interface only — never wire the speaker directly to a GPIO |
| TBD | MPU6050 I2C (SDA/SCL) | PLANNED — ACTIVE in v1 (firm inclusion), pins not selected yet; do not assign ahead of need. GPIO1/GPIO48 are free again since the PCA9685's removal, but that does not mean they're pre-claimed for the MPU6050 — choose deliberately once mounting is clear |

Constructor in use (software SPI, matches wiring above, unchanged):
```cpp
Adafruit_SSD1331 display = Adafruit_SSD1331(OLED_CS, OLED_DC, OLED_MOSI, OLED_SCLK, OLED_RST);
```

## ESP32 ↔ Pico communication boundary

- **Transport**: UART, permanent design. Pico side is fixed by hardware: GPIO0 = TX,
  GPIO1 = RX. ESP32-side: GPIO17 = RX, GPIO18 = TX (chosen and wired 2026-09-15,
  see pin table). `pico_link_config.h` is the single place that changed.
- **Status: wired and PHYSICALLY TESTED end-to-end 2026-09-15** (`PICO:PING` and
  `PICO:STATUS` from the ESP32 debug console, confirmed received/parsed correctly
  via the new `PICO:LAST` debug command — see `pico_link` module notes above).
  Required a real fix on the Pico side to work at all: `pico_ultron.ino` wasn't
  polling `Serial1`, only `Serial` (USB) — now polls both. Movement commands over
  this link (not just PING/STATUS) are still untested — see "Immediate next step".
- **Boundary contract**: ESP32 sends the same ASCII line protocol
  (`MOVE:...`, `ROTATE:...`, `NECK:...`, `STOP`, etc.) the Pico's USB bench interface
  already accepts today. The Pico remains the final safety authority regardless of
  transport — it does not trust the ESP32 any more than it trusts a human typing
  into the Serial Monitor. The ESP32's `pico_link` module has no function that can
  emit anything outside this fixed vocabulary, so it cannot be used to smuggle a raw
  PWM/servo/GPIO value to the Pico even if something upstream (a future Django
  layer) tried to.

## Django ↔ firmware boundary

- **Status: NOT STARTED**, unchanged this session. No Django code exists in this
  repository. Everything here remains architectural intent from `CLAUDE.md`.
- The ESP32's new `pico_link` + `face` public APIs (`picoMoveForward(speed)`,
  `picoNeckCenter()`, `setFace(FACE_ANGRY)`, etc.) are the shape a future Django
  integration would call into — bounded, named operations, never raw hardware
  values. No Django-specific code was added; this is just noting that the firmware
  foundation now has a natural seam for it.

## Safety review (this session)

Verified in software (by reading the code, not by physical test):

- Pico still boots stopped: `safetyInit()` → `stopCar()` runs before the command
  parser is live. Unchanged.
- Neck now also initializes to a fixed, safe position (`NECK_CENTER`) at boot,
  before `READY` is printed. This means **powering on/resetting the Pico with this
  firmware will move the neck servo to center once mains/battery power is present**
  — flagging this explicitly since it's a real (if minimal, centering-only) physical
  actuation that will happen automatically the next time this firmware runs on
  powered hardware. Confirm this is acceptable before the next upload+power-on.
- Command timeout (500ms) and its motor-only trigger condition are unchanged.
- Malformed commands (including malformed `NECK:*`) still fall through to
  `ERR:PARSE` and cause no actuation.
- Speed clamping (`constrain(..., 0, 100)` on the ESP32 side in `pico_link.cpp`,
  `constrain(..., 0, MOTOR_SPEED_MAX)` on the Pico side, unchanged) is enforced at
  both layers independently.
- Ultrasonic forward-veto (`safetyForwardBlocked`) is unchanged and still only gates
  `MOVE:FWD`.
- `STOP` is still checked first in `command_parser.cpp`'s `handleLine()`, ahead of
  the generic verb dispatch — unaffected by the `NECK` insertion.
- Neck positions are restricted to the `NeckPosition` enum end-to-end: the wire
  protocol only accepts the literal strings `CENTER`/`LEFT`/`RIGHT`, and
  `neckSetPosition()` only ever calls `Servo_1_Angle()` with one of three constants.
  There is no code path from the protocol to an arbitrary angle.
- The ESP32 cannot bypass the Pico's safety layer: `pico_link.cpp`'s `sendLine()` is
  `static` (file-private) and only reachable through the fixed high-level functions
  in `pico_link.h`. A future Django layer, integrating only against `pico_link.h`
  and `face.h`, has no path to raw PWM/servo/GPIO control either.
- The new debug console requires full explicit keywords and rejects anything else
  with `ERR:PARSE` — no accidental single-keystroke actuation risk (this replaces
  the old single-char scheme, which is a deliberate, justified change from Task 9).

No existing safety behavior was weakened.

## Known problems / open questions

1. ~~Front wheels (M1/M4) don't move at all~~ — **RESOLVED 2026-09-15**,
   hardware-side, no firmware change (root cause not pinned down; the planned
   multimeter diagnostic was never run because it started working before Enrico
   got to it). Surfaced, and then resolved, a second distinct bug: ~~M1/M4 spin in
   the wrong direction on `MOVE:FWD`~~ — **RESOLVED 2026-09-15**, same
   mirrored-wiring pattern as the earlier M2/M3 direction bug, fixed by enabling
   `REVERSE_MOTOR1`/`REVERSE_MOTOR4` (see "Wheel mapping" above), uploaded and
   physically confirmed by Enrico across all of `MOVE:FWD/BACK/LEFT/RIGHT` and
   `ROTATE:L/R`. **Locomotion is now solid for the first time.**
2. ~~Neck doesn't center correctly~~ — **RESOLVED 2026-09-14**, mechanically (horn
   re-seated while servo held 90°). See "Pico W firmware status" above.
3. ~~ESP32 UART pins ... unselected~~ — **RESOLVED 2026-09-15**: GPIO17/18 chosen,
   wired, and the link is physically tested end-to-end (see "ESP32 ↔ Pico
   communication boundary"). INMP441 pins, audio/amp wiring, and MPU6050 I2C pins
   remain unselected — expected, not a bug, blocks mic/speaker/MPU6050 integration.
4. `/dev/ttyACM0`/`/dev/ttyACM1` assignments are not stable across sessions —
   confirmed 2026-09-15 the Pico and ESP32 enumerate as separate `ttyACM*` ports
   when both are plugged in simultaneously, but which is which can change; recheck
   with `arduino-cli board list` every session before uploading or opening a
   monitor, don't assume from a previous session.
5. `moveDiagonal()` in `motor_controller.cpp` still has no protocol command exposing
   it (unchanged, intentional — "kept ready for a future protocol command").
6. `face_sim.py` (the pygame desktop mock-up) is stale — still reflects the old
   single ANGRY frame, not the 9-state system in `face.cpp`.
7. ~~`pico_link`'s receive-path parsing has never run against a real Pico response
   stream~~ — **RESOLVED 2026-09-15**, confirmed working against real `OK`/`DIST`/
   `LINE` responses, and against real movement (`MOVE:FWD:*`) — see `PICO:LAST`
   debug command and "ESP32 ↔ Pico communication boundary".
8. ~~The Pico's `STOPPED:TIMEOUT` watchdog message is only printed to `Serial`
   (USB), never to `Serial1`~~ — **RESOLVED 2026-09-15**. `pico_ultron.ino`'s
   timeout branch now does `Serial.println("STOPPED:TIMEOUT")` AND
   `Serial1.println("STOPPED:TIMEOUT")`. No ESP32-side change needed —
   `pico_link.cpp`'s `handleResponseLine()` and `debug_console.cpp`'s
   `picoResponseTypeName()` already recognized `STOPPED:TIMEOUT` /
   `PICO_RESP_STOPPED_TIMEOUT`, they just never received it over UART before.
   **Uploaded and physically confirmed 2026-09-15**: sent `MOVE:FWD:20`, waited
   >500ms with no further command, `PICO:LAST` showed `TYPE=STOPPED_TIMEOUT`.

## Pending hardware

- ~~ESP32↔Pico UART wiring~~ — **DONE 2026-09-15**, see "ESP32 ↔ Pico communication
  boundary".
- INMP441 wiring (blocks microphone work — still fully PLANNED, no code added this
  session on purpose).
- Speaker/amplifier wiring and final audio architecture decision (blocks voice
  output work — still fully PLANNED). Amplifier board's own power rail (3.3V or 5V,
  module supports either) and which ESP32 pins feed it as signal/control still need
  deciding; speaker itself only ever connects to the amplifier's channel output, not
  to the ESP32.
- MPU6050 wiring and I2C pin selection (blocks accelerometer/gyro integration — ACTIVE
  in v1 now, but pins not chosen; do not assign ahead of need).
- Decision on whether the Freenove shield's 5V rail can safely power the ESP32
  subsystem (not yet measured — do not connect until verified).

## Immediate next step

**As of 2026-09-15, the whole chain is verified for the first time**: neck centers
correctly, all 4 wheels spin with correct direction in every command, and the
ESP32↔Pico UART link is wired and proven end-to-end for both status
(`PICO:PING`/`PICO:STATUS`) and real locomotion (`MOVE:FWD:40` moved all 4 wheels
correctly over the link), and the watchdog's `STOPPED:TIMEOUT` message is
confirmed reaching the ESP32 over UART too. **The ESP32↔Pico link has nothing
outstanding — status, movement, and safety-timeout visibility are all verified.**

**Enrico decided 2026-09-15: next session continues on hardware** (not the
Django/LLM layer yet, even though it's technically unblocked). Concretely, that
means one of INMP441 wiring, audio/amp wiring, or MPU6050 I2C wiring+pin
selection — none is chosen yet, whichever Enrico's physical mounting progress
makes ready first.

Separately (not blocking): the planned LLM/Django layer isn't just conversation —
Enrico confirmed 2026-09-14 the LLM is meant to **control absolutely everything**
(movement, gestures, face, voice — the full intent surface described in `CLAUDE.md`'s
architecture), via a free-tier API, not a local model. Enrico and Claude are doing
all of that programming themselves. No Django code exists yet; this remains stated
direction, not started, and doesn't change `CLAUDE.md`'s existing safety rule that
LLM output is intent only and must pass through validated firmware commands — it
just confirms the LLM is the intended source of *every* high-level behavior, not a
narrow subset of it.
