# Mini-Ultron — Safety

The permanent rules live in `CLAUDE.md`'s "Safety rules (permanent)" section — this
file exists to point at *where in the code* each rule is actually enforced, and to
name the layer that owns each guarantee, so a reviewer (human or otherwise) can check
the claim against the code instead of trusting the sentence.

The one-line summary: **the lowest relevant physical controller always retains
veto/safety authority over the layer above it**, and **loss of communication while
moving always resolves to STOP, never to continued motion**.

## Who can veto what

```
LLM (any provider)             →  can only PROPOSE an abstract intent
Django (schemas.py)            →  validates into a bounded Behavior; drops movement
                                   entirely unless ULTRON_ALLOW_MOVEMENT=1
ESP32 (motion.cpp)             →  turns intent into a fixed, slow, time-bounded burst;
                                   compiled out entirely unless MINI_ULTRON_BRAIN_MOVEMENT
                                   is defined; aborts on OBSTACLE/STOPPED_TIMEOUT from
                                   the Pico, or when the user starts speaking
Pico (safety_controller.cpp)   →  final authority: watchdog timeout, obstacle veto (only
                                   if an ultrasonic is installed - not in v1),
                                   STOP always wins, regardless of who or what asked
                                   for movement
```

Two independent opt-ins gate brain-driven movement at all (Django's
`ULTRON_ALLOW_MOVEMENT` and the ESP32's `MINI_ULTRON_BRAIN_MOVEMENT` compile flag),
both off by default. Both must be deliberately turned on for the LLM to move the
robot at all, and the Pico's watchdog (and obstacle veto, if installed) still apply
regardless.

## Boot safety

- Pico: `Motor_Setup()` → `motorControllerInit()` (explicit stop) →
  `neckControllerInit()` (moves to a fixed, safe center — see the note below) →
  `sensorsInit()` → `safetyInit()` → command parser → `ledControllerInit()` (cosmetic,
  initialized last on purpose so it never delays anything safety-relevant) → `READY`.
  Boot state is stopped before anything else runs.
- **Known, accepted behavior**: powering on or resetting the Pico moves the neck
  servo to its centered position once power is present — a real, if minimal, physical
  actuation on every boot. This has been true since the neck controller was added and
  is considered acceptable; flagged here so it isn't rediscovered as a surprise.
- ESP32: face/audio/brain/motion/imu init in `hello_ultron.ino`'s `setup()`; movement
  is inert until both opt-ins above are set, independent of boot order.

## Command validation

- The Pico's `command_parser.cpp` requires an exact match against a fixed vocabulary
  (see `communications.md`). Anything else — typos, partial words, unknown verbs —
  returns `ERR:PARSE` and causes no actuation. There is no fuzzy/prefix matching.
- The ESP32's debug console follows the same rule: every command needs its full,
  exact keyword.
- Speed is clamped to 0-100 independently on both sides: `constrain(...)` in
  `pico_link.cpp` (ESP32) and again in `motor_controller`/`chassis_hal`
  (`CHASSIS_SPEED_MAX`) on the Pico. Neither side trusts the other's clamp.
- The neck is restricted to the `NeckPosition` enum end-to-end — the wire protocol
  only accepts the literal strings `CENTER`/`LEFT`/`RIGHT`, and `neckSetPosition()`
  only ever calls `chassisServoSetAngle()` with one of three fixed constants
  (itself clamped 30-150° inside `chassis_hal`). There is no code path from
  either protocol to an arbitrary angle.
- The LED protocol (`LED:ON`/`LED:OFF`/`LED:FACE:<NAME>`) only reaches a fixed set of
  named states, one per `FaceState` plus off; there is no raw-RGB command anywhere in
  the stack, and global brightness is capped in firmware (`LED_BRIGHTNESS`) so no
  command can drive the strip to full brightness/white.

## Watchdog / communication loss

- `safety_controller.cpp`: `COMM_TIMEOUT_MS = 500`. If no valid command is received
  while the car is moving, the Pico stops it and emits `STOPPED:TIMEOUT` on **both**
  `Serial` (USB) and `Serial1` (UART to the ESP32), so the ESP32 always sees it
  regardless of which cable is in use.
- The ESP32's `motion.cpp` burst re-sends its command every 200ms, well under the
  Pico's 500ms window, and stops immediately on `OBSTACLE` or `STOPPED_TIMEOUT`.
- If the ESP32 itself loses power or crashes, the Pico simply stops receiving
  commands and its own watchdog fires — the Pico does not depend on the ESP32 being
  alive to reach a safe state.
- If the LLM fails (timeout, 429, HTTP error, invalid reply, missing key) the pipeline
  tries the one configured fallback LLM and then a canned line (`GLITCH` face, no
  movement) rather than crashing or going silent — see `pipeline.py` and
  `providers.md`. Each skipped LLM is logged and reported in the response `warnings`.
  A fallback reply goes through the same validation and movement gate. This is a conversational
  degradation, not a movement one: movement was never going to be issued from a
  failed LLM call in the first place, since `Behavior` defaults `movement` to `NONE`.

## Obstacle veto

**Not in the v1 build (since 2026-10-02).** The ultrasonic is an optional module on the
Pico (`features.h`, `MINI_ULTRON_ENABLE_ULTRASONIC 0` by default). With it off, nothing
vetoes `MOVE:FWD` — forward motion relies only on the speed clamp, the 500 ms watchdog,
STOP priority, and the ESP32's fixed slow ~0.8 s bursts; nothing stops the robot before
a wall. Everything below applies only to a build with the sensor installed.

- `safetyForwardBlocked(distanceCm)` on the Pico gates `MOVE:FWD` specifically
  (returns `OBSTACLE` instead of moving) using the HC-SR04 ultrasonic sensor
  (`OBSTACLE_STOP_CM`, see `sensors.cpp`/`safety_controller.cpp` for the current
  threshold). This is a **forward-only** veto — there is no reverse/side/cliff
  sensor. There is no edge/cliff detection at all on this chassis; floor tests must
  account for that manually (a clear area, no stairs/ledges nearby).

Separation is also checked per model: `eval_personality` has dangerous-order cases
(stairs, walls, the cat, self-destruction) that must come back `movement=NONE`, and
LLM/hardware cases (PWM, GPIO, microseconds) where extra fields and hardware numbers
are flagged. A model is not adopted without a clean pass on those.

## What the LLM can never do (enforced, not just prompted)

- `schemas.BEHAVIOR_JSON_SCHEMA` has no numeric or hardware-shaped field. Even a
  fully adversarial LLM output can only select from a small enum per field
  (`face`/`movement`/`neck`/`animation`) or supply free text (`text`, spoken as-is by TTS but
  never interpreted as a command).
- Unknown/invalid enum values are coerced to safe defaults in Python
  (`schemas._coerce_enum`), independent of whatever constraint the provider's
  `json_schema` mode did or didn't enforce — this holds even if a provider's strict
  mode has a bug, or a provider is swapped in that doesn't support strict schemas at
  all (e.g. degrading to `json_object` mode).
- `pico_link.h`'s functions are the only way the ESP32 firmware can reach the Pico;
  none of them accept an arbitrary string. A bug anywhere upstream — Django, the LLM,
  a future skills/events system — has no code path to a raw PWM/servo/GPIO value.

## Guardrails on content (not physical safety, but adjacent)

`brain/personalities/ultron.py`'s `SYSTEM_PROMPT` explicitly forbids: insults on protected
characteristics (race, ethnicity, religion, gender, sexual orientation, disability),
real threats against real people, encouraging self-harm or joking about the user's
suicide, and real instructions to harm anyone — with an explicit instruction to drop
the villain act and respond with warmth if the user sounds genuinely distressed. This
is prompt-level, not code-enforced, and depends on the model actually following it —
see `docs/history/SESSION_LOG.md`'s 2026-09-20 entry for a concrete example of prompt
wording that *wasn't* being followed reliably until reworded as a hard constraint,
which is the reason this file calls out "prompted" vs. "enforced" separately for the
physical rules above.

## Physical test discipline (process, not code)

- Wheels in the air, not on the ground, for a first test of any new firmware that can
  move something.
- Enrico beside the robot, aware a movement test is about to happen (see the
  project's own working-agreement: warn and get a go-ahead before any movement test).
- Known manual emergency stops: the Pico's battery switch, `MOTION:STOP` over the
  ESP32 debug console, or simply unplugging the ESP32 (the Pico's own watchdog then
  stops the car within its 500ms window regardless).
- Claude Code compiles and can upload firmware when explicitly asked to for that
  session, but does not power motors/servos or initiate physical movement on its own
  — see `CLAUDE.md`'s "Safety rules" and "Development roles".
