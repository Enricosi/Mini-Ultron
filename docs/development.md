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
python manage.py test brain                    # unit tests: schemas, LLM client (mocked
                                                # HTTP via httpx.MockTransport), STT/TTS
                                                # wrappers, views, pipeline. No network,
                                                # no real models required.
RUN_MODEL_TESTS=1 python manage.py test brain  # + a real Piper -> Whisper loopback test
```

Tests use `FakeLLM`/`FakeSTT`/`FakeTTS` (see `services.py`) rather than mocking at the
HTTP layer wherever a test cares about pipeline behavior, not wire format; the LLM
HTTP tests specifically use `httpx.MockTransport` to assert on the actual request
shape sent to Groq (payload fields, headers, retry behavior).

Firmware has no unit tests — verification is compile + (when explicitly authorized)
upload + Enrico's physical confirmation. **Always compile after a firmware change;
never claim "it should compile."**

```bash
arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,PSRAM=opi" firmware/esp32/hello_ultron
arduino-cli compile --fqbn "rp2040:rp2040:rpipicow" firmware/pico/pico_ultron
```

## Deploying a CEREBRO change to the home-lab server

```bash
rsync -a --delete --exclude var/ --exclude .env cerebro/ <user>@<server>:~/mini-ultron/cerebro/
ssh <user>@<server> "systemctl --user restart cerebro"
curl http://<server>:8000/api/health   # confirm it came back up with the expected config
```

The server is a live, in-use system (the robot may be actively talking to it) — a
restart drops in-RAM conversation history (acceptable, v0.1 keeps none persistently)
but does interrupt any conversation in progress. Prefer deploying between
conversations when the robot is in active use.

## Personality/prompt changes

Use `python manage.py eval_personality` (real Groq calls, real schema validation)
before and after a prompt edit — comparing on vibes alone is how the "insult
sparingly" rule silently stopped working the first time (see `CURRENT_STATE.md`,
2026-09-20). `--delay` controls pacing to stay under Groq's per-minute token limit
when comparing multiple models/cases in one run.

## Firmware conventions

Full list in `CLAUDE.md`; the two most load-bearing:

- **Vendor, don't rewrite.** Freenove driver code is copied in, trimmed to only the
  sections this chassis uses, with a header comment saying what was removed and why.
  Vendored function bodies are never edited — Mini-Ultron-specific behavior goes in
  the layer above (`motor_controller`, `neck_controller`, `led_controller`, etc.).
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
  session: what changed, what's verified vs. still just implemented, what's next.
- `CLAUDE.md` — only touch when a stable architectural decision, permanent safety
  rule, or long-term convention actually changes.
- `hardware.md` / `communications.md` / `safety.md` / `architecture.md` — update when
  the thing they describe actually changes (a new peripheral, a new protocol command,
  a new safety mechanism), not on every session.
