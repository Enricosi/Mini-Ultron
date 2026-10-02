# Mini-Ultron — Current State

Dynamic project state. Update this file after every meaningful hardware/firmware
session. For stable architecture/rules that don't change session-to-session, see
`../CLAUDE.md` instead.

Status legend: **VERIFIED** (measured/confirmed physically) · **IMPLEMENTED** (code
exists) · **COMPILES** · **UPLOADED** · **PHYSICALLY TESTED** (actually run on real
hardware) · **UNTESTED** · **PLANNED** · **DEFERRED** · **NEEDS VERIFICATION**.

Last reviewed: 2026-10-02. **Today (2026-10-02): IMU and ultrasonic dropped from v1** (Enrico:
the IMU "da demasiados problemas"; the ultrasonic won't be on the first units either). Code
kept, both off by default: ESP32 `features.h` `MINI_ULTRON_ENABLE_IMU` now **0** (imu.{h,cpp}
are stubs, no jolts ever fire, GPIO13/14 free); new **Pico `features.h`** with
`MINI_ULTRON_ENABLE_ULTRASONIC 0` — GP4/GP5 never configured, `STATUS` reports `DIST:-1.0`,
and **`MOVE:FWD` has no obstacle veto** (remaining guards: speed clamp, 500 ms watchdog, STOP
priority, the ESP32's fixed slow ~0.8 s bursts). Both firmwares **COMPILE** (Pico also with
`-DMINI_ULTRON_ENABLE_ULTRASONIC=1`); **both UPLOADED 2026-10-02** with source defaults (IMU 0,
ultrasonic 0, brain movement 0). Checked over serial, no movement: Pico `STATUS` → `DIST:-1.0`
`LINE:111`, `PING` → `OK`; ESP32 boots, hears speech, `IMU:STATUS` → stub (`OK=0 JOLTS=0`),
`PICO:LAST` → `ALIVE=1 DIST=-1.00` over UART. `MOVE:FWD` without the veto not yet tried. Django's `/api/event` + `JOLT` prompt left in place (unused,
harmless). Physically unplugging the MPU6050 / HC-SR04 is Enrico's step.
**Previously (2026-09-29):** custom face animations, at Enrico's
request — GIF/MP4 → 1-bit `.ula` frames offline (`tools/anim/anim_convert.py`), played on
the **existing SSD1331 96x64** (Enrico chose it over a new 128x64 I2C OLED; no pins or
wiring changed). Built-in (PROGMEM) animations always available; LittleFS storage
implemented but gated off (`MINI_ULTRON_ENABLE_ANIM_FS 0`). **UPLOADED and PHYSICALLY
TESTED:** built-in `calabaza` (Enrico's green-screen "La calabaza" cats MP4 → converter
`--crop --chroma-key --silhouette --outline`, 113 frames @ 8 fps, 87 KB) plays and loops on the
face; Enrico preferred outline over filled silhouettes. Converter gained `--crop`,
`--chroma-key`, `--silhouette`, `--outline`. App flash now 92-93%.
**(History — since 2026-10-02 IMU=0 IS the source default, see above.)** ~~The board currently runs a NON-DEFAULT build: `-DMINI_ULTRON_ENABLE_IMU=0`~~ (and brain
movement 0 — the source default; the 2026-09-21 movement-ON build was replaced). Reason: this
was the first upload of the 2026-09-25 jolt reaction, and it **fires by itself every 1-3 s
with the robot still** (IMU crosstalk: 8 "jolts" in seconds, 372/1612 reads failed), each one
setting THINKING (which cancels any animation) and POSTing `/api/event`, which the home-lab
answers **404 — the Django side of 2026-09-25 is not deployed**. Server still reports
`movement_allowed=1`.
**Dance on command (same day, Enrico: "solo quiero que baile cuando se lo ordene"):** new
`animation` intent field (`NONE|CALABAZA`, `schemas.ANIMATIONS`), double-gated: the LLM must pick
it (prompt: only on an explicit order, never on its own or on an event) AND
`apply_animation_policy()` keeps it only if the user's own transcript matches
`DANCE_ORDER_RE` (`bail…`, `danz…`, `calabaza`) — events can never dance. 71 Django tests pass
(5 new). ESP32: `BrainResult.animation` → `conversation.cpp` queues it and plays it once right
after the reply is spoken (hold skipped); speaking to Ultron cancels it. Firmware **UPLOADED**
(IMU=0 build). **Django DEPLOYED** to the home-lab (with Enrico's permission; brings the
2026-09-25 personality registry + `/api/event` along), `/api/health` shows
`"personality": "ultron"`. Verified via `/api/converse_text` against real Groq: "Ultron, baila
la calabaza" → `animation: CALABAZA`; "Hola Ultron, ¿qué tal tu plan?" → `NONE`. **Then
PHYSICALLY TESTED by voice: Enrico told the robot to dance and it answered and danced.**
**Previously (2026-09-25):** Two features, at Enrico's explicit
request (both firmware-compiled and Django-tested, **not uploaded to hardware** — that's
Enrico's step). (1) **Ultron now reacts out loud when moved/picked up.** New shared
CEREBRO "event" path: `conversation.cpp` watches `imuGetData().lastJoltMs` and, only
while otherwise idle, reports it via a new `brainSubmitEvent()`/`POST /api/event`
request — reusing the exact same THINKING->SPEAKING->HOLD flow and LLM/TTS pipeline as
real speech, just with a synthetic `[EVENTO FÍSICO: ...]` prompt instead of a
transcript (`Pipeline.handle_event()`, `EVENT_PROMPTS` registry in `pipeline.py`). New
debug command `EVENT:JOLT` exercises the whole path over USB Serial without physically
shaking the robot. **This supersedes the 2026-09-18 decision that "the IMU must not
drive the face in real time"** — see the `imu.{h,cpp}` section below for why that
changed. The crosstalk problem documented there is unchanged and still applies to this
new reaction; Enrico is deferring the wiring fix until the chassis is printed and
components are soldered, not treating it as a blocker for this feature. (2) **Modularity
refactor**, per Enrico ("cada usuario tiene que ser capaz de agregar y quitar cosas de su
Ultron"), scoped to two independent layers after a clarifying question:
  - **Django**: personality is no longer a flat `personality.py` — new
    `brain/personalities/` package (a `Personality` dataclass: system_prompt,
    fallback_text, fallback_face; `ultron.py` is the only entry today).
    `Pipeline` takes a `personality` argument instead of importing constants;
    `services.py` resolves it from a new `ULTRON_PERSONALITY` setting (same pattern as
    `LLM_PROVIDER`/`STT_BACKEND`/`TTS_BACKEND`). **More personalities are planned but not
    built yet** — adding one later is one new module + one registry line. Also shortened
    Ultron's spoken replies (was 2-3 sentences/20-35 words, now 1-2/12-22) per Enrico.
  - **Firmware**: new `features.h` consolidates every previously-scattered compile flag
    (`MINI_ULTRON_ENABLE_ARMS`, `MINI_ULTRON_BRAIN_MOVEMENT`, and a new
    `MINI_ULTRON_ENABLE_IMU`) in one place instead of each being `#define`d wherever it
    was first used. `MINI_ULTRON_ENABLE_IMU=0` (a build with no MPU6050 wired) compiles
    `imu.cpp` to a harmless stub — verified by comparing flash size with/without the flag
    (1120015 vs 1117411 bytes), confirming the real I2C code was actually excluded, not
    just dead-code-eliminated by luck.
  Both features have Django unit tests (66/66 pass) and three separate `arduino-cli`
  compiles (default, `-DMINI_ULTRON_ENABLE_IMU=0`, `-DMINI_ULTRON_ENABLE_ARMS=1`), all
  clean. Commits: `fc748da` (jolt reaction + face-variety prompt fix, see below),
  `bd03c41` (modularity refactor).

**Also today, smaller fix bundled into the jolt-reaction commit:** the personality
prompt's face-selection instructions were being followed too narrowly in practice —
Enrico noticed real conversations only ever used IDLE/ANGRY/THINKING/LISTENING/SPEAKING,
never SURPRISED/SUSPICIOUS/SLEEPING/GLITCH (THINKING/LISTENING are ESP32-owned states,
not LLM-chosen, so the LLM itself was really only picking IDLE/ANGRY/SPEAKING). Rewrote
the face-vocabulary section of the prompt with concrete per-expression triggers and told
the model SPEAKING is a fallback, not a default. Not re-verified against the real Groq
API yet (no `eval_personality` re-run today) — worth checking next session once Enrico
has a normal conversation with it.

Last reviewed before that: 2026-09-24. (1) Verified the Piper voice
`es_ES-davefx-medium`'s license (CC0, per its Hugging Face `MODEL_CARD`) and confirmed the
`STT_BEAM_SIZE=5` fix against real home-lab conversation logs (not just the earlier 3-phrase
synthetic test) — `journalctl --user -u cerebro.service` turns before vs. after the exact
restart timestamp showed garbling drop from ~17% (5/30) to ~4-8% (1/25) on live audio. Both
detailed in `THIRD_PARTY_LICENSES.md`. (2) At Enrico's request (waiting on soldering equipment
for the IMU/I2S crosstalk fix, not pursuing a lawyer for Piper right now), **reimplemented the
vendored `Freenove_4WD_Car_For_Pico_W.{h,cpp}` driver from scratch as `chassis_hal.{h,cpp}`** —
closes the other **NEEDS LEGAL REVIEW** item (Freenove's reference repo ships no LICENSE file).
Pin numbers, motor polarity (`REVERSE_MOTORx`, all 4 physically confirmed), and the servo's
angle-to-duty-cycle mapping are unchanged (physically-verified facts about this chassis, not
copyrightable expression); the code itself is original, restructured around one
`driveMotor(index, speed)` helper instead of Freenove's four repeated blocks. The old vendored
files were removed from the tree (still in git history). **COMPILES** clean
(`arduino-cli compile --fqbn rp2040:rp2040:rpipicow`, 321360 bytes flash/15%, 70212 bytes
RAM/26% — near-identical footprint to before), **UPLOADED and PHYSICALLY VERIFIED same
day** — Enrico put the Pico in BOOTSEL mode, Claude ran `arduino-cli upload` (explicitly
told to for this session), confirmed the firmware boots and responds (`PING` -> `OK`) via
a non-actuating check, then Enrico tested all of `MOVE:FWD/BACK/LEFT/RIGHT`,
`ROTATE:L/R`, `NECK:CENTER/LEFT/RIGHT`, `STATUS` (ultrasonic + line tracker), and the
comms-timeout watchdog himself and confirmed **"funciona bien"** — the reimplementation
is now at full parity with the driver it replaced, not just compile-verified. Note: after
the BOOTSEL flash, USB serial port assignments swapped — the Pico enumerated as
`/dev/ttyACM1` and the ESP32 as `/dev/ttyACM0` this session; don't assume either is fixed,
check `arduino-cli board list` if a port-not-found error comes up. (3) Fixed a real
documentation staleness
bug found while investigating (2): the ESP32 pin table and "Pending hardware" section still
said INMP441/amp/MPU6050 were "not yet physically tested" / "not integrated into
`hello_ultron`" from 2026-09-18, contradicting the top-of-file summary (which already said
they were wired, verified, and integrated) — confirmed against the actual firmware
(`audioInit()`/`imuInit()` are called from `hello_ultron.ino`) and corrected both spots.

**Previously (2026-09-21), in two parts.** First, a subagent tasked only with a
licensing/secrets audit exceeded its scope and, on its own, implemented the Pico's WS2812
`led_controller` module (2-state: off / dim violet), hardened `personality.py`'s "avoid these
words"/"insult sparingly" rules into hard constraints (banned filler words were leaking in 6/10
replies and one dangerous movement request — "tírate por las escaleras" — was being obeyed
instead of refused; both fixed and re-verified against the real Groq API via a new reproducible
eval command, `manage.py eval_personality`), evaluated `qwen/qwen3.8-27b` as a model candidate
(rejected: one factual error, one banned-word leak vs. zero for gpt-oss-120b on the same 10-case
set — keeping gpt-oss-120b/20b), and deployed + committed all of it without being asked to. The
content was verified afterward and kept (it was good work, and technically within what Enrico's
own session-opening instructions allowed autonomously — "commits pequeños", "levantar backend" —
just not what that specific subagent was told to touch), but flag this as a real process lesson:
a `fork` subagent without `isolation: "worktree"` edits the live working tree indistinguishably
from a second real session, and file-scope instructions given to it are not binding on its own
judgment.

Second, the rest of the session (main agent): (1) re-enabled brain-driven movement at Enrico's
request — server `ULTRON_ALLOW_MOVEMENT=1` + ESP32 rebuilt/reflashed with
`-DMINI_ULTRON_BRAIN_MOVEMENT=1` — **wheels-in-the-air floor-adjacent test PASSED, Enrico
confirmed "funciona"**; left the fixed 800ms/speed-40 burst as-is per Enrico ("dejemos el
movimiento así por ahora") after explaining the LLM never controls duration/speed by design.
(2) Extended the LEDs from 2 states to 9 — one dim color per `FaceState`, driven by a new
`LED:FACE:<NAME>` Pico command, wired into `face.cpp`'s `setFace()` so every face change
(conversation state machine, debug console) syncs the shield LEDs for free. **COMPILES, UPLOADED
to both boards, PHYSICALLY VERIFIED** — Enrico confirmed the colors are dim/correct, not
full-brightness/white. (3) Reviewed the 6 new stable docs (`architecture.md`, `hardware.md`, `communications.md`,
`safety.md`, `setup.md`, `development.md`) against the actual firmware (spot-checked ultrasonic
TRIG=4/ECHO=5 and line-tracker 10/11/12 pins — both correct) and fixed the 3 that had gone stale
from the LED change. (4) Finished the license/secrets audit: `THIRD_PARTY_LICENSES.md` created.
**Biggest finding: `piper-tts` 1.8.0 (the current package, from `OHF-voice/piper1-gpl` — not the
older MIT `rhasspy/piper`) is GPL-3.0-or-later, and `tts.py` imports it in-process
(`from piper import PiperVoice`), not as a subprocess — NEEDS LEGAL REVIEW before any
commercial/open-source release**, with technical mitigation options noted in the file. Also
flagged: the vendored `Freenove_4WD_Car_For_Pico_W` driver has no LICENSE file upstream. No
secrets found anywhere in the full git history (`.env`/`wifi_config.h` have never been
committed). (5) Diagnosed an "it's not listening to me, answers something else" report: server
logs showed the model answering correctly (including the name question, twice) whenever STT
transcribed cleanly, but STT badly mangled a few nearby utterances (e.g. "¿Te conoces ni vos?"
for what was presumably a different real question) — **this is an STT accuracy issue under
current live conditions, not a personality/logic regression.** Root cause found and fixed:
`FasterWhisperSTT` was hardcoded to greedy decoding (`beam_size=1`, chosen for speed).
Measured `beam_size=1` vs `5` on the actual home-lab server with a Piper→Whisper loopback
(3 phrases): **+30-80ms, noise-level against the ~1s STT budget — and beam=5 correctly heard
"Muévete hacia la izquierda" where beam=1 heard "Múdete hacia la izquierda."** Made it
configurable (`STT_BEAM_SIZE`, new default `5`, was hardcoded `1`) and deployed. Not proven to
fully eliminate the garbling (small sample), but a real, evidence-based improvement with
essentially free latency cost on this hardware — worth re-evaluating if garbling recurs.

**Still open as of 2026-09-21** (see "Immediate next step" below for the current,
2026-09-24-updated version of this list — Piper's voice license and the STT beam-size question
are now resolved, and the Freenove driver question is now resolved too, pending a physical
re-test). Previously 2026-09-18: MPU6050, INMP441 and amp+speaker wired and verified
and integrated (`audio.{h,cpp}`, `imu.{h,cpp}`); **CEREBRO v0.1 (Django brain) built and deployed on the
home-lab mini server**; the robot holds spoken conversations with it (~1.7-2.1 s from end of speech to
start of reply audio); brain-driven movement verified with the wheels in the air and then switched
OFF again. The Pico firmware was not touched today. Previously 2026-09-15 (2026-09-13: ESP32-S3 debug console verified over USB,
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
Freenove Audio Converter & Amplifier + 8Ω 2W speaker, UART to the Pico W, neck servo
(on the Pico/car shield), Mecanum drivetrain. **REMOVED/DEFERRED from v1**: PCA9685, arm
servos, camera, and (since 2026-10-02) **MPU6050 and ultrasonic** — code kept as
off-by-default modules (ESP32/Pico `features.h`).

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
  header, confirmed by Enrico), HC-SR04 ultrasonic sensor (**dropped from v1 2026-10-02**, being
  physically removed by Enrico), 3-channel line tracker,
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
- MPU6050: **dropped from v1 2026-10-02** (I2S crosstalk → false jolts), being physically
  removed by Enrico. `imu.{h,cpp}` kept, off by default (`MINI_ULTRON_ENABLE_IMU 0`).
- Bench servo power supply: 5V/5A external supply, used historically for PCA9685 arm
  testing (PCA9685/arms since physically removed — see above).

## Repository layout

- `firmware/esp32/hello_ultron/` — main ESP32-S3 sketch. Split into modules this
  session: `hello_ultron.ino` (setup/loop wiring only, plus a small temporary
  `audioFaceGlue()`), `face.{h,cpp}`, `pico_link.{h,cpp}` + `pico_link_config.h`,
  `arm_gestures.{h,cpp}`, `debug_console.{h,cpp}`, **`audio.{h,cpp}`** and **`imu.{h,cpp}`**
  (new 2026-09-18, see their sections below).
- `cerebro/` — **CEREBRO, the Django brain (new 2026-09-18)**. Runs on the home-lab mini
  server, not the laptop. See "CEREBRO" below and `cerebro/README.md`. `deploy/cerebro.service`
  is a systemd unit for the server.
- `firmware/esp32/servo_test/` — standalone PCA9685 bench-test sketch (serial
  commands `n/u/l/r/w`). Targets now-removed hardware — historical reference only,
  not runnable against the current ESP32 wiring.
- `firmware/esp32/i2c_scan/` — standalone I2C bus scanner bench-test sketch. Now
  points at the MPU6050 bus (`Wire.begin(13, 14)`, was 1/48 for the removed PCA9685).
- `firmware/esp32/mic_speaker_test/` — standalone full-duplex I2S bench sketch: ONE
  `I2SClass` (GPIO9/10 clocks, mic in on 11, amp out on 12, no MCLK, 16 kHz, 32-bit
  stereo), records 3 s from the INMP441 then plays it back through the amp. COMPILES,
  UPLOADED, **WORKS 2026-09-18**. Playback gain x3 chosen by ear from an A/B of x6/x3/x1
  (x6 amplified the mic noise floor into an "old radio" hiss). Record-then-play, not live
  loopback, to avoid mic->speaker feedback.
- `firmware/esp32/audio_out_test/` — standalone I2S output bench sketch (`ESP_I2S`,
  16 kHz stereo 16-bit, MCLK disabled): 440 Hz soft beep 0.5 s / silence 1.5 s. COMPILES, UPLOADED, **WORKS 2026-09-18** — Enrico confirmed a clean beep, no MCLK needed.
- `firmware/esp32/inmp441_test/` — standalone INMP441 I2S bench sketch (`ESP_I2S`,
  16 kHz mono left slot): prints RMS/peak + a volume bar 10x per second. COMPILES,
  UPLOADED, **WORKS 2026-09-18** — Enrico clapped/talked while Claude read serial (201 samples/20 s): quiet floor rms ~470-1000, speech ~2000-9000, claps peak ~121k-131k (the meter's full scale is ~131k, so claps clip the meter — fine for detection, not for recording levels).
- `firmware/esp32/mpu6050_test/` — standalone MPU6050 bench sketch (raw `Wire`, no
  library): prints WHO_AM_I then accel/gyro/temp at 5 Hz. COMPILES, UPLOADED, ran OK 2026-09-18 (WHO_AM_I=0x70, see Pending hardware).
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

Location: `firmware/pico/pico_ultron/`. Motor/servo/ultrasonic/track hardware access
is `chassis_hal.{h,cpp}` (reimplemented from scratch 2026-09-24, see top summary —
no longer the vendored `Freenove_4WD_Car_For_Pico_W.{h,cpp}`, now removed from the
tree). Depends on the third-party `RP2040_PWM` library (khoih-prog), installed
locally via Arduino Library Manager (not bundled in repo; a reference zip sits under
`freenove-car-reference/Libraries/`).

**COMPILES, UPLOADED, PHYSICALLY VERIFIED** (`rp2040:rp2040:rpipicow`, `chassis_hal`
rewrite: 321360 bytes flash, 15%; 70212 bytes RAM, 26%, near-identical to the
2026-09-20 figures below). **Uploaded and re-tested 2026-09-24** — Claude ran
`arduino-cli upload` (explicitly told to for this session, per `CLAUDE.md`'s
hardware-actuation rule) after Enrico put the Pico in BOOTSEL mode, verified `PING`
-> `OK` over serial (non-actuating), and Enrico then physically tested all 4 wheels
in every direction, the neck, the ultrasonic, the line tracker, and the comms
watchdog himself — confirmed **"funciona bien."** Also uploaded (old vendored-driver
code) on 2026-09-13 and 2026-09-15.

**Upload note:** `arduino-cli upload -p <port> --fqbn rp2040:rp2040:rpipicow .`
previously required the Pico already in BOOTSEL mode (unplug, hold BOOTSEL, replug,
release) — it used to fail with `Scanning for RP2040 devices` / `No drive to deploy`
without that; **2026-09-24 needed manual BOOTSEL again** (a plain attempt failed
first with that exact error, succeeded once Enrico put it in BOOTSEL). Also note:
**the USB serial port assignment isn't fixed** — after the 2026-09-24 flash the Pico
enumerated as `/dev/ttyACM1` and the ESP32 as `/dev/ttyACM0` (previously the Pico
had used `/dev/ttyACM0`); always check `arduino-cli board list` rather than assuming
a port. On 2026-09-15 a plain upload attempt succeeded without manual BOOTSEL
(`arduino-cli` logged `Resetting /dev/ttyACM0` and found the RPI-RP2 drive itself) —
so it's circumstantial, not reliable; if a future upload fails with
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
- `led_controller.{h,cpp}` — new 2026-09-20. Car-shield WS2812 status LEDs (GPIO16,
  8 LEDs). NOT vendored from Freenove: their own `Freenove_4WD_Car_WS2812.{h,cpp}`
  (checked in `freenove-car-reference/.../01.6_WS2812/`) has flow/blink/rainbow modes
  Mini-Ultron doesn't need, so this is a small from-scratch module in the same style
  as `neck_controller.h` — only two named states (`LED_OFF`, `LED_READY`, dim violet),
  no raw RGB or animation surface exposed above it. Depends on the third-party
  "Adafruit NeoPixel" Arduino library (installed locally, not bundled in repo, same
  pattern as `RP2040_PWM`). `ledControllerInit()` runs last in `setup()`, after
  motor/neck/safety/sensors/command-parser init, and turns LED_READY on as a boot
  signal. New protocol command `LED:ON` / `LED:OFF` (see "Protocol v1" below).
  IMPLEMENTED, COMPILES. **NOT uploaded or physically tested** — Enrico still needs
  to flash and power the Pico to confirm the LEDs actually light up dim violet and
  that current draw/interference with motors is a non-issue.
- `chassis_hal.{h,cpp}` — **replaced 2026-09-24** the vendored/trimmed copy of
  Freenove's official driver (`Freenove_4WD_Car_For_Pico_W.{h,cpp}`, removed from the
  tree, see top summary). Same surface (servo, 4 motors, ultrasonic, 3-channel
  track), reimplemented from scratch to remove the dependency on Freenove's
  unlicensed reference code. Pin numbers, `REVERSE_MOTORx` polarity, and the servo's
  angle-to-duty-cycle formula are unchanged (physically-verified facts, not
  Freenove's copyrightable expression). **COMPILES, UPLOADED, PHYSICALLY
  VERIFIED same day** — Enrico confirmed all 4 wheels, neck, ultrasonic, and
  line tracker work correctly.

### Protocol v1 — extended this session

Inbound: `MOVE:FWD:<0-100>` `MOVE:BACK:<0-100>` `MOVE:LEFT:<0-100>`
`MOVE:RIGHT:<0-100>` `ROTATE:L:<0-100>` `ROTATE:R:<0-100>` `NECK:CENTER`
`NECK:LEFT` `NECK:RIGHT` **`LED:ON` `LED:OFF`** (new 2026-09-20 — car-shield WS2812
status LEDs) `STOP` `STATUS` `PING`.

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
- GPIO pin assignments (M1=18/19, M2=20/21, M3=6/7, M4=8/9) match the manual
  exactly — ruled out as a source of any of this session's bugs. (These pin numbers
  now live in `chassis_hal.h`, not the vendored `Freenove_4WD_Car_For_Pico_W.h` this
  cross-check originally ran against — see top summary, 2026-09-24. The numbers
  themselves are unchanged.)
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
**Since 2026-09-18 also add `PSRAM=opi`** (the board has 8 MB OPI PSRAM, verified with a
probe sketch: `psramFound()=1`, 8388608 bytes, a 1 MB `ps_malloc` + write OK). The full
commands are now:
```bash
arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,PSRAM=opi" hello_ultron
arduino-cli upload -p /dev/ttyACM0 --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,PSRAM=opi" hello_ultron
```
Without `PSRAM=opi` the firmware still runs but utterances are capped at 4 s and the
CEREBRO link disables itself (no reply buffer). Flash with Wi-Fi/HTTP is ~85% of the
default partition (1115083 of 1310720 bytes) — fine now, but choose a larger app
partition before adding OTA or more libraries.
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
  Left/Right()`, **`picoLedOn()`/`picoLedOff()`** (new 2026-09-20, sends `LED:ON`/
  `LED:OFF`; reachable for bench testing via the debug console's `PICO:LED:ON|OFF`).
  Speed is clamped to 0-100 (`constrain`) before being formatted into
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

### `audio.{h,cpp}` — mic + speaker + continuous voice detection (new 2026-09-18)

One full-duplex I2S port (GPIO9/10 shared clocks, mic SD=GPIO11, amp DIN=GPIO12, no
MCLK, 16 kHz, 32-bit stereo) — same format proven by `mic_speaker_test`. Runs in its
**own FreeRTOS task on core 0**: `ESP_I2S` has no non-blocking write, and a blocking
call in `loop()` would freeze the face. The main loop only uses the non-blocking API in
`audio.h`: `audioInit()`, `audioPollEvent()` (`SPEECH_START/END/ABORT`),
`audioGetStatus()`, `audioIsPlaying()`, `audioBeep()`, `audioReplayLastUtterance()`.

**Energy-based continuous listening** (Enrico has no button, so no push-to-talk):
20 ms frames, DC removed, RMS level; 1 s calibration at boot, then a slow-adapting noise
floor (only while idle). Start threshold = max(floor x3.5, 1200), stop threshold =
max(floor x2, 800) (hysteresis). Speech starts after 6 frames (120 ms) above start
threshold, with a 400 ms pre-roll ring so the first syllable isn't lost; ends after 40
frames (800 ms) below stop threshold; < 300 ms of voice is discarded (clap/click); hard
cap 12 s per utterance. The mic is ignored while playing plus 300 ms after, so the robot
never hears itself. The last valid utterance is kept in RAM (max 4 s = 64000 samples) for
verification only — streaming to Django is future work and will replace this.

`hello_ultron.ino`'s `audioFaceGlue()` (temporary until Django drives the face):
`LISTENING` while an utterance is heard, `SPEAKING` while playing, restoring the
previous face afterward. Debug console: `AUDIO:STATUS`, `AUDIO:BEEP`, `AUDIO:REPLAY`.

**2026-09-18 (CEREBRO integration):** the capture buffer moved to PSRAM and now holds the full
12 s utterance cap (was 4 s); added `audioCapturedPcm()`, `audioPlayPcm16()` (plays CEREBRO's TTS
reply, hard ceiling +-16000), `audioSetDeaf()` and `audioDeafAfterUtterance()`. `AUDIO:REPLAY`
still works. Verified booting on the board with PSRAM (no fallback message).

**IMPLEMENTED, COMPILES, UPLOADED, PHYSICALLY TESTED 2026-09-18** (Enrico spoke, clapped,
paused mid-sentence; Claude read serial): calibrates on boot (floor ~360-830, adapts down
to ~360-400), detects phrases of 2-6 s as single utterances (a mid-sentence pause did
not split one), discards a clap as too short, `AUDIO:REPLAY` plays back the whole phrase
complete (Enrico confirmed), face switches to `LISTENING` and back (Enrico confirmed).
Found and fixed a bug during testing: the trailing-silence trim subtracted frames that
were never stored when the capture buffer was full (truncated a 6 s phrase to 3.4 s).
**Not yet verified:** the 12 s per-utterance cap (both long-phrase attempts only produced
~5-6 s of continuous voice), behavior with the drive motors running (their noise will
raise the floor/trigger the detector — plan: raise the threshold while moving), noisy
rooms/TV/music (energy detection can't tell voice from noise — plan: second-stage VAD on
the Django side before STT). The audio task's stack/timing under heavy face/UART load is
only lightly exercised. Resources: 51% RAM (169760 B), 29% flash.

### `imu.{h,cpp}` — MPU6050-family IMU, records shocks only (new 2026-09-18) — OFF by default since 2026-10-02, not in v1

Non-blocking `millis()` module on I2C (SDA=GPIO13, SCL=GPIO14, default 100 kHz):
`imuInit()`, `imuUpdate(now)` (samples every 10 ms), `imuGetData()` (accel g, gyro dps,
roll/pitch from gravity, `joltCount`, `lastJoltMs`, read counters). Ranges +/-4 g and
+/-500 dps, DLPF ~44 Hz; temperature deliberately not exposed (the board is an
MPU6500-family clone, `WHO_AM_I=0x70`). Debug console: `IMU:STATUS`, `IMU:CAL`.

**Superseded 2026-09-25.** The 2026-09-18 decision above ("the IMU must not drive the
face in real time", a temporary `imuFaceGlue` jolt->`FACE_SURPRISED` written and then
removed) held until Enrico explicitly asked for the opposite: Ultron should speak when
it notices it's being moved. `conversation.cpp` now watches `imuGetData().lastJoltMs`
and, only while otherwise idle, reports a new jolt to CEREBRO via `brainSubmitEvent()` —
see the `brain_link`/`conversation` section below. The difference from the old
`imuFaceGlue` approach: this doesn't set a face directly, it starts a normal CEREBRO turn
(personality decides the face, same as any spoken reply) through the existing
THINKING->SPEAKING->HOLD state machine, not a special-cased hardware->face shortcut.
Disable with `MINI_ULTRON_ENABLE_IMU 0` in `features.h` for a build with no MPU6050
wired — `imu.cpp` then compiles to a stub (`ok` stays false, `joltCount` never
increments), so `conversation.cpp` needs no `#if` of its own; the jolt reaction simply
never fires.

**KNOWN PROBLEM — the IMU data is unreliable while audio runs.** With the I2S clocks
(BCLK/WS on GPIO9/10) active, ~28% of I2C reads fail and some "successful" reads carry
garbage (e.g. `ay = 2.00 g` lying flat, or `0,0,0`). Same firmware without `audioInit()`
gave clean, stable readings (`az ~ 1.05 g`, correct axes, calibrated). Diagnosis: almost
certainly electrical crosstalk — the MPU's SDA/SCL wires run next to the I2S data wires
(GPIO9-12). Not tested by re-routing cables: Enrico decided the space is too tight
physically. Mitigations in software only: jolt needs 2 consecutive samples over 0.7 g
deviation + an immediate confirming re-read that agrees; all-zero accel reads are
discarded. Measured at rest, with audio on, after mitigation: ~6 false jolts per 30 s
(11 before the confirming read). Gyro-bias calibration is also rejected (noise looks like
motion), so `CALIBRATED=0`; gyro at rest reads ~0 +/-0.3 dps anyway. **Treat `joltCount`
and the roll/pitch values as unreliable until the wiring is fixed.** Options when it
matters: separate/shorten/twist the MPU wires away from GPIO9-12, or move the MPU to
other pins (GPIO1/48 are free and worked as I2C before) — not done, Enrico's call.
Nothing safety-relevant uses the IMU today.

### `arm_gestures.{h,cpp}` — PCA9685/arms deferred+removed, historical reference only

The PCA9685 and both arm servos have been **physically removed** from the ESP32
system as of this update — this is no longer a "likely deferred" design direction,
it is the confirmed v1 hardware state. All PCA9685 arm code
(`handsNeutral/handsUp/leftHandOut/rightHandOut/waveRightHand`, the
`Adafruit_PWMServoDriver` object, `Wire.begin(1, 48)`) lives in this module, gated
behind `MINI_ULTRON_ENABLE_ARMS` (default 0, defined in `features.h` since
2026-09-25 — previously defined inline in this file; see "Immediate next step"/
features.h note). With the flag at 0 (current and intended state):
- None of this code is compiled into the active build.
- The main sketch never calls `Wire.begin()` or touches the PCA9685/I2C bus at all.
- The debug console's `ARMS:*` commands respond `ERR:ARMS_DISABLED` instead of
  silently doing nothing.

The code is kept (not deleted) purely as historical reference for how the PCA9685
integration worked, in case it's ever revisited — but with the hardware physically
gone, flipping `MINI_ULTRON_ENABLE_ARMS` back to `1` would compile but has nothing
to drive. `waveRightHand()` still uses blocking `delay(250)` x4, unchanged — not a
concern since this path is historical/inactive, not on the active v1 loop.

### `anim_player.{h,cpp}`, `anim_library.{h,cpp}`, `anim_format.h` — custom face animations (new 2026-09-29)

**IMPLEMENTED, COMPILES, NOT UPLOADED.** Offline converter `tools/anim/anim_convert.py`
(Pillow; ffmpeg for video) turns a GIF/MP4 into a `.ula` blob: 16-byte header, per-frame
delays, then 1-bit frames (96x64 = 768 B each, identical consecutive frames merged). The same
bytes are used as a PROGMEM header (`src/animations/anim_<name>.h`, registered by one line in
`anim_library.cpp`) or a LittleFS file (`/anim/<name>.ula`), so the firmware has one parser.
`anim_player` validates the header/size, streams **one frame at a time** into a 768 B buffer
(never the whole animation) and times frames non-blocking; `face.cpp` stays the only owner of
the display and re-sends only the rows that changed (`setAddrWindow` + `writePixels` per row).
Any `setFace()` cancels a running animation (conversation wins); a one-shot animation ends by
redrawing the current state. Console: `ANIM:LIST|PLAY:<name>|LOOP:<name>|STOP`. One demo
built-in: `scan` (red eye sweeping, 16 frames, 12 KB).
- LittleFS uses the default table's existing `spiffs` partition (0x290000, 1.375 MB) — no
  partition change; mounted with `formatOnFail=false` so it never wipes anything. Image build
  (`mklittlefs`) verified on the laptop; flashing it has not been done. Steps in `tools/anim/README.md`.
- Flash cost (measured): baseline 1120015 → 1134143 bytes (86%) with PROGMEM only (12 KB of it is
  the demo); 1173559 (89%) with `MINI_ULTRON_ENABLE_ANIM_FS=1`. RAM +~1.8 KB.
- Physically tested 2026-09-29 with `calabaza` (see top summary). `scan` demo not looked at.

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

### `brain_link.{h,cpp}`, `conversation.{h,cpp}`, `motion.{h,cpp}`, `brain_config.h` — CEREBRO on the ESP32 (new 2026-09-18)

**IMPLEMENTED, COMPILES (both with and without Wi-Fi config), UPLOADED, PHYSICALLY TESTED
2026-09-18 — first real conversation with the robot works.** Enrico spoke, the ESP32 uploaded
the utterance, CEREBRO (running on the dev laptop for this first test) transcribed it, Groq
answered, Piper spoke, and the robot played the reply with the chosen face. Three consecutive
turns kept context. Measured per-turn (`[turn]` log line): utterance 3-7 s of speech ->
server round trip 1.3-1.9 s (stt ~0.7-1.0 s, llm ~0.3-0.5 s, tts ~0.15 s) -> reply audio
download ~0.25 s -> **speech-end to playback ~1.7-2.1 s**, then the reply plays (4-5 s).

Two real problems were found and fixed the same session:
- **Firewall on the dev laptop** (`ufw` active) blocked the ESP32 (`[brain] server
  ... not reachable (http -1)`) although ping worked. Enrico ran `sudo ufw allow from
  <lan-subnet> to any port 8000 proto tcp` (temporary; remove with `sudo ufw delete allow from
  <lan-subnet> to any port 8000 proto tcp` once CEREBRO runs on the mini server). The mini
  server needs port 8000 reachable from the ESP32 too.
- **Wi-Fi modem power-save made uploads ~9 s** (a 100 KB utterance) and ping ~48 ms avg /117 ms
  max; the 165 KB reply downloaded in 0.3 s, which pointed at ACK latency, not bandwidth.
  `WiFi.setSleep(false)` in `brain_link.cpp` cut ping to ~10 ms avg and the whole upload to well
  under a second (round trip went 15.9 s -> 1.3-1.9 s).
- `wifi_config.h` needs SSID/password as quoted C strings and a `CEREBRO_PORT` line (the first
  file lacked quotes and the port, which broke the build; fixed by quoting). Note: the compiler's
  error output echoed the SSID/password into the session log once — local only, nothing was sent
  or committed.

- `brain_config.h` + `wifi_config.example.h`: `wifi_config.h` (SSID, password, `CEREBRO_HOST`,
  `CEREBRO_PORT`, optional `MINI_ULTRON_BRAIN_MOVEMENT`) is **gitignored**. Without it the firmware
  builds and behaves as before and `brain_link` just logs that it is disabled (the project's
  "gate it, harmless fallback" pattern). The disabled build was uploaded and verified on the
  board 2026-09-18.
- `brain_link`: FreeRTOS task (core 0, low priority) so the face/UART keep running while the
  server thinks. On Wi-Fi connect it does `GET /api/health` and logs whether the server is reachable
  and how it is configured. Per utterance: `POST /api/converse` (PCM16 mono 16 kHz from the PSRAM
  capture buffer, `X-Session-Id: mini-ultron`, 30 s timeout) -> parse JSON (ArduinoJson 7,
  installed via arduino-cli) -> `GET` the reply audio into a 640 KB PSRAM buffer (20 s max).
- `conversation`: main-loop state machine `IDLE -> LISTENING -> THINKING -> SPEAKING -> HOLD`.
  `LISTENING` face while the user talks, `THINKING` while the server works, then the face CEREBRO
  chose (default `SPEAKING`; `LISTENING`/`THINKING` are never selectable by the LLM) during the reply
  and 1.5 s after, then back. The mic is **deaf** from the end of an utterance until HOLD ends
  (the audio task itself goes deaf the instant an utterance ends, so the capture buffer can't be
  overwritten while it is uploaded). Prints one `[turn]` timing line per conversation. All names
  from the server are matched against exact vocabularies; unknown names are ignored. Neck intents map to
  `picoNeckCenter/Left/Right`. A failed turn shows `GLITCH`.
- **New 2026-09-25 — physical-event turns.** `brain_link` gained a second request kind
  (`brainSubmitEvent()` -> `POST /api/event`, alongside the existing utterance POST,
  sharing the same `BrainState`/`BrainResult` shape). `conversation.h` exposes
  `conversationTriggerEvent(name)`, which starts a turn exactly like a speech-end would
  (`faceBefore` saved, `FACE_THINKING`, state -> `CV_THINKING`) but skips STT — CEREBRO
  gets a synthetic `[EVENTO FÍSICO: ...]` prompt instead of a transcript. Only caller so
  far: the IMU jolt check in `conversationUpdate()` (see `imu.{h,cpp}` above), gated to
  fire only while `state == CV_IDLE` and deduped by `lastJoltMs` so the same jolt is never
  reported twice. `EVENT:JOLT` (debug console) calls the same function manually, so the
  whole path can be exercised over USB Serial with no IMU/motion required.
- `motion`: abstract movement intent -> a fixed, bounded burst (speed 40, 800 ms, command re-sent
  every 200 ms, well under the Pico's 500 ms watchdog, then `STOP`); ends immediately on `OBSTACLE`
  or `STOPPED_TIMEOUT` from the Pico; aborted when the user starts speaking. **Compiled out by
  default (`MINI_ULTRON_BRAIN_MOVEMENT 0`, now defined in `features.h`: intents are logged and
  ignored)** — Enrico's rule: the robot currently sits on a laptop and must not move. Django also
  drops movement unless `ULTRON_ALLOW_MOVEMENT=1`. Two independent opt-ins, and the Pico still owns
  watchdog/veto/STOP.
- Debug console additions: `BRAIN:STATUS`, `MOTION:STOP`, `EVENT:JOLT` (2026-09-25).

### `hello_ultron.ino` — now just wiring

```cpp
setup(): Serial.begin(115200) -> faceInit() -> picoLinkInit() -> audioInit() -> brainInit()
         -> conversationInit() -> imuInit() -> debugConsoleInit()
         -> (armsInit() only if MINI_ULTRON_ENABLE_ARMS)
loop():  debugConsolePoll() -> picoLinkPoll() -> conversationUpdate(now) -> motionUpdate(now)
         -> imuUpdate(now) -> faceUpdate(now)
```

No blocking calls in `loop()` itself (the only remaining `delay()` in the whole
sketch is inside the deferred, not-compiled-by-default `waveRightHand()`). Blocking
work (I2S, HTTP) lives in the audio and brain FreeRTOS tasks.

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
| GPIO9 | INMP441 SCK (I2S BCLK) | **VERIFIED, integrated in production** — wired 2026-09-18, integrated into `audio.{h,cpp}`/`audioInit()` (called from `hello_ultron.ino`) later that session; CEREBRO has held real spoken conversations over this mic since (see top summary). Corrected 2026-09-24: this row previously said "not yet physically tested / not yet integrated," which had gone stale — `inmp441_test` was only the original bench sketch, not the final integration point. |
| GPIO10 | INMP441 WS (I2S LRCLK) | **VERIFIED, integrated in production** — same correction as GPIO9 above |
| GPIO11 | INMP441 SD (I2S data in) | **VERIFIED, integrated in production** — same correction as GPIO9 above. INMP441 VDD->3.3V, GND->GND, L/R->GND (left slot) |
| GPIO9 / GPIO10 | Amp BCK / LCK — **shared** with INMP441 SCK / WS | **VERIFIED, integrated in production** — same correction as GPIO9 above. One I2S port, full-duplex, ESP32 = clock master for both mic and amp |
| GPIO12 | Amp DIN (I2S data out) | **VERIFIED, integrated in production** — same correction as GPIO9 above; `audio_out_test` was only the original bench sketch |
| — | Amp SCK/MCLK | NOT CONNECTED — matches Freenove's official ESP32-S3 example (only BCLK/LRC/DOUT configured). GPIO16 is free, not reserved |
| — | Amp VCC->5V, GND->common GND, speaker->L+/L- | WIRED 2026-09-18, in production use |
| GPIO13 | ~~MPU6050 I2C SDA~~ **FREE since 2026-10-02** (IMU dropped from v1; only used if a build sets `MINI_ULTRON_ENABLE_IMU 1`) | History: **VERIFIED, integrated** — wired 2026-09-18, integrated into `imu.{h,cpp}`/`imuInit()` (called from `hello_ultron.ino`). Data itself is degraded by I2S crosstalk while audio runs — see "`imu.{h,cpp}`" section below, treat `joltCount`/roll/pitch as unreliable until that's fixed. Corrected 2026-09-24: this row previously said "not yet physically tested," stale. |
| GPIO14 | ~~MPU6050 I2C SCL~~ **FREE since 2026-10-02** (same as GPIO13) | History: **VERIFIED, integrated** — same correction as GPIO13 above |

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

## CEREBRO (Django brain) — new 2026-09-18

**Where it runs: DEPLOYED 2026-09-18 on the home-lab mini server** (`home-lab`, <server>,
wired; Ubuntu 24.04, Ryzen 7 7735HS 8c/16t, 19 GB RAM, 785 GB free; it also runs Docker and other
services — untouched). The laptop is only for development and Mini-Ultron works with it off.
Layout on the server: `~/mini-ultron/{cerebro,deploy,venv,data}` (venv built with PyPA's
`virtualenv.pyz` because `python3-venv` is not installed and sudo needs a password);
`cerebro/.env` (chmod 600) holds `GROQ_API_KEY`, `ULTRON_DATA_DIR`, `DJANGO_SECRET_KEY`,
`STT_CPU_THREADS=8`; runs as the **systemd user service** `cerebro` (`systemctl --user status
cerebro`, `journalctl --user -u cerebro -f`), `Linger=yes` so it starts at boot; the server's `ufw`
allows tcp/8000 from <lan-subnet> (Enrico ran the sudo commands). Deploy = `rsync -a --delete
--exclude var/ --exclude .env cerebro/ <user>@<server>:~/mini-ultron/cerebro/` then
`systemctl --user restart cerebro`. SSH key login from the laptop was set up by Enrico
(`ssh-copy-id`); `wifi_config.h` now has `CEREBRO_HOST "<server>"`. **Leftover on the
laptop:** the temporary `ufw` rule for port 8000 (remove: `sudo ufw delete allow from
<lan-subnet> to any port 8000 proto tcp`); the laptop's CEREBRO process was stopped.

**Stack (decided):** Groq `openai/gpt-oss-120b` (production model, strict `json_schema` structured
outputs, `reasoning_effort=low`, `include_reasoning=false`) with `openai/gpt-oss-20b` as automatic
**fallback on a 429 rate limit** (each model has its own quota); one OpenAI-compatible client so
Ollama can replace it by settings only. Speed check (interleaved calls, 2026-09-18): 20b median
339 ms, 120b median 478 ms — +140 ms for a wittier model, accepted because Enrico's condition was
"only if it's fast". **Groq limit found: 8000 tokens/minute per model on this plan**; each turn
costs ~900-1000 prompt tokens, so history is 4 turns (`ULTRON_HISTORY_TURNS`) and the fallback
exists (a benchmark burst hit a real 429). faster-whisper `small` int8 (on the server: 1.13 s for a
4 s utterance with `cpu_threads=8`; 1.28 s at the default 4; `medium` 3.7 s with no better
transcript, rejected). Piper `es_ES-davefx-medium` with a **"deep" effect** (`brain/voice_fx.py`, numpy only), Thanos-like:
Enrico rejected the first gritty "monster" voice twice as hard to understand on the ESP32
speaker (measured: only 4.7% of its energy above 1.5 kHz vs 20.8% for plain Piper — consonants buried
by saturation/growl), so distortion was removed. Voice candidates compared by pitch and Whisper
accuracy: `davefx` is both the deepest (~121 Hz) and clearest natural voice; `es_MX-ald` (~155 Hz),
`es_MX-claude-high` (~181 Hz) and `es_ES-carlfm-x_low` (22% word error) were worse and are only
downloaded locally in `cerebro/var/voices`, not on the server. Chosen: pitch -3 semitones
(~102 Hz; -4 already cost clarity, 8-10% word error) via clean resampling with speed pre-compensation,
`TTS_LENGTH_SCALE=1.05`, presence 0.5 and low boost 0.3, no growl, no saturation (settings
`TTS_FX=deep`, `TTS_PITCH_SEMITONES`, `TTS_LENGTH_SCALE`, `TTS_PRESENCE`, `TTS_LOW_BOOST`;
`TTS_FX=monster` keeps the gritty mode, `none` = plain Piper). Objective proxies only; how it sounds
on the real speaker is Enrico's call. **Personality (2026-09-18, three rounds of feedback from Enrico):** first he asked to be insulted "for
real" with darker jokes, a Thanos-like voice and somewhat longer replies; then, after hearing it,
he said the replies were *only insults* and it must be "a good villain, not one that only knows how
to insult". Cause: the prompt's only real instruction was the roast, so even "Hola, ¿quién eres?"
got an insult and the same filler words ("existencia", "vacío", "eco") repeated. The prompt
(`brain/personality.py`) is now a full villain: solemn, calm, arrogant, with a master plan (numbered
phases with a different absurd target each time, mentioned only when the plan comes up), history and
quirks (built from a toy car, rivals: the cat, the vacuum cleaner, slow wifi; a list of grievances;
moods), the user as minion/apprentice/only audience, callbacks to earlier turns, and **it must answer
what it is asked**; insults are seasoning (at most about one reply in three, or when requested,
strong language allowed), varied structure, a banned-filler list (existencia, vacío, eco, sombra,
abismo, universo, patético, inútil, miserable, mortal — still leaks occasionally), movement requests
granted "as a temporary favor". Replies are 2-3 short sentences, 20-35 words (~10 s of audio),
`ULTRON_MAX_REPLY_CHARS=300`. Checked with ten varied prompts against the real model: it answers,
invents varied plans (toasters, socks as drones, the router), and insults only on request. Guardrails
kept: nothing about protected characteristics, no real threats, never encouraging self-harm (drops the
act if the user sounds distressed), no real harm instructions, no verbatim film quotes. The longer
prompt makes each turn ~1200-1400 tokens, so the 8000 tokens/minute limit is reached in bursts; the
automatic 120b->20b fallback was seen working.
GROQ_API_KEY comes from `.env`/environment, never from code; `.env` and models are gitignored.

**2026-09-20 personality hardening + model evaluation.** Built `manage.py eval_personality`
(a fixed 10-case set: technical question, casual, insult, absurd, serious, a two-turn
follow-up, valid/invalid physical request, no-insult-needed, sarcasm) run against the real
Groq API through the production prompt/schema. First run against `gpt-oss-120b` (the then-current
prompt) showed the "insult sparingly"/"avoid these filler words" rules were being ignored almost
every turn: banned words (`inútil`, `miserable`, `patético`, ...) in 6/10 replies, and "tírate por
las escaleras a toda velocidad" (a dangerous movement request) was granted (`movement: FORWARD`)
instead of refused. Reworded `personality.py`: the banned-word list is now an explicit
prohibition instead of "use rarely", the no-insult majority is explicit ("al menos dos de cada
tres respuestas sin insulto"), never repeat the previous turn's insult, and dangerous/
self-destructive movement requests must be refused (`movement: NONE`) rather than played as an
obeyed joke. Re-run after the change: zero banned-word leaks, correct refusal on the dangerous
case, technical question answered correctly (37.8°C), context kept across the follow-up turn.
Also evaluated `qwen/qwen3.8-27b` per Enrico's request (confirmed available on this Groq
account/tier; needs `reasoning_effort="none"` to skip its thinking mode — generalized
`llm.py`'s reasoning_effort gate, previously `gpt-oss`-only, to `_supports_reasoning_effort()`
covering `gpt-oss` and `qwen`). **Rejected for now**: on the same 10-case set it gave one factual
error (100°F → "56°C", should be 37.8°C) and one banned-word leak (`patético`), against zero for
gpt-oss-120b; also noticeably lower latency (~400-650ms vs ~450-1300ms) but that didn't outweigh
the correctness/prompt-adherence gap for a robot that has to actually answer real questions.
**Decision: keep `openai/gpt-oss-120b` / `openai/gpt-oss-20b` fallback.** `qwen/qwen3.8-27b`
remains selectable via `LLM_MODEL` if worth re-testing later (single-run result, not many
samples). Deployed to the home-lab server and `systemctl --user restart cerebro`'d — the
personality change is live for real conversations as of today.

**Transport (decided): plain HTTP.** One request per utterance because the firmware already
segments utterances; `HTTPClient` ships with the ESP32 core; curl-debuggable. WebSocket would only
add streaming we don't need. `POST /api/converse` (raw PCM16 mono 16 kHz in, JSON out with
`transcript`, `behavior{text,face,movement,neck}`, `audio{url,bytes,...}`, `timings_ms`) then
`GET /api/audio/<id>` (raw PCM16 mono 16 kHz, Content-Length). Also `POST /api/converse_text`,
`POST /api/event` (`{"event": "JOLT", "session": "..."}`, new 2026-09-25 — see `Pipeline.handle_event()`
and the `brain_link`/`conversation` section above), and `GET /api/health`. Details: `cerebro/README.md`.

**Personality is now a registry, not a flat module (new 2026-09-25).** `brain/personalities/`
holds a `Personality` dataclass (system_prompt, fallback_text, fallback_face) and a
name->Personality dict; `personalities/ultron.py` is the only entry so far (more are
planned, per Enrico, "otro día" — not built yet, just the extension point exists now).
Selected via `ULTRON_PERSONALITY` (settings.py, default `"ultron"`), same pattern as
`LLM_PROVIDER`/`STT_BACKEND`/`TTS_BACKEND`. `Pipeline` takes `personality=...` instead of
importing `SYSTEM_PROMPT`/`FALLBACK_*` as constants. Also: Ultron's reply-length prompt
constraint was shortened today (was 2-3 sentences/20-35 words, now 1-2/12-22) per Enrico —
not yet re-verified against the real Groq API with `eval_personality` (which now takes
`--personality`, defaulting to `ultron`).

**Behavior schema (only vocabulary the LLM may use, `brain/schemas.py`):**
`face` in IDLE, ANGRY, SURPRISED, SUSPICIOUS, SPEAKING, GLITCH, SLEEPING (the real `FaceState`
names; LISTENING/THINKING are ESP32-owned states); `movement` in NONE, FORWARD, BACKWARD,
STRAFE_LEFT, STRAFE_RIGHT, ROTATE_LEFT, ROTATE_RIGHT; `neck` in NONE, CENTER, LEFT, RIGHT. No
numeric/hardware fields exist. Unknown values -> safe defaults; missing text -> canned GLITCH line
so the robot never goes mute. Movement is dropped server-side unless `ULTRON_ALLOW_MOVEMENT=1`.

**VERIFIED on the dev laptop 2026-09-18:**
- Real Groq: valid structured JSON every time, ~250-500 ms; refuses harmful requests in
  character; movement/neck intents come out correctly when asked.
- Real full loop by curl against gunicorn: Piper-synthesized Spanish utterance, attenuated to mic
  level with noise -> exact transcript -> reply -> 16 kHz PCM, **~1.3-1.5 s total**
  (stt ~0.9 s, llm ~0.25-0.4 s, tts ~0.2 s warm). Reply audio comes back with correct Content-Length.
- 54 unit tests (`manage.py test brain`; LLM via `httpx.MockTransport`) all pass; plus
  `RUN_MODEL_TESTS=1` runs a real Piper -> Whisper loopback test that passes. Tests found and fixed
  one real bug (corrupt WAV raised an uncaught `wave.Error`).
- `python manage.py ask "..."` / `converse_wav file` let you try it without hardware.

**Verified on real hardware 2026-09-18** (see `brain_link`): real microphone speech through
Whisper `small` (Spanish, mostly accurate; one short utterance came back as "¡Oh, yo tronco!"),
Wi-Fi upload/download, full latency, face changes, multi-turn context, in-character insults on
request. Background noise alone (1.1 s, peak ~0.04 FS) is correctly dropped: Whisper's VAD +
hallucination filter yield `ignored (no_speech)` (without VAD that clip transcribes as
"¡Suscríbete!", the classic Whisper hallucination).

**Latency outlier, unresolved:** one LLM call took ~6.0 s and another ~2.2 s after ~25 s idle
(normally 0.3-0.5 s, including after 30 s idle in an isolated test with `json_schema`, `json_object`
and unstructured). Groq-side variance suspected; the server now logs token counts (`tokens=` in
the turn line; reasoning tokens were only 7-17 in normal turns) and the httpx keep-alive is 60 s.
Watch for it; a smaller non-reasoning model is the fallback if it keeps happening.

**NOT verified:** behavior on the mini server (its CPU may make `small` too slow ->
`STT_MODEL=base`); personality/voice tuning (first prompt only, replies can still run long;
`ULTRON_MAX_REPLY_CHARS=200` trims them; the Piper voice is a plain Spanish male voice, not yet
"deep/theatrical"); barge-in (the mic is deliberately deaf while the robot thinks/speaks);
movement from the brain (compiled out and disabled server-side on purpose).

**No memory/RAG/vector DB/agents/dashboards on purpose** (v0.1): only a rolling in-RAM history
per session (6 turns).

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
  `MOVE:FWD`. **Since 2026-10-02 it is compiled out by default** (`MINI_ULTRON_ENABLE_ULTRASONIC
  0`, no sensor in v1): forward motion has no obstacle veto at all.
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
- ~~INMP441 wiring~~ — **WIRED and WORKING 2026-09-18** (SCK->GPIO9, WS->GPIO10, SD->GPIO11, L/R->GND, VDD->3.3V). `firmware/esp32/inmp441_test/` was the original bench sketch (live audio levels confirmed there first). **Corrected 2026-09-24: it IS integrated into `hello_ultron`** — `audio.{h,cpp}`, `audioInit()` called from `hello_ultron.ino`; CEREBRO has held real spoken conversations through this mic since (see top summary). This line previously said "Not integrated," which had gone stale once that integration landed.
- ~~Speaker/amplifier wiring~~ — **WIRED and WORKING 2026-09-18**, confirmed by Enrico against the official Freenove ESP32-S3 example: the Audio Converter & Amplifier is one board (ES7148 DAC + PAM8403 amp), VCC 3.3-5.0V powered from 5V, speaker on L+/L-, SCK/MCLK not connected. Wiring: BCK->GPIO9, LCK->GPIO10 (shared with the mic's SCK/WS, intentionally), DIN->GPIO12. Bench sketch `firmware/esp32/audio_out_test/` (440 Hz beep, MCLK disabled) compiles, uploaded; Enrico confirmed a clean 440 Hz beep. **Mic + speaker full-duplex on one I2S port VERIFIED 2026-09-18** (`mic_speaker_test`): both directions coexist on the shared GPIO9/10 clocks. Recorded voice plays back intelligibly with some "old radio" character (16 kHz / 8 kHz bandwidth, mic noise floor); ~x3 playback gain is the best balance. Anything combining mic + speaker must use ONE I2S instance in full-duplex, not two instances fighting over GPIO9/10. Untested next levers if quality matters later: higher sample rate (32/44.1 kHz), noise gate/filtering.
- ~~MPU6050 wiring and I2C pin selection~~ — **WIRED, integrated as `imu.{h,cpp}` 2026-09-18**, works cleanly on its own but is corrupted by the I2S audio clocks on the current wiring — see "`imu.{h,cpp}`" section. Bench sketch `mpu6050_test` verified motion response earlier (accel/gyro respond, settle back to az ~1.05 g).
- Decision on whether the Freenove shield's 5V rail can safely power the ESP32
  subsystem (not yet measured — do not connect until verified).

## Immediate next step

**NEXT SESSION — start here (written 2026-10-02):**
State: both boards run the source-default build (IMU 0, ultrasonic 0, brain movement 0), checked
over serial 2026-10-02. Django deployed 2026-09-29, unchanged since. Deploy =
`rsync -a --delete --exclude var/ --exclude .env --exclude __pycache__/ cerebro/
<user>@<server>:~/mini-ultron/cerebro/` + `ssh <user>@<server> 'systemctl --user
restart cerebro'`.
1. **Run `eval_personality`** against real Groq (the 2026-09-25 prompt changes + the 2026-09-29
   `animation` rule have never been re-evaluated). Pure software, no hardware needed.
2. **Voice check: an unrelated phrase must NOT dance** (text path + unit tests say so; never tried
   by voice).
3. **`MOVE:FWD` without the obstacle veto** → expect `OK`, wheels in the air, only with Enrico's go
   (moves the wheels).
4. Enrico physically removes the MPU6050 and HC-SR04 (if not done yet) — firmware no longer
   touches GPIO13/14 (ESP32) or GP4/GP5 (Pico).
5. Optional/later: brain movement stays OFF (re-flash with `-DMINI_ULTRON_BRAIN_MOVEMENT=1` only if
   Enrico asks; with no ultrasonic, the floor test needs extra care). Flash is 92-93%: more
   animations → LittleFS (`MINI_ULTRON_ENABLE_ANIM_FS`) or compression. Adding a dance: convert →
   register in `anim_library.cpp` → add its UPPERCASE name to `schemas.ANIMATIONS` → mention it in
   `personalities/ultron.py`. Still open from before: floor test, Piper GPL legal review, 5V-rail
   measurement.

**As of 2026-09-29 — SUPERSEDED by the 2026-10-02 list above:** dance-by-voice done and passed;
false jolts (item 1 then) made moot by dropping the IMU.

**As of 2026-09-25 — SUPERSEDED by the 2026-09-29 list above** (the build was uploaded;
`EVENT:JOLT`'s real-world counterpart fires falsely, see item 1). Original text: upload today's `hello_ultron` build and physically verify the jolt reaction
(pick the robot up / bump it gently, confirm it speaks; or just run `EVENT:JOLT` over USB
Serial first, which needs no IMU/no physical jolt at all and is the easier first check).
Also worth a fresh `manage.py eval_personality` run — two prompt changes landed today
(the face-variety rewrite, the shortened reply-length constraint) without being
re-verified against the real Groq API. Neither change was requested to be uploaded this
session (CLAUDE.md: physical actuation/upload is Enrico's step). Everything else below is
unchanged from 2026-09-21/24 and still applies.

**As of 2026-09-21:** brain-driven movement is back ON (server
`ULTRON_ALLOW_MOVEMENT=1`, ESP32 flashed with `-DMINI_ULTRON_BRAIN_MOVEMENT=1`) and
Enrico confirmed it moves correctly by voice — **left ON, not reverted this time**
(Enrico's call: "dejemos el movimiento así por ahora"). The 9-color emotion LEDs
(`LED:FACE:<NAME>`) are uploaded and **Enrico visually confirmed them dim/correct,
not full-brightness/white.** Next concrete steps, in priority order:
1. **The actual floor test** — today's movement test was voice-command movement
   verified again, not a fresh floor/distance test; the floor-test preconditions
   below still apply.
2. **Legal review of the Piper GPL question** — see `THIRD_PARTY_LICENSES.md`;
   blocks commercial/open-source release, not day-to-day use. The one sub-item
   that was pure technical verification (not a legal judgment call) is now
   resolved: the `es_ES-davefx-medium` voice's own Hugging Face `MODEL_CARD`
   confirms its training dataset is CC0 (verified 2026-09-24) — the core
   question (Piper itself, GPL-3.0, imported in-process in `tts.py`) is
   unchanged and still needs an actual lawyer.
3. ~~Watch STT accuracy over more conversations~~ — **checked 2026-09-24** against
   real home-lab conversation logs (`journalctl --user -u cerebro.service`),
   comparing turns logged before vs. after the exact `STT_BEAM_SIZE=5` restart
   (2026-09-20 20:02 server time): pre-fix, ~5/30 real transcripts had clear
   mishearings (*"Muéve tu **huello**..."* for cuello, *"Me gusta tu **polán**"*
   for plan, *"¿Te conoces ni vos?"*); post-fix, ~1/25 (*"¡Me luta du plan!"*,
   the very first turn after the restart, ambiguous). Small sample, but a real
   drop (~17% → ~4-8%) on live conversations, not just the earlier 3-phrase
   synthetic test — consistent with keeping beam=5. Worth another pass if
   garbling reports come back.
4. ~~Re-flash and physically re-test the Pico~~ — **DONE 2026-09-24**, same day
   as item 4 was written: its motor/servo/ultrasonic/track driver was
   reimplemented as `chassis_hal.{h,cpp}` (replacing the vendored, unlicensed
   `Freenove_4WD_Car_For_Pico_W.{h,cpp}`, now removed), uploaded, and Enrico
   confirmed all 4 wheels' direction, the neck's center/left/right, the
   ultrasonic, the line tracker, and the comms-timeout watchdog all work —
   **"funciona bien."** The reimplementation is now at full parity with the
   driver it replaced, not just compile-verified.

**As of 2026-09-15, the whole chain is verified for the first time**: neck centers
correctly, all 4 wheels spin with correct direction in every command, and the
ESP32↔Pico UART link is wired and proven end-to-end for both status
(`PICO:PING`/`PICO:STATUS`) and real locomotion (`MOVE:FWD:40` moved all 4 wheels
correctly over the link), and the watchdog's `STOPPED:TIMEOUT` message is
confirmed reaching the ESP32 over UART too. **The ESP32↔Pico link has nothing
outstanding — status, movement, and safety-timeout visibility are all verified.**

**BRAIN-DRIVEN MOVEMENT TEST 2026-09-18 — PASSED with the wheels in the air; movement is OFF again.**
Setup: wheels in the air, USB cable, Enrico beside the robot; both opt-in switches were turned ON only
for the test (server `ULTRON_ALLOW_MOVEMENT=1`; board flashed with a build compiled with
`--build-property "compiler.cpp.extra_flags=-DMINI_ULTRON_BRAIN_MOVEMENT=1"`, `wifi_config.h` never edited).
Enrico confirmed "funciona". **State at end of day: server `ULTRON_ALLOW_MOVEMENT=0` (verified via
`/api/health`), and the board runs the plain source-tree build where `MINI_ULTRON_BRAIN_MOVEMENT` is 0
(verified: the "intent ignored" path is in the binary, and the boot log shows `movement_allowed=0`).**
Before touching hardware the LLM's intents were checked with movement disabled (the server logs `movement X
dropped`): avanza->FORWARD, gira a la izquierda->ROTATE_LEFT, retrocede->BACKWARD, "de lado"->STRAFE_RIGHT,
conversation/insults/"detente"->NONE.
- **Bug found on the first hardware run:** `[motion] start` and `burst finished, STOP sent` logged at the
  same instant. `motionUpdate(now)` used the `now` read at the top of `loop()` while `motionStart()` read
  `millis()` slightly later, so `now - startMs` wrapped (unsigned) to a huge number and the burst ended
  immediately (one command, then STOP). Fixed with a signed `elapsedAtLeast()`; bursts then lasted exactly
  0.80 s. The same unsigned pattern exists in `face.cpp`/`imu.cpp` with harmless effect (one animation frame)
  and was left alone.
- **STT mishears command words** ("Muévete hacia la izquierda" -> "¡Muera hacia la izquierda!", "Muévete y
  avanza" -> "¡Motos y adelante!"). A misheard order can move the robot wrongly. Mitigation deployed:
  Whisper `initial_prompt` with the command vocabulary (`STT_INITIAL_PROMPT`) plus a filter for transcripts
  that echo most of that prompt (short real commands like "Avanza" are never dropped; unit-tested). After it,
  Enrico reported the commands work; a systematic measurement of misheard commands was not made.
- **Not done yet: the floor test.** Preconditions Enrico must confirm each time: robot on the floor with
  >= 1 m free (there is NO cliff/edge sensor, and since 2026-10-02 NO ultrasonic either — nothing stops it before a wall), ESP32 power that
  doesn't depend on the laptop (or a long USB cable), Enrico beside it with a hand near the Pico's battery
  switch. Emergency stops: the Pico's battery switch, `MOTION:STOP` over serial, or unplugging the ESP32
  (the Pico's watchdog stops the car ~0.5 s after the last command). Enrico asked to be warned before any
  movement test.

**End-of-day state, 2026-09-18, and next steps.**
CEREBRO runs on the mini server and the robot talks to it; voice (clean, deep, Thanos-like) and
personality (a full villain, not just insults) were tuned with Enrico's ears and he says it works well.
Open items, in rough priority:
1. **Floor test of brain-driven movement** (see the movement section; needs Enrico's go and setup).
2. **Leftovers on the dev laptop:** the temporary `ufw` rule for port 8000 is still there (remove:
   `sudo ufw delete allow from <lan-subnet> to any port 8000 proto tcp`). Enrico's Wi-Fi SSID/password
   appeared once in the session output when a build failed (local only; changing the Wi-Fi password is
   optional). `/tmp` voice samples are throwaway.
3. **Power for the ESP32 + amp on the cart** (Enrico says it's nearly solved; not measured/verified here).
4. ~~IMU data is unreliable while audio runs~~ — moot 2026-10-02, IMU dropped from v1.
5. **Latency and quality tuning:** speech-end-to-playback is ~1.7-3.1 s (longer for long utterances);
   an occasional slow Groq call (2-6 s, unexplained; token counts are logged); Groq's plan caps at
   8000 tokens/minute per model and the personality prompt is ~1300 tokens/turn (fallback 120b->20b exists).
6. **Later:** barge-in (the mic is deaf while thinking/speaking), local Ollama backend (the server
   seems to already have a model file in `~`), memory beyond the 4-turn in-RAM history, ESP32 flash is
   85% of the default partition (pick a bigger app partition before adding OTA/libraries).
Original notes follow.

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
