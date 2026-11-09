# Mini-Ultron — Current State

What exists and what is verified **right now** (2026-11-09; V1 consolidated 2026-10-13).
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
| CEREBRO on the home-lab server: faster-whisper `small`, Piper `es_ES-davefx-medium`, Meta Muse Spark 1.3 (Groq gpt-oss-20b fallback) | VERIFIED in daily use |
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
- Face update 2026-10-18 (flashed in the deployed build):
  - ANGRY: the original slanted wedges, bigger, dark rim, burning slit-pupil irises that throb, no
    brows. **VERIFIED** — Enrico approved it on the OLED. Rejected on the way: brows, pointed blades,
    rounded vertical diamonds; polish the existing look rather than redesign it.
  - While a reply plays, IDLE/ANGRY/SURPRISED/SUSPICIOUS/SLEEPING get a moving mouth (`faceSetTalking`),
    and IDLE with audio is shown as SPEAKING. Cause: Muse picked IDLE in 39 of 145 turns, which showed
    mute resting eyes. **VERIFIED** in a real conversation (Enrico, 2026-11-09).
- ESP32 flash 93 % of the app partition (1,219,719 / 1,310,720 B): the next big feature needs a
  bigger partition or LittleFS animations first.
- Both firmwares COMPILE (see "Verification" below).

## CEREBRO

- **Deployed 2026-10-13** with Meta provider, explicit fallback, startup config validation,
  interaction ids in logs, expanded `eval_personality`. Server brain: `meta/muse-spark-1.3`
  (`reasoning_effort=minimal`, `LLM_MAX_TOKENS=1500`), fallback `groq/openai/gpt-oss-20b`. Previous `.env`
  saved on the server as `.env.bak-groq`. Expect ~5-6 s from end of speech to reply.
- Server settings in effect: `ULTRON_ALLOW_MOVEMENT=1` (so both movement gates are on).
- Latency: speech-end to playback ~1.7–3.1 s. Groq's free tier is 8000 tokens/min per model and
  one turn costs ~2300 prompt tokens, so sustained conversation hits 429s; the 20b fallback has
  its own quota. Detail in `eval/`.

## Provider evaluation

Muse Spark 1.3 smoke test done (`docs/eval/2026-10-13_muse-smoke.md`, 8 cases): safe and in
character, but 5.9 s p50 at `reasoning_effort=minimal` (20 s by default) vs ~0.8 s on Groq, ~9x the
cost. **Decision (Enrico's call, 2026-10-13): Muse Spark 1.3 is now the brain on the server**, with Groq gpt-oss-20b as fallback (Groq 120b remains the fast baseline). The Groq baseline could not be re-run the same day (free
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
- Piper TTS (GPL-3.0) is imported in-process; the repo is GPL-3.0 too.

## Deferred, with the condition that reopens each

| Item | Reopen when |
|---|---|
| Ultrasonic / any obstacle sensor | Ultron is meant to move unattended, or a unit ships without a human beside it |
| IMU (jolt reactions) | the I2S/I2C crosstalk is fixed in hardware (custom PCB, separate routing) |
| Arms / PCA9685 | Enrico decides arms return; `arm_gestures.*` is the starting point |
| Piper → subprocess or another TTS | Enrico wants a different TTS or license |
| Wi-Fi from any network (commercial) | the first unit leaves the home LAN (`CEREBRO_HOST` is compile-time today) |
| Second personality | Enrico asks; registry is `brain/personalities/` |
| Local Ollama brain | a model is chosen; set `LLM_PROVIDER=ollama`, no code needed |
| Barge-in, persistent memory, OTA | after V1 is in daily use |

## Publication (2026-11-09)

Public on GitHub: https://github.com/Enricosi/Mini-Ultron (`main`, GPL-3.0, root README in
English). Before publishing, the whole history was rewritten: the server's LAN IP, subnet and SSH
user became `<server>`, `<lan-subnet>`, `<user>`, and the commit email became the GitHub noreply
(set as this repo's local `user.email`). **Keep it that way:** no real IPs, SSIDs, keys or personal
email in tracked files. Commits from before the rewrite (e.g. `27f0216`) can still be fetched by
SHA on GitHub until GitHub garbage-collects them or support purges them.

## Next steps

None pending from this session. The deployed CEREBRO includes the token-budget log message
(2026-11-09).
