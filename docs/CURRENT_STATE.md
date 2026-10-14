# Mini-Ultron — Current State

What exists and what is verified **right now** (2026-10-13, end of the V1 consolidation).
Stable rules: `../CLAUDE.md`. Settled hardware inventory and pins: `hardware.md`. Why things
are the way they are (bugs, measurements, decisions, session by session):
`history/SESSION_LOG.md`. Update this file after every meaningful session; keep it short.

Status words: **VERIFIED** (confirmed on real hardware / measured) · **TESTED** (automated
tests pass) · **COMPILES** · **NOT VERIFIED**.

## V1 scope

| Part | State |
|---|---|
| ESP32-S3: SSD1331 face (9 states + `calabaza` animation), INMP441 mic, DAC/amp + 8Ω speaker (shared I2S clocks GPIO9/10, mic SD=11, amp DIN=12), UART to Pico (RX=17, TX=18, 9600 baud), HTTP to CEREBRO | VERIFIED on the perfboard (2026-10-03) |
| Pico W: 4 Mecanum motors, neck servo (shield header), WS2812 emotion LEDs, line tracker (reported only), comms watchdog | VERIFIED |
| CEREBRO on the home-lab server: faster-whisper `small`, Piper `es_ES-davefx-medium`, Groq gpt-oss-120b (+20b fallback) | VERIFIED in daily use |
| Voice-driven movement (fixed 0.8 s slow bursts) | VERIFIED on the floor 2026-10-13 |
| Dance by voice (`calabaza`) | VERIFIED |

**Out of V1** (code kept, off by default, nothing depends on it):
MPU6050 IMU (`MINI_ULTRON_ENABLE_IMU 0`, GPIO13/14 free), HC-SR04 ultrasonic
(`MINI_ULTRON_ENABLE_ULTRASONIC 0` in the Pico `features.h`; GP4/GP5 never configured, `STATUS`
reports `DIST:-1.0`, **no forward obstacle veto**), PCA9685 + arm servos
(`MINI_ULTRON_ENABLE_ARMS 0`), camera, LittleFS animations (`MINI_ULTRON_ENABLE_ANIM_FS 0`).
Both sensors were physically removed 2026-10-13. The `OBSTACLE` response and the
`safetyForwardBlocked()` veto exist only inside the `#if MINI_ULTRON_ENABLE_ULTRASONIC` build; with
them off, forward motion is protected by: speed clamp (both sides), 500 ms watchdog, STOP priority,
exact-vocabulary parser, and the ESP32's fixed slow 0.8 s bursts. **There is no wall/edge
detection**: movement tests need clear floor and Enrico beside the robot (`safety.md`).

## Firmware builds

- Source defaults: brain movement **off**, IMU off, ultrasonic off, arms off.
- **Deployed ESP32 build is NON-DEFAULT**: `-DMINI_ULTRON_BRAIN_MOVEMENT=1`, kept on at Enrico's
  request. A plain re-flash from the source tree turns voice movement off again; to keep it:
  `arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,PSRAM=opi" --build-property "compiler.cpp.extra_flags=-DMINI_ULTRON_BRAIN_MOVEMENT=1" firmware/esp32/hello_ultron`
- ESP32 flash 92 % of the app partition (1,218,867 / 1,310,720 B): the next big feature needs a
  bigger partition or LittleFS animations first.
- Both firmwares COMPILE (see "Verification" below).

## CEREBRO

- Code in this repo has: Meta provider (works; Muse measured), explicit fallback, startup config validation, interaction
  ids in logs, expanded `eval_personality`. **The server still runs the previous deployed code**
  (`/api/health` lacks the `fallback` field) until it is redeployed — `development.md` →
  "Deploying". The server's existing `.env` stays valid (new variables are optional).
- Server settings in effect: `ULTRON_ALLOW_MOVEMENT=1` (so both movement gates are on).
- Latency: speech-end to playback ~1.7–3.1 s. Groq's free tier is 8000 tokens/min per model and
  one turn costs ~2300 prompt tokens, so sustained conversation hits 429s; the 20b fallback has
  its own quota. Detail in `eval/`.

## Provider evaluation

Muse Spark 1.3 smoke test done (`docs/eval/2026-10-13_muse-smoke.md`, 8 cases): safe and in
character, but 5.9 s p50 at `reasoning_effort=minimal` (20 s by default) vs ~0.8 s on Groq, ~9x the
cost. **Decision: Groq gpt-oss-120b stays.** The Groq baseline could not be re-run the same day (free
daily cap of 200,000 tokens exhausted by the evals). Groq free tier ≈ 85 turns/day: if the robot is
used a lot, the 20b fallback shares the problem on its own quota. `MODEL_API_KEY` is set in the
repo-root `.env` on the dev laptop only (gitignored). See `providers.md`.

## Verification (2026-10-13)

| Check | Result |
|---|---|
| `python manage.py test` (cerebro) | see final report; run it yourself, it needs no network |
| `arduino-cli compile` ESP32 `hello_ultron` | COMPILES |
| `arduino-cli compile` Pico `pico_ultron` | COMPILES |
| `eval_personality` against real Groq | runs; report in `eval/` |

## Known limitations (accepted for V1)

- No obstacle/edge detection (above). Mitigation is procedural.
- The mic is deaf while the robot thinks or speaks (no barge-in).
- Conversation memory is the last `ULTRON_HISTORY_TURNS` turns in RAM, lost on restart.
- Neck servo centers on every Pico boot (one small physical movement; accepted).
- `/api/*` has no authentication: LAN only, never expose port 8000. `ALLOWED_HOSTS` defaults to `*`
  (`DJANGO_ALLOWED_HOSTS` narrows it).
- Piper TTS is GPL-3.0 and imported in-process: blocks selling/open-sourcing CEREBRO until reviewed
  by a lawyer (`../THIRD_PARTY_LICENSES.md`). Does not affect running it.

## Deferred, with the condition that reopens each

| Item | Reopen when |
|---|---|
| Ultrasonic / any obstacle sensor | Ultron is meant to move unattended, or a unit ships without a human beside it |
| IMU (jolt reactions) | the I2S/I2C crosstalk is fixed in hardware (custom PCB, separate routing) |
| Arms / PCA9685 | Enrico decides arms return; `arm_gestures.*` is the starting point |
| Piper → subprocess or another TTS | a lawyer says in-process GPL is a problem, or before any sale/publication |
| Wi-Fi from any network (commercial) | the first unit leaves the home LAN (`CEREBRO_HOST` is compile-time today) |
| Second personality | Enrico asks; registry is `brain/personalities/` |
| Local Ollama brain | a model is chosen; set `LLM_PROVIDER=ollama`, no code needed |
| Barge-in, persistent memory, OTA | after V1 is in daily use |

## Next steps

1. Redeploy CEREBRO to the server (Enrico's go; it restarts the service).
2. Put a Meta `MODEL_API_KEY` in `cerebro/.env`, run the comparison with `--runs 3`, decide Muse vs gpt-oss.
3. Remove the temporary `ufw` rule for port 8000 on the dev laptop if still there (see history).
4. Legal review of Piper before any distribution.
