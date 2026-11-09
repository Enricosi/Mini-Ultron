# Mini-Ultron

A small, local, theatrical robot with the personality of a cartoon villain. You talk to it; it
listens, thinks, answers in a deep solemn voice, glares at you from an OLED face, turns its head,
and — if you let it — rolls a short distance on Mecanum wheels.

The robot itself is two microcontrollers. Its brain, **CEREBRO**, is a Django service on a home-lab
server: speech-to-text, an LLM, text-to-speech, and a strict validation layer between what the
model says and what the hardware is allowed to do.

## How it works

```
CEREBRO  (Django on a home-lab server: Whisper STT -> LLM -> Piper TTS)
    |  Wi-Fi, plain HTTP: audio in, JSON intent + audio out (never hardware values)
    v
ESP32-S3  (upper body: OLED face, mic, speaker, network)
    |  UART, ASCII line protocol
    v
Raspberry Pi Pico W  (lower body: 4 Mecanum motors, neck servo, LEDs, local safety)
```

One turn of conversation:

1. The ESP32 hears an utterance on its INMP441 mic and POSTs the raw PCM to `/api/converse`.
2. CEREBRO transcribes it (faster-whisper), asks the LLM for a reply in a fixed JSON shape, and
   validates it into a bounded `Behavior`:
   `{"text": "...", "face": "ANGRY", "movement": "NONE", "neck": "LEFT", "animation": "NONE"}`.
3. CEREBRO speaks the text with Piper, plus a voice effect, and returns the JSON and the audio.
4. The ESP32 shows the face, plays the voice with a moving mouth, and forwards neck/movement
   intents to the Pico as fixed commands (`MOVE:FWD`, `NECK:LEFT`, ...).
5. The Pico executes them only if its own safety layer agrees.

## Safety by design

The LLM can ask for `movement: FORWARD`. It can never produce a speed, a duration, a PWM value or
a servo angle. Every layer narrows what can be expressed, and the lowest physical controller keeps
the final veto:

- Boot state is always STOP. Unknown or malformed commands never cause movement.
- Speed is clamped on both controllers; movement is a fixed, slow, ~0.8 s burst chosen by firmware.
- A 500 ms watchdog on the Pico stops the motors if the link goes quiet. STOP beats everything.
- Voice-driven movement is opt-in twice, both off by default: `ULTRON_ALLOW_MOVEMENT` in CEREBRO and
  `MINI_ULTRON_BRAIN_MOVEMENT` in the ESP32 firmware.
- V1 has no obstacle sensor, so movement tests need clear floor and a human beside the robot.

Details: [`docs/safety.md`](docs/safety.md).

## Hardware (V1)

| Part | Role |
|---|---|
| Freenove ESP32-S3 WROOM (PSRAM) | Face, audio, Wi-Fi, bridge to the Pico |
| SSD1331 96x64 color OLED | The face: 9 expressions plus a dance animation |
| INMP441 I2S microphone | Voice in |
| ES7148 DAC + PAM8403 amp, 8 Ω 2 W speaker | Voice out |
| Raspberry Pi Pico W on a Freenove FNK0089 car shield | Locomotion and local safety |
| 4 DC motors + Mecanum wheels, neck servo, 8 WS2812 LEDs, line tracker | Lower body |
| 2x 18650 cells | Motor power |
| Any small Linux box on the LAN | Runs CEREBRO |

Pins, wiring and power: [`docs/hardware.md`](docs/hardware.md). An MPU6050 IMU and an HC-SR04
ultrasonic are supported as optional compile-time modules but are not part of V1.

## Repository layout

```
cerebro/                 Django backend (STT, LLM providers, TTS, personality, validation)
firmware/esp32/          hello_ultron/ is the robot firmware; the rest are bench sketches
firmware/pico/           pico_ultron/: motors, neck, LEDs, watchdog, command parser
deploy/                  systemd user unit for CEREBRO
docs/                    architecture, protocols, hardware, safety, setup, current state
hardware/mechanical/     CAD, drawings and mechanical notes
tools/anim/              converts a GIF or video into an OLED face animation
```

## Getting started

Full instructions: [`docs/setup.md`](docs/setup.md).

**CEREBRO** (tested on Python 3.14, on the server):

```bash
python3 -m venv venv && venv/bin/pip install -r cerebro/requirements.txt
cp cerebro/.env.example cerebro/.env      # add your own LLM API key here; never commit it
cd cerebro
../venv/bin/python manage.py fetch_models # Piper voice + Whisper model
../venv/bin/python manage.py test         # offline unit tests
../venv/bin/gunicorn config.wsgi --bind 0.0.0.0:8000 --workers 1 --threads 4 --timeout 90
```

Try it without any hardware:

```bash
python manage.py ask "Hola Ultron, ¿quién eres?" --wav reply.wav
```

**ESP32-S3** (Arduino CLI): copy `firmware/esp32/hello_ultron/wifi_config.example.h` to
`wifi_config.h` (gitignored), fill in your Wi-Fi and the CEREBRO host, then:

```bash
arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,PSRAM=opi" firmware/esp32/hello_ultron
```

**Pico W**:

```bash
arduino-cli compile --fqbn "rp2040:rp2040:rpipicow" firmware/pico/pico_ultron
```

Optional hardware is switched on or off in each controller's `features.h`.

## LLM providers

The brain speaks to any OpenAI-compatible endpoint, selected by environment variables: Groq, Meta
Model API (Muse Spark), Ollama for a fully local model, or anything OpenAI-style. One fallback
provider is tried when the main one fails, then a canned in-character line. You bring your own model
and API key; keys live only in `cerebro/.env`. See [`docs/providers.md`](docs/providers.md).

## Status

V1 is complete and in daily use: conversation, faces, voice, neck, voice-driven movement and a
dance are verified on the real robot. What is verified right now and what comes next:
[`docs/CURRENT_STATE.md`](docs/CURRENT_STATE.md).

Known limitations: no obstacle detection, no barge-in (the mic is deaf while it speaks),
conversation memory is RAM-only, and the HTTP API has no authentication, so keep it on your LAN.

## Documentation

| Doc | Contents |
|---|---|
| [`docs/architecture.md`](docs/architecture.md) | Layers and internal structure |
| [`docs/communications.md`](docs/communications.md) | HTTP API and the UART line protocol |
| [`docs/hardware.md`](docs/hardware.md) | Components, pins, power |
| [`docs/safety.md`](docs/safety.md) | Who can veto what, watchdog, test discipline |
| [`docs/setup.md`](docs/setup.md) | Building, flashing, deploying |
| [`docs/providers.md`](docs/providers.md) | LLM providers, fallback, evaluation |
| [`cerebro/README.md`](cerebro/README.md) | Backend API and configuration |
| [`docs/history/SESSION_LOG.md`](docs/history/SESSION_LOG.md) | How it got here, session by session |

## License

[GPL-3.0](LICENSE). Third-party components and their licenses:
[`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md).
