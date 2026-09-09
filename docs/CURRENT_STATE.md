# Mini-Ultron — Current State

Dynamic project state. Update this file after every meaningful hardware/firmware
session. For stable architecture/rules that don't change session-to-session, see
`../CLAUDE.md` instead.

Status legend: **VERIFIED** (measured/confirmed physically) · **IMPLEMENTED** (code
exists) · **COMPILES** · **UPLOADED** · **PHYSICALLY TESTED** (actually run on real
hardware) · **UNTESTED** · **PLANNED** · **DEFERRED** · **NEEDS VERIFICATION**.

Last reviewed: 2026-09-09 (embedded foundation implementation session).

## Hardware that physically exists

- Freenove ESP32-S3 WROOM board (Freenove ESP32-S3 Ultimate Starter Kit). Previously
  enumerated at `/dev/ttyACM0` — NEEDS VERIFICATION, don't assume current path.
- 0.95" 96x64 full-color SSD1331 SPI OLED — wired and driven. VERIFIED (Enrico has run
  it).
- PCA9685 16-channel PWM driver, on the ESP32's I2C bus — wired and driven. VERIFIED.
  Only used by the now-deferred arm gestures (see below); nothing in the active v1
  ESP32 firmware touches it.
- 2x arm servos on PCA9685 channels 0/1 — VERIFIED historically working, but DEFERRED
  from the active v1 build as of this session (see "Arms deferred" below).
- Freenove FNK0089 4WD Mecanum car kit: Raspberry Pi Pico W, car shield, 4 DC motors,
  4 Mecanum wheels, front/neck servo mechanism (wired to the shield's own servo
  header, confirmed by Enrico), HC-SR04 ultrasonic sensor, 3-channel line tracker,
  2x18650 battery holder. Mechanically assembled.
- 2x 18650 cells: **NOT currently available.** Car has never been run under its own
  battery power. No motor or neck-servo movement has ever physically occurred.
- INMP441 I2S microphone module: physically acquired, **not wired, not installed**.
- Freenove 8Ω 2W speaker + "Audio Connectors & Amplifier" board: available from kit,
  **not wired, not integrated**.
- Freenove ESP32-S3 camera module: previously test-attached via flex cable. Deferred
  from v1 — do not mechanically stress the flex cable.
- MPU6050: available from kit, not integrated, optional/deferred.
- Bench servo power supply: 5V/5A external supply, used historically for PCA9685 arm
  testing.

## Repository layout

- `firmware/esp32/hello_ultron/` — main ESP32-S3 sketch. Split into modules this
  session: `hello_ultron.ino` (setup/loop wiring only), `face.{h,cpp}`,
  `pico_link.{h,cpp}` + `pico_link_config.h`, `arm_gestures.{h,cpp}`,
  `debug_console.{h,cpp}`.
- `firmware/esp32/servo_test/` — standalone PCA9685 bench-test sketch (unchanged this
  session, serial commands `n/u/l/r/w`).
- `firmware/esp32/i2c_scan/` — standalone I2C bus scanner bench-test sketch
  (unchanged this session).
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

**COMPILES** (`rp2040:rp2040:rpipicow`, verified this session — 317856 bytes flash,
15%; 69920 bytes RAM, 26%). Previously **UPLOADED** to the real Pico W (per Enrico,
prior session, before this session's changes). **Not re-uploaded this session** — no
upload was performed, per instruction.

**PHYSICALLY TESTED for motor or neck movement: NO.** 18650 batteries are not
available, so no drive current has ever reached the motors, and the neck servo has
never physically moved. Everything below is IMPLEMENTED/COMPILES but UNTESTED on
real hardware.

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
- `motor_controller.{h,cpp}` — unchanged this session.
- `safety_controller.{h,cpp}` — unchanged this session. `COMM_TIMEOUT_MS = 500`,
  `OBSTACLE_STOP_CM = 15`, both still VERIFIED in code.
- `sensors.{h,cpp}` — unchanged this session.
- `neck_controller.{h,cpp}` — **NEW this session.** Exposes only
  `neckControllerInit()`, `neckSetPosition(NeckPosition)`, `neckGetPosition()`, where
  `NeckPosition` is `NECK_CENTER | NECK_LEFT | NECK_RIGHT`. No raw angle or pulse
  width is reachable from outside this module — `Servo_1_Angle()` (which itself
  clamps to 30–150°) is only ever called with one of three fixed constants:
  `NECK_ANGLE_CENTER = 90°`, `NECK_ANGLE_LEFT = 60°`, `NECK_ANGLE_RIGHT = 120°`.
  **NEEDS_PHYSICAL_VERIFICATION**: these three angles are a conservative subset of
  Freenove's own vendor-supported 30–150° range (their own demo sweeps the full
  range on their reference bracket), but Mini-Ultron's neck bracket is Enrico's own
  mechanical installation — confirm on the real hardware that ±30° from center
  doesn't bind or over-stress the mount before trusting these further. Startup
  position is `NECK_CENTER` (safe default). IMPLEMENTED, COMPILES, UNTESTED
  physically.
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

### Wheel mapping — still NEEDS VERIFICATION (unchanged this session)

`motor_controller.h`'s header comment claims `M1=front-left, M2=back-left,
M3=front-right, M4=back-right`. This is still an inference from pin naming plus
direction-logic cross-checking against Freenove's official demo, not a physically
confirmed fact. Not touched this session; still do not treat it as VERIFIED.

## ESP32-S3 firmware status

`hello_ultron.ino` was restructured this session from one large file into five
focused modules, mirroring the Pico's existing module style. **COMPILES**
(`esp32:esp32:esp32s3`, verified this session — 353146 bytes flash, 26%; 23952 bytes
RAM, 7%). Not uploaded this session.

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
reads the current state. IMPLEMENTED, COMPILES, **PHYSICALLY TESTED: only the
original 3 states (IDLE-equivalent behavior didn't exist before) — ANGRY/LISTENING/
THINKING were run on hardware in a prior session; the 6 new states (IDLE as a
distinct state, SPEAKING, SURPRISED, SUSPICIOUS, SLEEPING, GLITCH) are UNTESTED on
real hardware.**

### `pico_link.{h,cpp}` + `pico_link_config.h` — ESP32↔Pico transport (new this session)

Implements Tasks 2/3/11 from this session's brief as one module (deliberately not
split further — the communication layer and its public API are the same
responsibility here):

- **Transport**: compile-time selectable. `pico_link_config.h` defines
  `PICO_LINK_RX_PIN`/`PICO_LINK_TX_PIN` as `-1` (TBD) and leaves
  `PICO_LINK_USE_HARDWARE_UART` commented out. With it undefined (today's state),
  `picoPort` stays `nullptr` and every `pico*()` call logs
  `"[picoLink] no UART configured, would send: <line>"` to the debug Serial console
  instead of transmitting — the code compiles and behaves consistently with no real
  UART pins chosen. A `#error` guard prevents enabling hardware UART while pins are
  still `-1`. Once real ESP32 GPIOs are chosen, uncommenting the macro and setting
  the two pin numbers is the only change needed — no call site changes.
- **High-level API** (the only interface the rest of the firmware should use):
  `picoPing()`, `picoStop()`, `picoStatus()`, `picoMoveForward/Backward/Left/Right
  (int speedPercent)`, `picoRotateLeft/Right(int speedPercent)`, `picoNeckCenter/
  Left/Right()`. Speed is clamped to 0-100 (`constrain`) before being formatted into
  the wire string — no raw/arbitrary command function is exposed.
- **Receive path** (Task 11): `picoLinkPoll()` (call every `loop()` iteration)
  parses newline-terminated responses from `picoPort` when connected, into
  `picoLastResponseType()` (`PICO_RESP_READY/OK/ERR/OBSTACLE/STOPPED_TIMEOUT/DIST/
  LINE/UNKNOWN`), `picoLastDistanceCm()`, `picoLastLineBits()`, and `picoIsAlive()`
  (true once any response has ever been seen).

IMPLEMENTED, COMPILES. **Functionally untestable until UART pins exist and the Pico
is physically wired** — today it can only be exercised through its "disconnected"
logging fallback (verified by inspection/compile only, not by running it against a
live Pico).

### `arm_gestures.{h,cpp}` — arms deferred from v1 (Task 6, new this session)

All PCA9685 arm code (`handsNeutral/handsUp/leftHandOut/rightHandOut/
waveRightHand`, the `Adafruit_PWMServoDriver` object, `Wire.begin(1, 48)`) moved out
of `hello_ultron.ino` into this module, gated behind `#define
MINI_ULTRON_ENABLE_ARMS 0`. With the flag at 0 (today's default):
- None of this code is compiled into the active build.
- The main sketch never calls `Wire.begin()` or touches the PCA9685/I2C bus at all.
- The debug console's `ARMS:*` commands respond `ERR:ARMS_DISABLED` instead of
  silently doing nothing.

Flipping the flag to `1` restores exactly the prior working behavior (nothing was
deleted or rewritten). `waveRightHand()` still uses blocking `delay(250)` x4,
unchanged — acceptable per `CLAUDE.md`'s convention (isolated gesture, and arms are
deferred so it's not on the active path anyway).

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
| GPIO1 | I2C SDA (PCA9685 bus) | VERIFIED wiring, but **not initialized in the active v1 build** (arms deferred — see `arm_gestures.h`); only used if `MINI_ULTRON_ENABLE_ARMS` is flipped back to 1 |
| GPIO48 | I2C SCL (PCA9685 bus) | same as above |
| — | PCA9685 CH0/CH1 = arm servos | wired, VERIFIED historically, DEFERRED from active build |
| — | PCA9685 CH2 | unused — neck is on the Freenove shield, not PCA9685 |
| TBD | UART RX from Pico | PLANNED — `PICO_LINK_RX_PIN` in `pico_link_config.h`, currently `-1`; do not invent |
| TBD | UART TX to Pico | PLANNED — `PICO_LINK_TX_PIN` in `pico_link_config.h`, currently `-1`; do not invent |
| TBD | INMP441 I2S (WS/BCLK/SD) | PLANNED — not selected; must avoid GPIO1/2/21/41/42/47/48 and whatever UART pins get chosen |
| TBD | Speaker/amplifier interface | PLANNED — architecture not decided; do not assume raw GPIO can drive it directly |

Constructor in use (software SPI, matches wiring above, unchanged):
```cpp
Adafruit_SSD1331 display = Adafruit_SSD1331(OLED_CS, OLED_DC, OLED_MOSI, OLED_SCLK, OLED_RST);
```

## ESP32 ↔ Pico communication boundary

- **Transport**: UART, permanent design. Pico side is fixed by hardware: GPIO0 = TX,
  GPIO1 = RX. ESP32-side RX/TX GPIOs are still intentionally unselected (see pin
  table). `pico_link_config.h` is the single place that will change once they're
  chosen.
- **Status: wire protocol and ESP32-side code IMPLEMENTED and COMPILES; physical
  link NOT WIRED.** Until it is, `pico_link` operates in a logging-only fallback
  mode (see above) — this is intentional, not a bug, and requires no further code
  change to "turn on" once wiring exists.
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

1. Wheel physical position mapping (front-left/etc.) is inferred, not verified —
   unchanged from before this session.
2. ESP32 UART pins, INMP441 pins, and audio/amp wiring are all unselected —
   expected, not a bug, but blocks UART bring-up and mic/speaker work.
3. No battery power available yet → zero physical motor or neck-servo testing has
   occurred despite firmware compiling and (for the pre-this-session code)
   previously being uploaded.
4. `/dev/ttyACM0` for the ESP32 is a past observation, not a guarantee for the next
   session — recheck with `ls /dev/tty*` or `arduino-cli board list` before
   uploading.
5. `moveDiagonal()` in `motor_controller.cpp` still has no protocol command exposing
   it (unchanged, intentional — "kept ready for a future protocol command").
6. Neck LEFT/RIGHT angles (60°/120°) are software defaults chosen conservatively
   within Freenove's own vendor-tested range, but are explicitly
   NEEDS_PHYSICAL_VERIFICATION against Enrico's actual bracket/mount — see
   `neck_controller.cpp`.
7. `face_sim.py` (the pygame desktop mock-up) was not updated this session and now
   only reflects the old single ANGRY frame, not the 9-state system in `face.cpp` —
   treat it as stale if reused later.
8. `pico_link`'s receive-path parsing (`picoLinkPoll`) has never run against a real
   Pico response stream — it's logically derived from the Pico's own
   `command_parser.cpp` output format, but only compile-checked, not exercised.

## Pending hardware

- 2x 18650 batteries (blocks all physical drive/neck testing).
- ESP32↔Pico UART wiring (blocks testing `pico_link`'s live transport path; needs
  ESP32 pin selection first, which needs physical mounting to be far enough along).
- INMP441 wiring (blocks microphone work — still fully PLANNED, no code added this
  session on purpose).
- Speaker/amplifier wiring and final audio architecture decision (blocks voice
  output work — still fully PLANNED).
- Decision on whether the Freenove shield's 5V rail can safely power the ESP32
  subsystem (not yet measured — do not connect until verified).

## Immediate next step

The embedded foundation is now at the point described in the session goal: what
remains is mostly physical. The smallest next **physical** test, once Enrico is
ready (not performed by Claude):

1. With the Pico still on USB power/Serial only (no battery pack needed for this
   one), re-upload `pico_ultron` and confirm the neck servo physically centers on
   boot and holds there — this is the very first physical validation of any part of
   this session's work, and it's the lowest-risk one available (no drive current
   involved).
2. From the Arduino Serial Monitor, send `NECK:LEFT` and `NECK:RIGHT` and confirm
   the mount tolerates a full center↔60°↔center↔120° cycle without binding, then
   update `neck_controller.cpp`'s constants/comment from
   NEEDS_PHYSICAL_VERIFICATION to VERIFIED (or narrow them if it doesn't).

Everything else (ESP32 UART pin selection, wiring the ESP32↔Pico link, uploading the
new `hello_ultron.ino`, battery-powered drive testing) should follow only after that
first neck test, per Enrico's own physical-mounting timeline.
