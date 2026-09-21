# Mini-Ultron — Setup

How to stand up each piece from scratch. This is the reproducibility answer: if the
home-lab server, the ESP32, or the Pico died today, this is what you'd redo.

## CEREBRO (Django backend) — on the home-lab server, never the laptop

Full instructions: `cerebro/README.md`. Short version:

```bash
python3 -m venv venv && venv/bin/pip install -r cerebro/requirements.txt
cp cerebro/.env.example cerebro/.env    # then set GROQ_API_KEY
cd cerebro
../venv/bin/python manage.py fetch_models   # downloads the Piper voice + Whisper model
../venv/bin/python manage.py test brain     # 60+ unit tests, no network/models needed
```

Run it with `deploy/cerebro.service` (a systemd **user** unit — no sudo needed to
install it, just `loginctl enable-linger $USER` once so it survives reboots without a
login session). The server's firewall needs to allow the ESP32 to reach TCP/8000 on
the LAN.

Never put `GROQ_API_KEY` (or any secret) in code, in a commit, or in this repo's
tracked files — it lives in `cerebro/.env` (gitignored) or a real environment
variable, full stop.

## ESP32-S3 firmware (`firmware/esp32/hello_ultron/`)

```bash
arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,PSRAM=opi" hello_ultron
arduino-cli upload  --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,PSRAM=opi" -p /dev/ttyACM0 hello_ultron
```

`CDCOnBoot=cdc` is required or the USB debug console silently never binds — see
`CURRENT_STATE.md` for how much time that cost the first time it happened.
`PSRAM=opi` is required for the full 12s utterance buffer and the CEREBRO link at
all; without it the firmware still runs but with a 4s cap and the brain link
disabled.

Required libraries (installed via `arduino-cli lib install`, not bundled in this
repo — see `THIRD_PARTY_LICENSES.md`): Adafruit SSD1331 OLED Driver, Adafruit GFX,
Adafruit BusIO, ArduinoJson.

Copy `wifi_config.example.h` to `wifi_config.h` (gitignored) and fill in real Wi-Fi
credentials, `CEREBRO_HOST`, `CEREBRO_PORT`. Without this file the firmware still
compiles and runs; the brain link just logs that it's disabled instead of connecting.

`/dev/ttyACM*` assignments are **not stable across sessions** when both boards are
plugged in — always check `arduino-cli board list` before uploading, don't assume
from a previous session.

## Pico W firmware (`firmware/pico/pico_ultron/`)

```bash
arduino-cli compile --fqbn "rp2040:rp2040:rpipicow" pico_ultron
arduino-cli upload  --fqbn "rp2040:rp2040:rpipicow" -p /dev/ttyACM0 pico_ultron
```

Required libraries: RP2040_PWM (khoih-prog) and Adafruit NeoPixel (both installed via
Arduino Library Manager, not bundled — see `THIRD_PARTY_LICENSES.md`). Uploading
sometimes requires manually putting the Pico into BOOTSEL mode first (unplug, hold
BOOTSEL, replug, release) — `arduino-cli upload` has worked without it at least once,
reliability unconfirmed; fall back to manual BOOTSEL if it fails with "No drive to
deploy".

**Uploading firmware that can drive motors/servos, or powering the chassis, is
Enrico's action** — see `CLAUDE.md`'s hardware-actuation rule.

## Trying the backend without any hardware

```bash
python manage.py ask "Hola Ultron, ¿quién eres?" --wav reply.wav
python manage.py converse_wav utterance.pcm --out reply.wav
python manage.py eval_personality              # the reproducible personality/model eval set
```
