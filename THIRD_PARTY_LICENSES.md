# Third-party dependencies — technical license inventory

This is a technical inventory for engineering decisions, not legal advice. Items
marked **NEEDS LEGAL REVIEW** should go to an actual lawyer before Mini-Ultron is
sold, or its source published, as a commercial/open-source product. Nothing here is
a legal opinion.

Scope: every third-party dependency actually used to build or run Mini-Ultron today
— firmware libraries, Python packages, ML models/voices, and code we adapted or
vendored from elsewhere. Repo layout for context: `firmware/esp32/hello_ultron/`
(ESP32-S3), `firmware/pico/pico_ultron/` (Pico W), `cerebro/` (Django backend).

## Decision record (V1 consolidation, 2026-10-13)

| Question | Status |
|---|---|
| Piper (GPL-3.0) imported in-process by CEREBRO | **Open, not blocking V1.** Fine for running Mini-Ultron at home. Must be resolved by a lawyer before selling a unit or publishing the CEREBRO source. Containment: it is imported in exactly one place (`cerebro/brain/tts.py`, lazily, inside `PiperTTS`) behind the `TTSBackend` interface, and `fetch_models` uses `piper.download_voices`. Replacing it (another TTS class) or moving it behind a subprocess/HTTP boundary is a change to `tts.py` and `services.py` only. |
| Freenove-derived code | **Closed.** Not in the tree; `chassis_hal` is original (below). |
| Hosted LLM APIs (Groq, Meta Model API) | Services, not linked code: no license obligation in the repo, but their terms (data use, rate limits, regional availability — Meta's API preview was US-only when this was written) apply to any distributed product. Review before commercializing. Keys live only in `cerebro/.env`. |
| Anything else | Ordinary BSD/MIT/Apache plus LGPL NeoPixel; keep this file as the NOTICE seed. |

## Secrets/credentials scan (2026-09-21)

Checked: full tracked working tree, and full git history (`git log --all -p`) for
API-key-shaped strings, hardcoded passwords, and Wi-Fi SSID/password literals.
**Nothing found.** `cerebro/.env` and `firmware/esp32/hello_ultron/wifi_config.h`
have never been committed at any point in this repo's history (confirmed via
`git log --all --diff-filter=A --name-only`) — both are `.gitignore`d from the
start and only `*.example` templates are tracked. `freenove-car-reference/` (a full
clone of Freenove's own GitHub repo, kept locally for reference) is also
`.gitignore`d and has never been part of this repo's history, so it carries no
distribution risk if this repo is ever published. The home-lab server's LAN IP
(`<server>`) appears in docs/config — a private RFC1918 address, not a secret,
but worth scrubbing from public docs if the repo is ever made public, purely to
avoid advertising internal topology.

## Biggest finding — Piper TTS is GPL-3.0-or-later, imported in-process

**NEEDS LEGAL REVIEW — this is the one item that actually blocks "commercial
+ open-source" as currently wired.**

- **What**: `cerebro/brain/tts.py`'s `PiperTTS` backend does `from piper import
  PiperVoice` — a direct Python import into the same Django process, not a
  subprocess call to a separate `piper` executable.
- **Package**: `piper-tts` 1.8.0, from `OHF-voice/piper1-gpl` on GitHub.
  **License: GPL-3.0-or-later.** (Older `rhasspy/piper` releases were MIT — this
  project is on the newer GPL-licensed fork/rewrite, per `pip show piper-tts`.)
- **Why it matters**: GPL-3.0 is a strong copyleft license. Linking/importing a
  GPL library into your own process is generally treated as creating a combined
  "derivative work" that must itself be distributed under GPL-compatible terms —
  unlike LGPL, there's no "dynamic linking is fine" carve-out. If CEREBRO is ever
  distributed (sold as part of a product, or its source published) with Piper
  imported this way, the GPL's copyleft obligations plausibly extend to the whole
  Django backend that links against it. This is a real legal question, not
  something to resolve by guessing — get it reviewed before commercializing.
- **Possible technical mitigations** (engineering options, not legal conclusions):
  1. Invoke `piper` as a separate subprocess (it ships a CLI entry point) instead
     of importing it in-process — many projects treat "communicates over stdin/
     stdout with a separate GPL executable" as outside GPL's linking concern, but
     this is genuinely disputed and would still want a lawyer's sign-off.
  2. Swap to a permissively-licensed local TTS engine instead (e.g. an
     MIT-licensed alternative) if GPL is unacceptable for the product.
  3. Keep GPL and release the CEREBRO backend itself under a GPL-compatible
     license — viable if Mini-Ultron's software is meant to be fully open-source
     anyway, but forecloses a closed-source commercial variant of the backend.
- **Piper voice model** (`es_ES-davefx-medium`, an ONNX file downloaded by
  `manage.py fetch_models` from the `rhasspy/piper-voices` Hugging Face repo):
  **VERIFIED 2026-09-24** — the voice's own `MODEL_CARD` on Hugging Face
  (`rhasspy/piper-voices/es/es_ES/davefx/medium/MODEL_CARD`) states its training
  dataset (`davefx`, from `OHF-Voice/voice-datasets`) is **License: CC0**
  (public domain dedication, no restrictions). One nuance worth flagging, not
  resolving here: the model card also says it was "finetuned from U.S. English
  lessac voice (medium quality)" — the Lessac base voice's own licensing wasn't
  independently checked, so this is strong evidence the voice is unencumbered,
  not an absolute guarantee free of any upstream claim on the base checkpoint.

## Freenove-derived code — separately tracked

Mini-Ultron uses Freenove hardware (ESP32-S3 Ultimate Starter Kit, FNK0089 4WD
Mecanum kit). `freenove-car-reference/` (Freenove's own GitHub repo, cloned
locally) is **not part of this repo's git history** (gitignored) — it's
read-only reference material, never distributed if this repo is published.

| What | Where | Origin | License | Status |
|---|---|---|---|---|
| Motor/servo/ultrasonic/line-tracker driver | `firmware/pico/pico_ultron/chassis_hal.{h,cpp}` | **RESOLVED 2026-09-24 — reimplemented from scratch, no longer vendored.** The previous copy (`Freenove_4WD_Car_For_Pico_W.{h,cpp}`, a trimmed copy of Freenove's official `Freenove_4WD_Car_Kit_for_Raspberry_Pi_Pico` driver) was **removed from the working tree** (still recoverable from git history, but no longer part of the current source). `chassis_hal.{h,cpp}` reimplements the same behavior independently — pin numbers, motor polarity, and the servo's angle-to-duty-cycle mapping are physically-verified facts about this chassis (not copyrightable expression) that carried over; the code structure/expression is original. **COMPILES and PHYSICALLY VERIFIED** (`arduino-cli compile --fqbn rp2040:rp2040:rpipicow` 2026-09-24; uploaded same day and Enrico confirmed all of movement, neck, and sensor status work correctly on real hardware). | N/A — original code, only facts carried over | Clean, no Freenove-license dependency. Still depends on the third-party `RP2040_PWM` (khoih-prog, MIT) library for PWM generation, same as before — that dependency was never the licensing concern. |
| WS2812 LED control | `firmware/pico/pico_ultron/led_controller.{h,cpp}` | **Not vendored.** Freenove ships its own `Freenove_4WD_Car_WS2812.{h,cpp}` (flow/blink/rainbow modes) in the official kit examples, but Mini-Ultron deliberately wrote its own from-scratch driver instead (see the file's header comment) — specifically to avoid inheriting this same licensing question for a feature that was easy to reimplement independently. | N/A — original code | Clean, no Freenove-license dependency |
| Pin numbers / wiring facts (GPIO assignments, wheel mapping) | Comments throughout the Pico/ESP32 firmware, `docs/hardware.md` | Facts (pin numbers, physical wiring), not copyrightable expression | N/A | No license concern — facts about how to wire a board aren't copyrightable |

**Both Freenove-derived blockers are now fully closed**: the Pico driver above
(reimplemented and physically re-verified by Enrico 2026-09-24 — all 4 wheels,
neck, ultrasonic, and line tracker confirmed working), and the ESP32 side
never vendored Freenove code to begin with (see WS2812 row).

## Arduino/C++ libraries (firmware)

| Library | Version | License | Commercial use | Modification | Redistribution | Used for | Risk |
|---|---|---|---|---|---|---|---|
| Adafruit GFX Library | 1.12.6 | BSD (2-clause style) | Yes | Yes | Yes, with copyright notice | OLED graphics primitives | Low |
| Adafruit BusIO | 1.17.4 | MIT | Yes | Yes | Yes, with copyright notice | SPI/I2C transport for Adafruit display libs | Low |
| Adafruit SSD1331 OLED Driver Library | 1.3.0 | BSD (2-clause style) | Yes | Yes | Yes, with copyright notice | OLED face driver | Low |
| Adafruit NeoPixel | 1.15.5 | **LGPL-3.0** | Yes | Yes | Yes, with source availability for the library itself | WS2812 shield LEDs (`led_controller.cpp`) | Low-medium — LGPL's "combined work" relinking requirement is a real embedded-systems question in general, but Mini-Ultron already ships/publishes its own firmware source and expects users to compile/reflash it themselves, which is the situation LGPL's relinking clause is designed to be satisfied by. Still worth a line in any published LICENSE/NOTICE file. |
| Adafruit PWM Servo Driver Library | 3.0.3 | BSD (2-clause style) | Yes | Yes | Yes, with copyright notice | PCA9685 driver for the deferred/historical arm code (`arm_gestures.cpp`, compiled out via `MINI_ULTRON_ENABLE_ARMS 0`) | Low — and moot unless arms are reactivated |
| ArduinoJson | 7.4.3 | MIT | Yes | Yes | Yes, with copyright notice | Parsing CEREBRO's JSON replies (`brain_link.cpp`) | Low |
| RP2040_PWM (khoih-prog) | 1.7.0 | MIT | Yes | Yes | Yes, with copyright notice | PWM helper depended on by `chassis_hal.cpp` (motor/servo driver, reimplemented 2026-09-24) | Low |
| ESP32 Arduino core (WiFi, HTTPClient, Wire, SPI, ESP_I2S) | via `esp32:esp32` board package | Apache-2.0 / LGPL mix (Espressif's `arduino-esp32` core) | Yes | Yes | Yes | Wi-Fi, HTTP client, I2C, SPI, I2S on the ESP32 | Low — standard, widely-shipped-commercially board support package |
| RP2040 Arduino core (`arduino-pico`, earlephilhower) | via `rp2040:rp2040` board package | BSD-3-Clause | Yes | Yes | Yes | Base Arduino framework for the Pico W | Low |

## Python packages (`cerebro/requirements.txt`)

| Package | Version | License | Commercial use | Modification | Redistribution | Used for | Risk |
|---|---|---|---|---|---|---|---|
| Django | 6.1.1 | BSD-3-Clause | Yes | Yes | Yes | Web framework / HTTP API | Low |
| httpx | 0.28.1 | BSD-3-Clause | Yes | Yes | Yes | HTTP client to the LLM providers (Groq, Meta, Ollama, OpenAI-compatible) | Low |
| numpy | 2.5.3 | BSD-3-Clause (+ small bundled BSD/MIT/Zlib/CC0 pieces) | Yes | Yes | Yes | Audio DSP (`voice_fx.py`, `audio_utils.py`) | Low |
| faster-whisper | 1.2.1 | MIT | Yes | Yes | Yes | Local STT (wraps CTranslate2 + Whisper) | Low |
| **piper-tts** | 1.8.0 | **GPL-3.0-or-later** | See "Biggest finding" above | Yes | Yes, but copyleft applies | Local TTS | **High — NEEDS LEGAL REVIEW** |
| gunicorn | 26.2.0 | MIT | Yes | Yes | Yes | Production WSGI server on the home-lab deploy | Low |

### ML models

| Model | Source | License | Risk |
|---|---|---|---|
| faster-whisper `small` (Whisper weights, CTranslate2 format) | SYSTAN/faster-whisper on Hugging Face, derived from OpenAI Whisper | MIT (OpenAI Whisper's original license) | Low |
| Piper voice `es_ES-davefx-medium` | `rhasspy/piper-voices` on Hugging Face | **CC0** per the voice's own `MODEL_CARD` (verified 2026-09-24) — base checkpoint (`lessac`) not independently checked | Low |

## What this means for the roadmap

- **Not a blocker for continuing local development or hardware bring-up** — none of
  this affects how the robot works today, only what's safe to do once the goal is
  "publish the source" or "sell an assembled unit."
- **Before open-sourcing the repo**: the Freenove driver question is fully
  resolved (reimplemented and physically re-verified 2026-09-24); decide on
  the Piper GPL question (subprocess boundary, swap engine, or go full-GPL on
  the backend) — still open, needs an actual lawyer, not being pursued right
  now per Enrico's call (2026-09-24).
- **Before selling an assembled unit**: same Piper item, since a sold product
  is a distribution event same as publishing source.
- Nothing else in this inventory is a meaningful blocker — the rest is ordinary
  BSD/MIT-licensed tooling with no unusual obligations beyond keeping copyright
  notices in a NOTICE/THIRD_PARTY file, which this document already is the start of.
