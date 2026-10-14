"""
Startup validation of the CEREBRO settings. Fails fast with every problem listed at once,
instead of surfacing as an odd error on the first request from the robot.
"""
import logging

from django.core.exceptions import ImproperlyConfigured

from .llm import PROVIDER_PRESETS
from .personalities import PERSONALITIES

logger = logging.getLogger("brain.config")

LLM_PROVIDERS = (*PROVIDER_PRESETS, "fake")
STRUCTURED_MODES = ("json_schema", "json_object", "none")
REASONING_EFFORTS = ("", "none", "minimal", "low", "medium", "high", "xhigh", "max")
STT_BACKENDS = ("faster_whisper", "fake")
TTS_BACKENDS = ("piper", "fake")
TTS_FX_MODES = ("deep", "monster", "none")
LOG_LEVELS = ("DEBUG", "INFO", "WARNING", "ERROR")


def api_key_for(provider: str, explicit: str, environ) -> str:
    """The key to use for a provider: the explicit override, else the provider's own variable."""
    if explicit:
        return explicit
    key_env = PROVIDER_PRESETS.get(provider, {}).get("key_env", "")
    return environ.get(key_env, "") if key_env else ""


def main_key(settings, environ) -> str:
    return api_key_for(settings.LLM_PROVIDER, settings.LLM_API_KEY, environ)


def fallback_key(settings, environ) -> str:
    # LLM_API_KEY belongs to the main provider: only reuse it for the same one.
    shared = settings.LLM_API_KEY if settings.LLM_FALLBACK_PROVIDER == settings.LLM_PROVIDER else ""
    return api_key_for(settings.LLM_FALLBACK_PROVIDER, settings.LLM_FALLBACK_API_KEY or shared, environ)


def _llm_problems(provider, model, base_url, label):
    problems = []
    if provider not in LLM_PROVIDERS:
        problems.append(f"{label}PROVIDER={provider!r} is not one of {list(LLM_PROVIDERS)}")
    elif provider == "openai_compat" and not base_url:
        problems.append(f"{label}BASE_URL is required when the provider is openai_compat")
    if provider != "fake" and not model:
        problems.append(f"{label}MODEL is empty")
    return problems


def validate(settings, environ) -> list[str]:
    """Return a list of human-readable problems (empty = config is usable)."""
    problems = _llm_problems(settings.LLM_PROVIDER, settings.LLM_MODEL, settings.LLM_BASE_URL, "LLM_")
    if settings.LLM_FALLBACK_MODEL:
        problems += _llm_problems(
            settings.LLM_FALLBACK_PROVIDER,
            settings.LLM_FALLBACK_MODEL,
            settings.LLM_FALLBACK_BASE_URL or settings.LLM_BASE_URL,
            "LLM_FALLBACK_",
        )
    if settings.LLM_TIMEOUT_S <= 0:
        problems.append("LLM_TIMEOUT_S must be > 0")
    if not 0 <= settings.LLM_TEMPERATURE <= 2:
        problems.append("LLM_TEMPERATURE must be between 0 and 2")
    if settings.LLM_MAX_TOKENS <= 0:
        problems.append("LLM_MAX_TOKENS must be > 0")
    if settings.LLM_STRUCTURED not in STRUCTURED_MODES:
        problems.append(f"LLM_STRUCTURED must be one of {list(STRUCTURED_MODES)}")
    if settings.LLM_REASONING_EFFORT not in REASONING_EFFORTS:
        problems.append(f"LLM_REASONING_EFFORT must be one of {list(REASONING_EFFORTS)}")
    if settings.LLM_FALLBACK_REASONING_EFFORT not in REASONING_EFFORTS:
        problems.append(f"LLM_FALLBACK_REASONING_EFFORT must be one of {list(REASONING_EFFORTS)}")
    if settings.STT_BACKEND not in STT_BACKENDS:
        problems.append(f"STT_BACKEND must be one of {list(STT_BACKENDS)}")
    if settings.TTS_BACKEND not in TTS_BACKENDS:
        problems.append(f"TTS_BACKEND must be one of {list(TTS_BACKENDS)}")
    if settings.TTS_FX not in TTS_FX_MODES:
        problems.append(f"TTS_FX must be one of {list(TTS_FX_MODES)}")
    if settings.ULTRON_PERSONALITY not in PERSONALITIES:
        problems.append(f"ULTRON_PERSONALITY={settings.ULTRON_PERSONALITY!r} is not one of {sorted(PERSONALITIES)}")
    if settings.ULTRON_HISTORY_TURNS < 0:
        problems.append("ULTRON_HISTORY_TURNS must be >= 0")
    if settings.ULTRON_MAX_REPLY_CHARS < 20:
        problems.append("ULTRON_MAX_REPLY_CHARS must be >= 20")
    level = settings.LOGGING["loggers"]["brain"]["level"]
    if level not in LOG_LEVELS:
        problems.append(f"ULTRON_LOG_LEVEL must be one of {list(LOG_LEVELS)}")
    return problems


def check_settings(settings, environ) -> None:
    """Raise ImproperlyConfigured on a bad config; log warnings for recoverable gaps."""
    problems = validate(settings, environ)
    if problems:
        raise ImproperlyConfigured("Invalid CEREBRO configuration:\n  - " + "\n  - ".join(problems))

    def has_key(provider, key):
        return provider == "fake" or not PROVIDER_PRESETS.get(provider, {}).get("key_env") or bool(key)

    main_ok = has_key(settings.LLM_PROVIDER, main_key(settings, environ))
    fallback_on = bool(settings.LLM_FALLBACK_MODEL)
    fallback_ok = fallback_on and has_key(settings.LLM_FALLBACK_PROVIDER, fallback_key(settings, environ))
    if not main_ok and not fallback_ok:
        key_env = PROVIDER_PRESETS[settings.LLM_PROVIDER]["key_env"]
        raise ImproperlyConfigured(
            f"No usable LLM credentials: set {key_env} (or LLM_API_KEY) for provider "
            f"{settings.LLM_PROVIDER!r}, or LLM_PROVIDER=fake for an offline run."
        )
    if not main_ok:
        logger.warning("main LLM %s has no API key; every turn will use the fallback", settings.LLM_PROVIDER)
    if fallback_on and not fallback_ok:
        logger.warning("LLM fallback %s has no API key; it will be skipped", settings.LLM_FALLBACK_PROVIDER)
    if settings.LLM_FALLBACK_MODEL == settings.LLM_MODEL and settings.LLM_FALLBACK_PROVIDER == settings.LLM_PROVIDER:
        logger.warning("LLM fallback is the same provider/model as the main LLM; it adds a retry only")
