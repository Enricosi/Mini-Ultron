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
that aren't needed yet. Do not rewrite stable Freenove drivers without a concrete
reason — vendor them, wrap them, don't "clean them up." Commit before risky
integration work. A `delay()` in a tiny isolated bench-test sketch is fine; the actual
robot firmware must not block.

## High-level architecture

```
Django / home-lab  (high-level brain)
        | intent (text/JSON), never raw hardware values
        v
ESP32-S3            (upper body: face, voice, network)
        | UART (link not wired yet — see CURRENT_STATE.md)
        v
Raspberry Pi Pico W (lower body: locomotion + local safety)
        v
Freenove car shield / 4 DC motors / neck servo / ultrasonic / line tracker
```

Responsibilities are separated by controller and must stay that way:

- **Django** owns conversation, personality, persistent memory, LLM provider
  abstraction, STT/TTS orchestration, and validating LLM output into a bounded set of
  high-level intents (e.g. `{"text": "...", "face": "ANGRY", "gesture": "NONE",
  "movement": "NONE"}"`). Django never emits PWM values, servo microseconds, motor
  driver GPIO, or other hardware-shaped data. The LLM output itself must never reach a
  motor or servo directly — it always passes through validation into a known-safe
  command first.
- **ESP32-S3** is the interaction controller: OLED face, animations, future
  mic/speaker, Wi-Fi to Django, and (once wired) sending validated locomotion/neck
  intents to the Pico over UART. It converts abstract intent into deterministic
  hardware behavior (e.g. `FACE:ANGRY`). It does not drive the drive motors.
- **Pico W** is the locomotion / lower-body / local-safety controller. It owns the 4
  DC motors, Mecanum movement, the neck/front servo (physically wired to the Freenove
  shield, so it is the Pico's responsibility even though earlier prototyping drove a
  temporary neck servo from the ESP32's PCA9685 — see CURRENT_STATE.md for the
  present, unresolved state of that), the ultrasonic sensor, the line tracker, and the
  movement watchdog/STOP logic. The Pico knows nothing about personality, LLM,
  conversation, or *why* a move was requested — only whether the move is safe *now*.

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
- The ultrasonic sensor can veto an unsafe forward command regardless of what
  requested it.
- No autonomous behavior may cause harm; no uncontrolled movement after comms loss.
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

- Vendor official Freenove driver code rather than rewriting it. When trimmed for
  Mini-Ultron, say what was removed and why in a header comment (see
  `firmware/pico/pico_ultron/Freenove_4WD_Car_For_Pico_W.h` for the pattern). Don't
  edit vendored function bodies — add Mini-Ultron-specific behavior in the layer above
  them (`motor_controller`, `safety_controller`, `sensors`, etc.).
- Pico command protocol is an ASCII line protocol over any `Stream`, so the same
  parser code serves USB Serial today and `Serial1`/UART to the ESP32 later without a
  rewrite. Don't design a protocol that only works over one transport.
- Don't invent GPIO assignments (ESP32 UART pins, INMP441 pins, audio pins) ahead of
  need. Pins get chosen once the physical mounting makes accessibility clear, and once
  what's already occupied is known. Check `docs/CURRENT_STATE.md`'s pin table before
  assigning anything.
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
  a plausible/derived mapping, not a verified one. See CURRENT_STATE.md.
- The neck servo belongs to the Pico — it is physically wired to the Freenove
  shield's own servo header (confirmed by Enrico), not to the ESP32/PCA9685. Neck
  control must be implemented on the Pico. Don't build new features on top of an
  ESP32-driven neck.
- Arms/PCA9685 gestures are working historical functionality, likely deferred from
  v1. Don't delete them, and don't make new v1 firmware depend on the PCA9685.
- Prefer expressive OLED animation work over adding more physical actuators — the
  face is becoming the primary expressive channel for personality, more so than arms.
- Non-blocking (`millis()`-based) logic is required wherever concurrency actually
  matters (robot main loops). A `delay()` is only acceptable in a small, isolated,
  single-purpose bench-test sketch, never in `pico_ultron.ino` or the real
  `hello_ultron` face/behavior loop going forward.

## Documentation maintenance rule

- After every meaningful hardware/firmware session, update `docs/CURRENT_STATE.md`
  (wiring, GPIOs, tested/untested status, known problems, next step).
- Only touch this file (`CLAUDE.md`) when a stable architectural decision, a permanent
  safety rule, or a long-term convention actually changes — not for day-to-day
  progress.
