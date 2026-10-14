"""Builds the Pipeline from Django settings (once), so backends stay swappable by config."""
import atexit
import logging
import os
import threading

from django.conf import settings

from . import checks
from .llm import FakeLLM, OpenAICompatLLM
from .personalities import get_personality
from .pipeline import AudioStore, History, Pipeline
from .stt import FakeSTT, FasterWhisperSTT
from .tts import FakeTTS, PiperTTS

logger = logging.getLogger("brain.services")

_pipeline: Pipeline | None = None
_lock = threading.Lock()


def build_llm():
    if settings.LLM_PROVIDER == "fake":
        return FakeLLM()
    return _openai_compat(
        settings.LLM_PROVIDER,
        settings.LLM_MODEL,
        checks.main_key(settings, os.environ),
        settings.LLM_BASE_URL,
        settings.LLM_REASONING_EFFORT,
    )


def build_fallback_llm():
    """The one explicitly configured fallback LLM, or None."""
    if not settings.LLM_FALLBACK_MODEL:
        return None
    if settings.LLM_FALLBACK_PROVIDER == "fake":
        return FakeLLM()
    return _openai_compat(
        settings.LLM_FALLBACK_PROVIDER,
        settings.LLM_FALLBACK_MODEL,
        checks.fallback_key(settings, os.environ),
        settings.LLM_FALLBACK_BASE_URL or settings.LLM_BASE_URL,
        settings.LLM_FALLBACK_REASONING_EFFORT,
    )


def _openai_compat(provider, model, api_key, base_url, reasoning_effort):
    return OpenAICompatLLM(
        provider=provider,
        model=model,
        api_key=api_key,
        base_url=base_url,
        timeout_s=settings.LLM_TIMEOUT_S,
        temperature=settings.LLM_TEMPERATURE,
        max_tokens=settings.LLM_MAX_TOKENS,
        structured=settings.LLM_STRUCTURED,
        reasoning_effort=reasoning_effort,
    )


def build_stt():
    if settings.STT_BACKEND == "fake":
        return FakeSTT()
    return FasterWhisperSTT(
        model=settings.STT_MODEL,
        language=settings.STT_LANGUAGE,
        compute_type=settings.STT_COMPUTE_TYPE,
        cpu_threads=settings.STT_CPU_THREADS,
        download_root=str(settings.ULTRON_DATA_DIR / "whisper"),
        initial_prompt=settings.STT_INITIAL_PROMPT,
        beam_size=settings.STT_BEAM_SIZE,
    )


def build_tts():
    if settings.TTS_BACKEND == "fake":
        return FakeTTS()
    return PiperTTS(
        voice=settings.TTS_VOICE,
        voices_dir=settings.ULTRON_DATA_DIR / "voices",
        length_scale=settings.TTS_LENGTH_SCALE,
        peak=settings.TTS_PEAK,
        fx=settings.TTS_FX,
        pitch_semitones=settings.TTS_PITCH_SEMITONES,
        growl=settings.TTS_GROWL,
        drive=settings.TTS_DRIVE,
        low_boost=settings.TTS_LOW_BOOST,
        presence=settings.TTS_PRESENCE,
    )


def build_personality():
    return get_personality(settings.ULTRON_PERSONALITY)


def get_pipeline() -> Pipeline:
    global _pipeline
    if _pipeline is None:
        with _lock:
            if _pipeline is None:
                checks.check_settings(settings, os.environ)
                _pipeline = Pipeline(
                    stt=build_stt(),
                    llm=build_llm(),
                    fallback_llm=build_fallback_llm(),
                    tts=build_tts(),
                    history=History(settings.ULTRON_HISTORY_TURNS),
                    audio_store=AudioStore(),
                    allow_movement=settings.ULTRON_ALLOW_MOVEMENT,
                    max_reply_chars=settings.ULTRON_MAX_REPLY_CHARS,
                    min_audio_s=settings.ULTRON_MIN_AUDIO_S,
                    personality=build_personality(),
                )
    return _pipeline


def shutdown() -> None:
    """Release the HTTP clients. Registered atexit so gunicorn/management commands exit cleanly."""
    if _pipeline is not None:
        for llm in (_pipeline.llm, _pipeline.fallback_llm):
            if llm is not None:
                llm.close()


atexit.register(shutdown)


def set_pipeline(pipeline: Pipeline | None) -> None:
    """Tests inject a pipeline built from fakes (or None to reset)."""
    global _pipeline
    _pipeline = pipeline


def preload() -> None:
    """Load the heavy models now instead of on the first request."""
    pipeline = get_pipeline()
    for backend in (pipeline.stt, pipeline.tts):
        try:
            backend.preload()
        except Exception:  # keep the server up; the error resurfaces per request
            logger.exception("preload failed for %s", backend.name)
    logger.info(
        "CEREBRO ready: llm=%s fallback=%s stt=%s/%s tts=%s/%s movement=%s personality=%s",
        pipeline.llm.label,
        pipeline.fallback_llm.label if pipeline.fallback_llm else "none",
        pipeline.stt.name,
        getattr(pipeline.stt, "model_name", "-"),
        pipeline.tts.name,
        getattr(pipeline.tts, "voice_name", "-"),
        "ALLOWED" if pipeline.allow_movement else "off",
        pipeline.personality.name,
    )
