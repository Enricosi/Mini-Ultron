"""
CEREBRO settings. Everything deployment-specific comes from environment variables
(or a .env file), so the same code runs on the dev laptop and on the home-lab mini
server without edits. See cerebro/.env.example for the full list.
"""
import os
from pathlib import Path

from django.core.exceptions import ImproperlyConfigured

BASE_DIR = Path(__file__).resolve().parent.parent  # .../cerebro
REPO_DIR = BASE_DIR.parent


def _load_dotenv(path: Path) -> None:
    """Tiny .env loader (KEY=VALUE lines). Real environment variables win."""
    if not path.is_file():
        return
    for line in path.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, _, value = line.partition("=")
        value = value.strip().strip('"').strip("'")
        if value:  # an empty value means "not set"
            os.environ.setdefault(key.strip(), value)


_load_dotenv(BASE_DIR / ".env")
_load_dotenv(REPO_DIR / ".env")


def env(name: str, default: str = "") -> str:
    return os.environ.get(name, default)


def env_bool(name: str, default: bool = False) -> bool:
    return env(name, "1" if default else "0").strip().lower() in ("1", "true", "yes", "on")


def env_int(name: str, default: int) -> int:
    try:
        return int(env(name, str(default)))
    except ValueError:
        raise ImproperlyConfigured(f"{name} must be an integer, got {env(name)!r}") from None


def env_float(name: str, default: float) -> float:
    try:
        return float(env(name, str(default)))
    except ValueError:
        raise ImproperlyConfigured(f"{name} must be a number, got {env(name)!r}") from None


# --- Django basics -----------------------------------------------------------
SECRET_KEY = env("DJANGO_SECRET_KEY", "dev-only-insecure-key-change-on-the-server")
DEBUG = env_bool("DJANGO_DEBUG", False)
# The robot reaches this server over the LAN by IP/hostname, so accept any Host header.
# CEREBRO is a LAN-only service; do not expose it to the internet as-is.
ALLOWED_HOSTS = [h.strip() for h in env("DJANGO_ALLOWED_HOSTS", "*").split(",") if h.strip()]

INSTALLED_APPS = ["brain"]
MIDDLEWARE = ["django.middleware.common.CommonMiddleware"]
ROOT_URLCONF = "config.urls"
WSGI_APPLICATION = "config.wsgi.application"
ASGI_APPLICATION = "config.asgi.application"
TEMPLATES = []
DATABASES = {}  # v0.1 keeps no persistent state (conversation history is in memory)
DEFAULT_AUTO_FIELD = "django.db.models.BigAutoField"
USE_TZ = True
LANGUAGE_CODE = "es"
DATA_UPLOAD_MAX_MEMORY_SIZE = 4 * 1024 * 1024  # a 12 s utterance is ~384 KB

# --- Data dir (models, voices, debug audio). Point it at a big disk on the server. ---
ULTRON_DATA_DIR = Path(env("ULTRON_DATA_DIR", str(BASE_DIR / "var")))
# Hugging Face cache (faster-whisper models) lives inside the data dir too.
os.environ.setdefault("HF_HOME", str(ULTRON_DATA_DIR / "hf"))

# --- LLM ---------------------------------------------------------------------
# Provider presets (brain/llm.py): "groq" (default), "meta" (Muse Spark), "ollama",
# "openai_compat" (any OpenAI-style endpoint, needs LLM_BASE_URL), "fake" (offline, tests/dev).
# Each provider reads its own key variable (GROQ_API_KEY, MODEL_API_KEY for meta);
# LLM_API_KEY overrides it. See docs/providers.md.
LLM_PROVIDER = env("LLM_PROVIDER", "groq")
LLM_MODEL = env("LLM_MODEL", "openai/gpt-oss-120b")
LLM_BASE_URL = env("LLM_BASE_URL", "")  # empty = provider default
LLM_API_KEY = env("LLM_API_KEY", "")
# One explicit fallback, tried when the main LLM fails (timeout, 429, HTTP error, invalid
# reply, missing key). Empty model = no fallback. Provider defaults to the main one; the
# default model only applies to groq (a gpt-oss id means nothing to other providers).
LLM_FALLBACK_PROVIDER = env("LLM_FALLBACK_PROVIDER", "") or LLM_PROVIDER
LLM_FALLBACK_MODEL = env(
    "LLM_FALLBACK_MODEL", "openai/gpt-oss-20b" if LLM_FALLBACK_PROVIDER == "groq" else ""
)
LLM_FALLBACK_BASE_URL = env("LLM_FALLBACK_BASE_URL", "")
LLM_FALLBACK_API_KEY = env("LLM_FALLBACK_API_KEY", "")
LLM_TIMEOUT_S = env_float("LLM_TIMEOUT_S", 20.0)
LLM_TEMPERATURE = env_float("LLM_TEMPERATURE", 0.8)
LLM_MAX_TOKENS = env_int("LLM_MAX_TOKENS", 400)
# json_schema (strict constrained decoding) | json_object | none
LLM_STRUCTURED = env("LLM_STRUCTURED", "json_schema")
LLM_REASONING_EFFORT = env("LLM_REASONING_EFFORT", "low")  # gpt-oss models only

# --- STT (faster-whisper, local) ----------------------------------------------
STT_BACKEND = env("STT_BACKEND", "faster_whisper")  # or "fake"
STT_MODEL = env("STT_MODEL", "small")  # tiny/base/small/medium/large-v3...
STT_LANGUAGE = env("STT_LANGUAGE", "es")  # empty = auto-detect
STT_COMPUTE_TYPE = env("STT_COMPUTE_TYPE", "int8")
STT_CPU_THREADS = env_int("STT_CPU_THREADS", 0)  # 0 = library default
# 1 = greedy decoding (fastest); raising it trades latency for transcription accuracy.
# 5 measured ~30-80ms slower than 1 on the home-lab server's CPU (noise-level against the
# ~1s STT budget) but fixed a real mishearing ("Múdete" -> "Muévete") that beam=1 didn't.
STT_BEAM_SIZE = env_int("STT_BEAM_SIZE", 5)
# Biases Whisper toward the robot's command words (a real test misheard "muévete" as "muera").
# Empty = off. Transcripts that merely echo this prompt are dropped.
STT_INITIAL_PROMPT = env(
    "STT_INITIAL_PROMPT",
    "Ultron, muévete. Avanza. Retrocede. Gira a la izquierda. Gira a la derecha. Detente.",
)

# --- TTS (Piper, local) -----------------------------------------------------------
TTS_BACKEND = env("TTS_BACKEND", "piper")  # or "fake"
TTS_VOICE = env("TTS_VOICE", "es_ES-davefx-medium")
TTS_LENGTH_SCALE = env_float("TTS_LENGTH_SCALE", 1.05)  # >1 = slower, more solemn
TTS_PEAK = env_float("TTS_PEAK", 0.35)  # output loudness, fraction of full scale
# Voice effect (voice_fx.py): "deep" = clean, solemn Thanos-like; "monster" = gritty; "none" = plain Piper.
TTS_FX = env("TTS_FX", "deep")
TTS_PITCH_SEMITONES = env_float("TTS_PITCH_SEMITONES", -3.0)  # more negative = deeper
TTS_GROWL = env_float("TTS_GROWL", 0.0)  # 0..0.6 roughness
TTS_DRIVE = env_float("TTS_DRIVE", 0.0)  # 0 = clean, higher = more distortion (>1 hurts clarity)
TTS_LOW_BOOST = env_float("TTS_LOW_BOOST", 0.3)  # extra bass body
TTS_PRESENCE = env_float("TTS_PRESENCE", 0.5)  # >1.5 kHz clarity; raise it if consonants are muddy

# --- Behavior policy -----------------------------------------------------------------
# Movement is OFF until you turn it on: the LLM may *ask* for a movement, but Django
# drops it (movement=NONE) unless this is true. Turn on only when the robot is safe to move.
ULTRON_ALLOW_MOVEMENT = env_bool("ULTRON_ALLOW_MOVEMENT", False)
ULTRON_PERSONALITY = env("ULTRON_PERSONALITY", "ultron")  # see brain/personalities/
ULTRON_HISTORY_TURNS = env_int("ULTRON_HISTORY_TURNS", 4)  # fewer turns = fewer tokens/minute
ULTRON_MAX_REPLY_CHARS = env_int("ULTRON_MAX_REPLY_CHARS", 300)
ULTRON_MAX_BODY_BYTES = env_int("ULTRON_MAX_BODY_BYTES", 2 * 1024 * 1024)  # ~65 s of 16 kHz audio; firmware caps utterances at 12 s
ULTRON_MIN_AUDIO_S = env_float("ULTRON_MIN_AUDIO_S", 0.3)  # shorter than this is a click, not speech
ULTRON_SAVE_DEBUG_AUDIO = env_bool("ULTRON_SAVE_DEBUG_AUDIO", True)
ULTRON_PRELOAD = env_bool("ULTRON_PRELOAD", True)  # load STT/TTS models at server start

LOGGING = {
    "version": 1,
    "disable_existing_loggers": False,
    "formatters": {"plain": {"format": "%(asctime)s %(levelname)s %(name)s: %(message)s"}},
    "handlers": {"console": {"class": "logging.StreamHandler", "formatter": "plain"}},
    "loggers": {
        "brain": {"handlers": ["console"], "level": env("ULTRON_LOG_LEVEL", "INFO").upper(), "propagate": False},
        "django.request": {"handlers": ["console"], "level": "WARNING", "propagate": False},
    },
}
