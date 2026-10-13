# Mini-Ultron — Architecture

Stable, high-level view of how the system is put together. For the permanent rules
this architecture must never violate, see `CLAUDE.md`. For live status/what's
verified, see `CURRENT_STATE.md`. For hardware detail, `hardware.md`. For the wire
protocols, `communications.md`.

## Layers

```
Django / home-lab mini server  (CEREBRO: conversation, personality, LLM, STT/TTS)
        | Wi-Fi, plain HTTP — abstract intent only, never raw hardware values
        v
ESP32-S3            (upper body: face, voice, network, ESP32<->Pico bridge)
        | UART, ASCII line protocol
        v
Raspberry Pi Pico W (lower body: locomotion + neck + local safety)
        v
Freenove car shield / 4 DC motors / neck servo / line tracker / WS2812
(IMU and ultrasonic: optional compile-time modules, not in v1)
```

Each arrow narrows what can be expressed: the LLM can ask for `movement: FORWARD`,
never a PWM duty cycle. Django validates that into a Behavior. The ESP32 turns a
Behavior into the Pico's fixed command vocabulary. The Pico turns a command into
motor/servo control, but only after its own safety layer agrees. Nothing skips a
layer, and no layer trusts the one above it more than it would trust a human typing
directly into its own serial console — see `safety.md`.

## CEREBRO (Django) — internal shape

```
config/settings.py   every setting, read from env vars / cerebro/.env; bad numbers fail with the variable name
brain/
  checks.py        startup validation of the settings (all problems listed at once) + API-key resolution
  llm.py           LLMBackend interface, OpenAICompatLLM (Groq, Meta, Ollama, any OpenAI-style
                   endpoint, via PROVIDER_PRESETS), FakeLLM; LLMError carries a `kind`
  schemas.py       the ONLY vocabulary the LLM may produce; validates raw model output into a
                   bounded Behavior (face, movement, neck, animation) and applies the movement
                   and dance policies
  personalities/   registry of characters (system prompt + canned fallback line); "ultron" today
  stt.py / tts.py  STTBackend/TTSBackend interfaces + faster-whisper / Piper + fakes
  voice_fx.py      numpy-only voice shaping on top of Piper
  pipeline.py      the whole turn: audio -> STT -> LLM (main, then the one fallback) -> validated
                   Behavior -> TTS, with a documented failure policy per stage and an
                   interaction id on every log line
  services.py      builds the Pipeline from settings (the one place that knows concrete classes),
                   releases HTTP clients at exit
  views.py / urls.py  the HTTP API the ESP32 calls
  evals.py         cases, checks, stats and report of the model evaluation
  audio_utils.py   PCM/WAV decode, resampling, normalization
  management/commands/  ask, converse_wav, fetch_models, eval_personality
```

The LLM is chosen by `LLM_PROVIDER` + `LLM_MODEL` and talks through one client class, not
one class per vendor; personality, actions, memory and policy never see which provider
answered (`providers.md`). A genuinely different wire protocol (native Anthropic/Gemini)
would be one more `LLMBackend` subclass; nothing else changes. STT and TTS follow the same
pattern. Failure handling is explicit and has a single level: main LLM, one configured
fallback, canned line — never a chain.

### Personality

A `Personality` is a dataclass (system prompt, canned fallback text and face) registered
in `brain/personalities/__init__.py` and selected by `ULTRON_PERSONALITY`. Memory is
`pipeline.History`: the last `ULTRON_HISTORY_TURNS` turns per session in RAM. Mood state,
user recognition and persistent memory do not exist; add them against a real need.

### Events

`POST /api/event` lets a controller report a physical event instead of an utterance
(`EVENT_PROMPTS` in `pipeline.py`; today only `JOLT`, which only an IMU build can send).
No event bus exists or is planned until a second real event source does.

## Firmware shape

See `hardware.md` for pins/peripherals and `communications.md` for the wire
protocols. Both firmwares follow the same module style: one file per concern
(`face`, `pico_link`, `audio`, `imu`, `motor_controller`, `neck_controller`,
`safety_controller`, `sensors`, `led_controller`, ...), no God-object, no blocking
calls in `loop()` (blocking work — I2S, HTTP — lives in FreeRTOS tasks on the ESP32).
Freenove-derived code is not in the tree: the Pico's hardware access is
`chassis_hal.{h,cpp}`, written from scratch (`THIRD_PARTY_LICENSES.md`). Optional
hardware is switched per build in each controller's `features.h`. See CLAUDE.md's
"Firmware conventions" for the full rules.

## Vision (deferred, designed conceptually only)

No camera is physically on v1 hardware. If/when it returns, the intended shape is a
separate `VisionProvider`-style seam (local vs. cloud vs. future accelerated),
producing the same kind of bounded, validated signal the LLM already consumes (e.g.
"person detected", not raw frames pushed into the prompt), with capture on-demand
rather than continuous streaming, for privacy, latency, and cost reasons. Nothing
here is implemented; this is a placeholder for where it would plug in, not a spec to
build against yet.
