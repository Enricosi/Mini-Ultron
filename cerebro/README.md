# CEREBRO — Mini-Ultron's brain (Django)

Voice in → **faster-whisper** (STT) → **LLM** (Groq) → validated behavior → **Piper** (TTS) → voice out,
plus abstract `face` / `neck` / `movement` intents for the ESP32. The LLM never produces hardware
values; `brain/schemas.py` is the only vocabulary it can speak, and Django validates everything.

**Where it runs:** on the home-lab mini server, not the laptop. The laptop is only for development.
Nothing here depends on the laptop: config is env vars, models live in `ULTRON_DATA_DIR`.

## Set up (on the server)

```bash
python3 -m venv venv && venv/bin/pip install -r cerebro/requirements.txt
cp cerebro/.env.example cerebro/.env        # then put GROQ_API_KEY in it
cd cerebro
../venv/bin/python manage.py fetch_models   # Piper voice + Whisper model -> ULTRON_DATA_DIR
../venv/bin/python manage.py test brain     # unit tests (mocks, no network/models needed)
RUN_MODEL_TESTS=1 ../venv/bin/python manage.py test brain   # + real Piper->Whisper loopback
../venv/bin/gunicorn config.wsgi --bind 0.0.0.0:8000 --workers 1 --threads 4 --timeout 90
```

A systemd **user** unit is in `deploy/cerebro.service` (no sudo to install; `sudo loginctl
enable-linger $USER` once so it starts at boot). `curl http://SERVER:8000/api/health` shows what is
configured. If `python3-venv` is missing and you have no root: `curl -O https://bootstrap.pypa.io/virtualenv.pyz && python3 virtualenv.pyz venv`.
The server's firewall must allow tcp/8000 from the LAN (`sudo ufw allow from <lan-subnet> to any port 8000 proto tcp`).

## Try it without hardware

```bash
python manage.py ask "Hola Ultron, ¿quién eres?" --wav reply.wav   # text -> LLM -> TTS
python manage.py converse_wav utterance.pcm --out reply.wav        # full STT->LLM->TTS on an audio file
```

## API used by the ESP32 (plain HTTP, see `brain/views.py`)

| | |
|---|---|
| `POST /api/converse` | body = one utterance, raw PCM s16le mono 16 kHz (or WAV). Header `X-Session-Id`. Returns JSON: `transcript`, `behavior {text, face, movement, neck}`, `audio {url, bytes, sample_rate}`, `timings_ms`, or `{"ignored": true, "reason": ...}` |
| `GET /api/audio/<id>` | reply audio, raw PCM s16le mono 16 kHz, `Content-Length` set |
| `POST /api/converse_text` | `{"text": "..."}` → same JSON (no STT), for testing |
| `GET /api/health` | configured backends (never secrets) |

Why HTTP: the firmware already segments whole utterances (one request per utterance), `HTTPClient`
ships with the ESP32 core, and everything is curl-debuggable. WebSocket would only add streaming we
don't need yet.

## Swapping backends (settings only, no code changes)

- LLM: `LLM_PROVIDER=ollama LLM_MODEL=... ` (OpenAI-compatible `/v1/chat/completions`).
- STT model size: `STT_MODEL=tiny|base|small|medium` — pick by the server's CPU.
- TTS voice: `TTS_VOICE=<piper voice>`, then `fetch_models`. Voice effect: `TTS_FX=deep|monster|none`
  (`deep` = clean solemn Thanos-like, default), `TTS_PITCH_SEMITONES` (-3; -4 already costs clarity),
  `TTS_LENGTH_SCALE` (pace), `TTS_PRESENCE`; `monster` mode also uses `TTS_GROWL`/`TTS_DRIVE`.
- LLM rate limits: `LLM_FALLBACK_MODEL` is tried once on a 429 (Groq's free tier is 8000 tokens/min per model).

## Safety

- `ULTRON_ALLOW_MOVEMENT=0` (default): Django drops any movement the LLM asks for.
- The ESP32 has its own compile-time movement gate, and the Pico keeps final authority
  (watchdog, obstacle veto, STOP). See `docs/CURRENT_STATE.md`.
- No authentication: this is a LAN-only service. Do not expose it to the internet.
