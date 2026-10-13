# Mini-Ultron / CEREBRO-LOCAL — Project Memory

Local theatrical AI robot with a sarcastic/cartoon-villain personality. This file is
stable architectural memory: it should only change when a real architectural decision,
safety rule, or long-term convention changes. For "what actually exists right now"
(GPIOs, firmware status, wiring), see `docs/CURRENT_STATE.md` instead — that file is
updated after every meaningful session; this one is not.

## Philosophy

"feo -> funcional -> ordenado -> bonito." Build the simplest working version first.
Do not turn this into an enterprise framework. Do one concrete feature at a time, test
components independently before integration, prefer simple/working/understood over
clever/abstract/framework-heavy. Do not create manager/service/factory abstractions
that aren't needed yet. Do not rewrite working firmware without a concrete reason. Commit before risky
integration work. A `delay()` in a tiny isolated bench-test sketch is fine; the actual
robot firmware must not block.

## High-level architecture

```
Django / home-lab mini server  (high-level brain: CEREBRO)
        | Wi-Fi, plain HTTP: audio in, JSON intent + audio out; never raw hardware values
        v
ESP32-S3            (upper body: face, voice, network)
        | UART (see docs/hardware.md)
        v
Raspberry Pi Pico W (lower body: locomotion + local safety)
        v
Freenove car shield / 4 DC motors / neck servo / ultrasonic / line tracker
```

Responsibilities are separated by controller and must stay that way:

- **Deployment rule:** the brain (Django, STT, TTS, LLM client) runs on Enrico's home-lab
  mini server, never on the development laptop. The laptop is only for editing/testing and must
  never be a runtime dependency: Mini-Ultron has to work with the laptop off. Keep config in
  environment variables (`cerebro/.env`, gitignored) and secrets out of Git.
- **Django** owns conversation, personality, persistent memory, LLM provider
  abstraction, STT/TTS orchestration, and validating LLM output into a bounded set of
  high-level intents (e.g. `{"text": "...", "face": "ANGRY", "gesture": "NONE",
  "movement": "NONE"}"`). Django never emits PWM values, servo microseconds, motor
  driver GPIO, or other hardware-shaped data. The LLM output itself must never reach a
  motor or servo directly — it always passes through validation into a known-safe
  command first.
- **ESP32-S3** is the interaction controller: OLED face, animations, mic/speaker, Wi-Fi
  to Django (plain HTTP, its blocking work lives in FreeRTOS tasks so the face never
  freezes), and sending validated locomotion/neck intents to the Pico over UART. It
  converts abstract intent into deterministic hardware behavior (e.g. `FACE:ANGRY`),
  matching every name from Django against exact vocabularies and ignoring anything
  unknown. It does not drive the drive motors.
- **Pico W** is the locomotion / lower-body / local-safety controller. It owns the 4
  DC motors, Mecanum movement, the neck/front servo (physically wired to the
  Freenove shield's own servo header — the earlier ESP32/PCA9685 prototyping of the
  neck is fully superseded now, not just deferred; see CURRENT_STATE.md), the
  ultrasonic sensor, the line tracker, and the movement watchdog/STOP logic. The
  Pico knows nothing about personality, LLM, conversation, or *why* a move was
  requested — only whether the move is safe *now*.

The rule that holds this together: **high-level intent → validated command →
deterministic firmware behavior**, and **the lowest relevant physical controller
always retains veto/safety authority** over the layer above it. Communication loss
while moving must always resolve to STOP, never to continued motion.

## Safety rules (permanent)

- Boot state is always STOP — motors are stopped before any other init runs.
- Invalid/unparseable commands must never cause movement.
- Speed is always constrained to a validated range before reaching the motor driver.
- Loss of communication while moving must auto-STOP (watchdog timeout).
- STOP always takes priority over any queued or in-flight movement command.
- When an ultrasonic sensor is installed (Pico `features.h`), it can veto an unsafe
  forward command regardless of what requested it. The v1 build has none, so forward
  motion relies on the speed clamp, the watchdog, STOP priority, and the firmware's
  fixed slow time-bounded bursts — don't lengthen or speed up those bursts to compensate.
- No autonomous behavior may cause harm; no uncontrolled movement after comms loss.
- Movement intents from the brain are opt-in at two independent levels (Django
  `ULTRON_ALLOW_MOVEMENT`, firmware `MINI_ULTRON_BRAIN_MOVEMENT`), both off by default. The
  firmware turns an abstract intent into a fixed, slow, time-bounded burst; the LLM never
  supplies speeds or durations.
- LLM/Django output is intent only. It is never translated directly into raw PWM,
  servo microseconds, or arbitrary GPIO control — it must pass through a firmware
  layer that only knows a fixed vocabulary of safe commands.
- Physical hardware tests (uploading firmware that drives motors/servos, powering the
  chassis, moving anything) are performed manually by Enrico. Claude Code inspects,
  documents, implements, and compiles, but does not initiate physical actuation
  itself unless explicitly told to for that specific session.

## Development roles

- **Enrico**: builds and wires the physical robot, performs all hardware tests, owns
  architecture and design decisions, is professionally invested in Python/Django (not
  interested in becoming a C++ expert — wants to understand behavior, state machines,
  protocols, and GPIO purpose, not syntax minutiae).
- **Claude Code** (this assistant): embedded C++ implementation, inspecting/extending
  existing firmware, compiling, debugging, and keeping this project's documentation
  current. Avoid unnecessary rewrites of working code.
- **ChatGPT** (external, not this tool): architecture discussion, technical reasoning,
  debugging strategy, drafting prompts for Claude, reviewing decisions, Django
  guidance.

## Firmware conventions

- No Freenove example code in the tree: the Pico's hardware access is `chassis_hal`,
  written from scratch (see `THIRD_PARTY_LICENSES.md` for why). Its pins, polarity and
  servo mapping are physically verified — changing them means re-testing on the robot.
  Add Mini-Ultron-specific behavior in the layer above it (`motor_controller`,
  `safety_controller`, `sensors`, etc.). Don't rewrite working firmware out of purism.
- Pico command protocol is an ASCII line protocol over any `Stream`, so the same
  parser code serves USB Serial today and `Serial1`/UART to the ESP32 later without a
  rewrite. Don't design a protocol that only works over one transport.
- Don't invent GPIO assignments (ESP32 UART pins, INMP441 pins, audio pins, MPU6050
  I2C pins) ahead of need. Pins get chosen once the physical mounting makes
  accessibility clear, and once what's already occupied is known. Check
  `docs/hardware.md`'s pin tables before assigning anything.
- When a subsystem's pins aren't chosen yet but its logic can still be written
  (e.g. `firmware/esp32/hello_ultron/pico_link_config.h`), gate the real hardware
  path behind a compile-time flag with the pins left at an obviously-invalid default
  (`-1`), and give the ungated fallback a harmless behavior (log instead of
  transmit) rather than skipping the feature entirely. This lets the calling code
  and its API be written, compiled, and reasoned about before wiring exists, with no
  call-site changes needed once pins are chosen. Use this same pattern for the
  INMP441 and audio output when their turn comes.
- Don't assert a physical wheel position mapping (which motor is physically
  front-left/back-left/etc.) as fact unless it's been physically confirmed on this
  specific chassis — code comments that reference the official Freenove demo describe
  a plausible/derived mapping, not a verified one. See docs/hardware.md.
- The neck servo belongs to the Pico — it is physically wired to the Freenove
  shield's own servo header (confirmed by Enrico), not to the ESP32/PCA9685. Neck
  control must be implemented on the Pico. Don't build new features on top of an
  ESP32-driven neck.
- The PCA9685, both arm servos, and the camera module have been physically removed
  from (or, for the camera, never mounted onto) the ESP32 system and are **not part
  of active v1 hardware**. Don't reserve GPIO1/GPIO48 or any other old PCA9685/camera
  pins, and don't make any current firmware depend on them. The existing
  arm-gesture code (`arm_gestures.{h,cpp}`) is preserved, working, historical
  reference — kept compiled-out (`MINI_ULTRON_ENABLE_ARMS 0`) rather than deleted —
  but treat it as deferred/historical, not active v1 functionality.
- v1 hardware scope is decided: SSD1331 OLED, INMP441 mic, Freenove audio
  converter/amplifier + 8Ω 2W speaker, UART to the Pico, neck servo (on the Pico),
  Mecanum drivetrain. **The MPU6050 and the ultrasonic were dropped from v1
  (2026-10-02)** — the IMU's I2C was corrupted by the I2S audio clocks and produced
  false jolts. Both stay in the tree as optional, off-by-default modules
  (`MINI_ULTRON_ENABLE_IMU` on the ESP32, `MINI_ULTRON_ENABLE_ULTRASONIC` on the Pico),
  so a build that has them can turn them back on; nothing else may depend on them.
- Optional hardware is modular per build: each controller has one `features.h` listing
  its optional subsystems' compile flags. New optional hardware gets a flag there with
  a harmless "not installed" behavior, not scattered inline `#define`s.
- The Freenove audio amplifier board is powered separately via its own VCC/GND
  (module-rated 3.3V-5V), and drives the 8Ω speaker through one complete channel
  pair (L+/L- or R+/R-). Never wire the speaker directly to an ESP32 GPIO — it must
  always go through the amplifier board's speaker output.
- Prefer expressive OLED animation work over adding more physical actuators — the
  face is becoming the primary expressive channel for personality, more so than arms.
- Non-blocking (`millis()`-based) logic is required wherever concurrency actually
  matters (robot main loops). A `delay()` is only acceptable in a small, isolated,
  single-purpose bench-test sketch, never in `pico_ultron.ino` or the real
  `hello_ultron` face/behavior loop going forward.

## Documentation maintenance rule

- After every meaningful hardware/firmware session, update `docs/CURRENT_STATE.md`
  (snapshot: what exists, what is verified, what is deferred and why, next step) and,
  if wiring/GPIOs changed, `docs/hardware.md`.
- Only touch this file (`CLAUDE.md`) when a stable architectural decision, a permanent
  safety rule, or a long-term convention actually changes — not for day-to-day
  progress.
