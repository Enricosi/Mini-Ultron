"""Builds the Pipeline from Django settings (once), so backends stay swappable by config."""
import logging
import threading

from django.conf import settings

from .llm import FakeLLM, OpenAICompatLLM
from .pipeline import AudioStore, History, Pipeline
from .stt import FakeSTT, FasterWhisperSTT
from .tts import FakeTTS, PiperTTS

logger = logging.getLogger("brain.services")

_pipeline: Pipeline | None = None
_lock = threading.Lock()


def build_llm():
    if settings.LLM_PROVIDER == "fake":
        return FakeLLM()
    return OpenAICompatLLM(
        provider=settings.LLM_PROVIDER,
        model=settings.LLM_MODEL,
        api_key=settings.LLM_API_KEY,
        base_url=settings.LLM_BASE_URL,
        timeout_s=settings.LLM_TIMEOUT_S,
        temperature=settings.LLM_TEMPERATURE,
        max_tokens=settings.LLM_MAX_TOKENS,
        structured=settings.LLM_STRUCTURED,
        reasoning_effort=settings.LLM_REASONING_EFFORT,
        fallback_model=settings.LLM_FALLBACK_MODEL,
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


def get_pipeline() -> Pipeline:
    global _pipeline
    if _pipeline is None:
        with _lock:
            if _pipeline is None:
                _pipeline = Pipeline(
                    stt=build_stt(),
                    llm=build_llm(),
                    tts=build_tts(),
                    history=History(settings.ULTRON_HISTORY_TURNS),
                    audio_store=AudioStore(),
                    allow_movement=settings.ULTRON_ALLOW_MOVEMENT,
                    max_reply_chars=settings.ULTRON_MAX_REPLY_CHARS,
                )
    return _pipeline


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
        "CEREBRO ready: llm=%s/%s stt=%s/%s tts=%s/%s movement=%s",
        pipeline.llm.name,
        getattr(pipeline.llm, "model", "-"),
        pipeline.stt.name,
        getattr(pipeline.stt, "model_name", "-"),
        pipeline.tts.name,
        getattr(pipeline.tts, "voice_name", "-"),
        "ALLOWED" if pipeline.allow_movement else "off",
    )
