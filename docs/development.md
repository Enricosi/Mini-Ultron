# Mini-Ultron — Development conventions

How to work in this repo day to day. For *what* the architecture is, see
`architecture.md`; for the rules that must never change, `CLAUDE.md`.

## Philosophy

"feo -> funcional -> ordenado -> bonito." Build the simplest working version first.
One concrete feature at a time. Prefer simple/working/understood over
clever/abstract/framework-heavy — see `CLAUDE.md` for the full statement. This
applies equally to firmware and to the Django backend.

## Testing

```bash
cd cerebro
python manage.py test                          # unit tests: schemas, providers (mocked HTTP via
                                                # httpx.MockTransport), config validation, fallback,
                                                # evaluation harness, STT/TTS wrappers, views, pipeline.
                                                # No network, no real models required.
RUN_MODEL_TESTS=1 python manage.py test        # + a real Piper -> Whisper loopback test
```

Tests use `FakeLLM`/`FakeSTT`/`FakeTTS` (see `services.py`) rather than mocking at the
HTTP layer wherever a test cares about pipeline behavior, not wire format; the LLM
HTTP tests specifically use `httpx.MockTransport` to assert on the actual request
shape sent to each provider (payload fields, headers, retry behavior, error kinds).

Every change to configuration, providers, fallback or the behavior schema needs a test
(`test_config.py`, `test_llm.py`, `test_pipeline.py`, `test_schemas.py`).

Firmware has no unit tests — verification is compile + (when explicitly authorized)
upload + Enrico's physical confirmation. **Always compile after a firmware change;
never claim "it should compile."**

```bash
arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,PSRAM=opi" firmware/esp32/hello_ultron
arduino-cli compile --fqbn "rp2040:rp2040:rpipicow" firmware/pico/pico_ultron
```

## Configuration and logging

All settings are environment variables read in `cerebro/config/settings.py` and
documented in `cerebro/.env.example`; `brain/checks.py` validates them at start-up. Do not
hardcode a value that someone may want to change per deployment: add a setting.

Logging uses the `brain.*` loggers (`ULTRON_LOG_LEVEL`: DEBUG, INFO, WARNING, ERROR).
INFO = one line per turn with the interaction id `[ab12cd34]`, the LLM that answered,
timings, tokens; WARNING = a recoverable failure (an LLM skipped, TTS failed, a retry);
ERROR = nothing answered (canned line) or an unexpected exception. Never log keys: errors
name the variable (`GROQ_API_KEY is not set`), never its value.

## Deploying a CEREBRO change to the home-lab server

```bash
rsync -a --delete --exclude var/ --exclude .env cerebro/ <user>@<server>:~/mini-ultron/cerebro/
ssh <user>@<server> "systemctl --user restart cerebro"
curl http://<server>:8000/api/health   # confirm provider, model, fallback, movement_allowed
journalctl --user -u cerebro -n 30     # "CEREBRO ready: llm=..." or the config error
```

The server is a live, in-use system (the robot may be actively talking to it) — a
restart drops in-RAM conversation history (acceptable, v0.1 keeps none persistently)
but does interrupt any conversation in progress. Prefer deploying between
conversations when the robot is in active use.

## Personality/prompt changes

Use `python manage.py eval_personality` (real API calls, real schema validation) before
and after a prompt edit or a model change — comparing on vibes alone is how the "insult
sparingly" rule silently stopped working the first time (`history/SESSION_LOG.md`,
2026-09-20). Use `--runs 3` and `--tpm` (see `providers.md`) and keep the report in
`docs/eval/`.

## Firmware conventions

Full list in `CLAUDE.md`; the two most load-bearing:

- **Freenove stays out of the tree.** The Pico's hardware access is `chassis_hal` (written
  from scratch; its pins, polarity and servo mapping are physically verified facts, so
  changing them means re-testing on the robot). Don't copy Freenove example code back in
  (`THIRD_PARTY_LICENSES.md`). Mini-Ultron behavior goes in the layer above
  (`motor_controller`, `neck_controller`, `led_controller`, etc.).
- **Optional hardware goes in `features.h`** with a harmless "not installed" behavior.
- **Non-blocking in real firmware.** `millis()`-based state machines, not `delay()`,
  anywhere concurrency matters (`pico_ultron.ino`, `hello_ultron.ino`'s main loop).
  `delay()` is only acceptable in a small, isolated, single-purpose bench-test sketch
  (`firmware/esp32/*_test/`).

## Committing

Small, logical commits — one concern per commit (a prompt fix, a firmware feature, a
docs update, are separate commits even in the same session). Don't bundle an
unrelated refactor into a bug-fix commit. Run tests/compiles before committing, not
after.

## Documentation maintenance

- `CURRENT_STATE.md` — update after every meaningful hardware/firmware/backend
  session, but keep it a snapshot (what exists, what is verified, what is deferred and
  why, next steps), not a diary. Session narrative that explains a decision goes in
  `history/SESSION_LOG.md`.
- `providers.md` / `docs/eval/` — update when a provider or the evaluation changes.
- `CLAUDE.md` — only touch when a stable architectural decision, permanent safety
  rule, or long-term convention actually changes.
- `hardware.md` / `communications.md` / `safety.md` / `architecture.md` — update when
  the thing they describe actually changes (a new peripheral, a new protocol command,
  a new safety mechanism), not on every session.
