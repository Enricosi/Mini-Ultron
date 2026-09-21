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
Raspberry Pi Pico W (lower body: locomotion + neck + sensors + local safety)
        v
Freenove car shield / 4 DC motors / neck servo / ultrasonic / line tracker / WS2812
```

Each arrow narrows what can be expressed: the LLM can ask for `movement: FORWARD`,
never a PWM duty cycle. Django validates that into a Behavior. The ESP32 turns a
Behavior into the Pico's fixed command vocabulary. The Pico turns a command into
motor/servo control, but only after its own safety layer agrees. Nothing skips a
layer, and no layer trusts the one above it more than it would trust a human typing
directly into its own serial console — see `safety.md`.

## CEREBRO (Django) — internal shape

```
brain/
  personality.py   the character: system prompt text only, no state machine (see below)
  llm.py           LLMBackend interface + OpenAICompatLLM (Groq, Ollama, any
                   OpenAI-compatible endpoint) + FakeLLM for tests
  schemas.py       the ONLY vocabulary the LLM may produce; validates raw model
                   output into a bounded Behavior dataclass
  stt.py           STTBackend interface + FasterWhisperSTT + FakeSTT
  tts.py           TTSBackend interface + PiperTTS + FakeTTS
  voice_fx.py      numpy-only voice shaping (pitch/presence/etc.) on top of Piper
  pipeline.py      the whole turn: audio -> STT -> LLM -> validated Behavior -> TTS,
                   with a documented failure policy per stage
  services.py      builds the Pipeline from Django settings — this is the provider
                   abstraction seam: swapping LLM/STT/TTS backend is a settings
                   change, not a code change
  views.py         the HTTP API the ESP32 calls
  audio_utils.py   PCM/WAV decode, resampling, normalization helpers
```

This already implements the "don't lock into one vendor" requirement for the LLM: a
provider is selected by `LLM_PROVIDER` + `LLM_MODEL` (`groq`, `ollama`, or any
OpenAI-compatible `openai_compat` endpoint) through one client class
(`OpenAICompatLLM`), not one class per vendor. Adding a genuinely different wire
protocol (not OpenAI-compatible — e.g. native Anthropic/Gemini) means adding one more
`LLMBackend` subclass; it doesn't touch `pipeline.py`, `schemas.py`, or `views.py`.
The same pattern already exists for STT and TTS (`STTBackend`/`TTSBackend`).

### Personality as a system (direction, not built yet as a framework)

Today personality is deliberately just `SYSTEM_PROMPT`, a string — intentionally not
over-engineered before it needed to be. The near-term useful split, when it's
actually needed (e.g. a second personality pack, or moods that persist across turns),
is:

- **Base personality** — who Mini-Ultron is (currently `SYSTEM_PROMPT`'s "QUIÉN ERES"/"CÓMO HABLAS" sections).
- **Rules/guardrails** — hard constraints (protected characteristics, self-harm, no
  real threats) that must survive any personality pack.
- **Mood/state** — short-lived modifiers (ufano, ofendido, aburrido...) already
  mentioned in the prompt as flavor, not yet tracked as real state between turns.
- **Context/memory** — `pipeline.History`, currently a rolling in-RAM window per
  session (`ULTRON_HISTORY_TURNS`), no persistence.
- **User recognition** — not implemented; the LLM currently addresses one undivided
  "user".
- **Output contract** — `schemas.py`'s `Behavior`, already a clean seam.

The useful boundary to add first, if/when a second personality pack is ever wanted,
is separating "rules that never change" from "flavor text that could be swapped" at
the prompt-assembly level — not a plugin system, not a registry, just two prompt
fragments concatenated. Don't build more than that until a second pack actually
exists.

## Skills / events (future, not built)

CLAUDE.md and the product direction call for third parties eventually being able to
register capabilities/events (e.g. reacting to `person_detected`). Nothing like this
exists yet and nothing should be built speculatively — `schemas.py`'s Behavior
vocabulary is the only "capability surface" today, and it's driven entirely by the
LLM's own JSON output, not by an event bus. If/when a real second event source shows
up (e.g. vision), design the minimal registration point then, against that real
need.

## Firmware shape

See `hardware.md` for pins/peripherals and `communications.md` for the wire
protocols. Both firmwares follow the same module style: one file per concern
(`face`, `pico_link`, `audio`, `imu`, `motor_controller`, `neck_controller`,
`safety_controller`, `sensors`, `led_controller`, ...), no God-object, no blocking
calls in `loop()` (blocking work — I2S, HTTP — lives in FreeRTOS tasks on the ESP32).
Vendored Freenove code stays vendored and untouched at the function-body level;
Mini-Ultron-specific behavior is added in the layer above it. See CLAUDE.md's
"Firmware conventions" for the full rules.

## Vision (deferred, designed conceptually only)

No camera is physically on v1 hardware. If/when it returns, the intended shape is a
separate `VisionProvider`-style seam (local vs. cloud vs. future accelerated),
producing the same kind of bounded, validated signal the LLM already consumes (e.g.
"person detected", not raw frames pushed into the prompt), with capture on-demand
rather than continuous streaming, for privacy, latency, and cost reasons. Nothing
here is implemented; this is a placeholder for where it would plug in, not a spec to
build against yet.
