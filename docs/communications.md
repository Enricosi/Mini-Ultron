# Mini-Ultron — Communication protocols

Two wire protocols exist. Both are deliberately simple, text-based (except the raw
audio bodies), and designed so the lowest layer never has to trust the layer above it
— see `safety.md`.

## 1. Django (CEREBRO) ↔ ESP32-S3 — plain HTTP

The ESP32 is the HTTP client; CEREBRO is the server, reachable on the LAN only
(`ALLOWED_HOSTS` defaults to `*`, narrow it with `DJANGO_ALLOWED_HOSTS`; no auth — do not
expose this port to the internet).

| Endpoint | Method | Body / params | Response |
|---|---|---|---|
| `/api/converse` | POST | one utterance, raw PCM s16le mono 16 kHz (or WAV); header `X-Session-Id` (optional, defaults to `"default"`) | JSON: `transcript`, `behavior {text, face, movement, neck, animation}`, `audio {url, bytes, sample_rate}` or `null`, `timings_ms {stt, llm, tts, total}`, `warnings`, or `{"ignored": true, "reason": ...}` if too short/silent/hallucinated |
| `/api/audio/<id>` | GET | — | reply audio, raw PCM s16le mono 16 kHz, `Content-Length` + `X-Sample-Rate` set |
| `/api/converse_text` | POST | `{"text": "...", "session": "..."}` | same JSON shape as `/api/converse`, no STT — used for testing without a mic |
| `/api/event` | POST | `{"event": "JOLT", "session": "..."}` | same JSON shape; a controller-reported physical event (unknown names are ignored). Only an IMU build sends `JOLT`. |
| `/api/health` | GET | — | LLM provider/model/fallback, whether the API key is set (never its value), STT/TTS backends, `movement_allowed`, `personality` |

One request per utterance (not a stream): the firmware already segments whole
utterances client-side, `HTTPClient` ships with the ESP32 Arduino core, and every
request/response is curl-debuggable. A WebSocket would only buy streaming this
project doesn't currently need.

### The `behavior` payload — the only vocabulary the LLM may produce

Validated by `cerebro/brain/schemas.py` (`BEHAVIOR_JSON_SCHEMA`, enforced as a strict
`json_schema` response format when the provider supports it, and re-validated in
Python regardless):

```json
{
  "text": "lo que se dice en voz alta",
  "face": "IDLE | ANGRY | SURPRISED | SUSPICIOUS | SPEAKING | GLITCH | SLEEPING",
  "movement": "NONE | FORWARD | BACKWARD | STRAFE_LEFT | STRAFE_RIGHT | ROTATE_LEFT | ROTATE_RIGHT",
  "neck": "NONE | CENTER | LEFT | RIGHT",
  "animation": "NONE | CALABAZA"
}
```

No numeric or hardware-shaped field exists here on purpose: no PWM, no angle, no
speed, no duration. `LISTENING` and `THINKING` faces are deliberately absent from
this vocabulary — those are ESP32-owned system states, not something the LLM picks.
Unknown/invalid values fall back to safe defaults (`schemas.parse_behavior`); missing
`text` is the one fatal case, and it triggers the canned fallback line in
the personality's canned `fallback_text`/`fallback_face` so the robot never goes mute
(after the configured fallback LLM, if any, also failed — see `providers.md`). Movement is
additionally dropped server-side (`apply_policy`) unless `ULTRON_ALLOW_MOVEMENT=1`, and an
animation is dropped unless the user's own words ordered a dance (`apply_animation_policy`).
The response also carries `warnings` (e.g. `llm_error: groq/... timeout: ...`,
`movement FORWARD dropped: ...`), which the ESP32 ignores and humans read.

## 2. ESP32-S3 ↔ Pico W — UART, ASCII line protocol

Physical layer: UART, 9600 baud, newline-terminated ASCII lines, one command per
line, fields separated by `:`. Pico side is fixed by the RP2040 module (GPIO0=TX,
GPIO1=RX); ESP32 side is `pico_link_config.h` (RX=GPIO17, TX=GPIO18). The exact same
parser (`command_parser.cpp`, templated over `Stream&`) also serves the Pico's USB
bench interface — there is no protocol difference between "a human typing into the
Serial Monitor" and "the ESP32 over UART", which is the point: the Pico does not
grant the ESP32 any more trust than a human at a terminal.

**Inbound (ESP32 → Pico, or human → Pico over USB):**

```
MOVE:FWD:<0-100>    MOVE:BACK:<0-100>
MOVE:LEFT:<0-100>   MOVE:RIGHT:<0-100>   (strafe)
ROTATE:L:<0-100>    ROTATE:R:<0-100>     (rotate in place)
NECK:CENTER         NECK:LEFT            NECK:RIGHT
LED:ON              LED:OFF              (car-shield WS2812 status LEDs)
LED:FACE:<NAME>     same 9 names as the ESP32's FaceState, e.g. LED:FACE:ANGRY
STOP
STATUS
PING
```

**Outbound (Pico → ESP32/USB):**

```
OK                  command accepted (and executed, if it involved motion)
ERR:PARSE           line didn't match any known command shape
OBSTACLE            MOVE:FWD refused - ultrasonic sees something too close (never sent
                    when no ultrasonic is installed - the v1 default)
DIST:<cm>           part of the STATUS reply; DIST:-1.0 = no ultrasonic installed
LINE:<3 bits>       part of the STATUS reply, e.g. LINE:010 (L,C,R)
STOPPED:TIMEOUT     the communication watchdog fired (no command for >500ms while moving)
READY               printed once at boot
```

Any line that doesn't match exactly one of the inbound shapes above returns
`ERR:PARSE` and causes no actuation — there is no partial/fuzzy match, on purpose
(see `safety.md`).

### ESP32-side high-level API (`pico_link.h`)

The rest of the ESP32 firmware never builds a raw protocol line itself; it only calls
`picoPing/Stop/Status()`, `picoMoveForward/Backward/Left/Right(speedPercent)`,
`picoRotateLeft/Right(speedPercent)`, `picoNeckCenter/Left/Right()`,
`picoLedOn/Off()`, `picoLedFace(faceName)` (called once, from inside `face.cpp`'s
`setFace()`, so every face change syncs the shield LEDs with no extra call sites).
Speed is clamped 0-100 before it's ever formatted into a line.
There is no function in this module that accepts an arbitrary string, so nothing
upstream — including a bug in the Django/LLM layer — has a code path to smuggle a raw
command to the Pico.

### ESP32 debug console (USB Serial, 115200 baud, for bench use)

Mirrors the same style, with a couple of extra observability-only commands:

```
FACE:IDLE|ANGRY|LISTENING|THINKING|SPEAKING|SURPRISED|SUSPICIOUS|SLEEPING|GLITCH
PICO:PING | PICO:STOP | PICO:STATUS | PICO:LAST | PICO:LED:ON | PICO:LED:OFF
NECK:CENTER | NECK:LEFT | NECK:RIGHT
MOVE:FWD:<0-100> | MOVE:BACK:<0-100> | MOVE:LEFT:<0-100> | MOVE:RIGHT:<0-100>
ROTATE:L:<0-100> | ROTATE:R:<0-100>
AUDIO:STATUS | AUDIO:BEEP | AUDIO:REPLAY
BRAIN:STATUS | MOTION:STOP
IMU:STATUS | IMU:CAL             (IMU off by default since 2026-10-02: stub values)
ARMS:NEUTRAL|UP|LEFT|RIGHT|WAVE   (ERR:ARMS_DISABLED — deferred hardware)
HELP
```

`PICO:LAST` is the ESP32's own receive-path state (`ALIVE`, last response `TYPE`,
`DIST`, `LINE`), useful for confirming the UART link is actually alive without
needing a second USB cable on the Pico.
