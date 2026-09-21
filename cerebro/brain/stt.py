"""Speech-to-text. Interface: float32 mono 16 kHz audio in, Transcript out."""
import logging
import re
import threading
from dataclasses import dataclass

import numpy as np

logger = logging.getLogger("brain.stt")

# Whisper invents these when it is fed silence/noise (very common with Spanish).
_HALLUCINATIONS = (
    "subtítulos realizados por",
    "subtitulos realizados por",
    "amara.org",
    "gracias por ver",
    "suscríbete",
    "suscribete",
    "no olvides suscribirte",
    "¡gracias!",
    "gracias.",
)


def _plain(text: str) -> str:
    return re.sub(r"[\W_]+", " ", text.lower()).strip()


def is_prompt_echo(text: str, prompt: str) -> bool:
    """Whisper sometimes parrots (most of) its initial_prompt when it hears noise: drop those.
    Only LONG overlaps count: short real commands such as "Avanza" or "Gira a la izquierda"
    are substrings of the prompt and must never be dropped."""
    t, p = _plain(text), _plain(prompt)
    if not t or not p or len(t.split()) < 0.6 * len(p.split()):
        return False
    return t in p or p in t


def is_hallucination(text: str) -> bool:
    t = text.strip().lower()
    if len(re.sub(r"[\W_]+", "", t)) < 2:
        return True
    return any(t == h or h in t for h in _HALLUCINATIONS) and len(t) < 60


@dataclass
class Transcript:
    text: str
    language: str = ""
    audio_s: float = 0.0


class STTBackend:
    name = "base"

    def transcribe(self, audio: np.ndarray) -> Transcript:  # pragma: no cover - interface
        raise NotImplementedError

    def preload(self) -> None:
        pass


class FasterWhisperSTT(STTBackend):
    name = "faster_whisper"

    def __init__(
        self,
        model: str,
        language: str,
        compute_type: str,
        cpu_threads: int,
        download_root: str,
        initial_prompt: str = "",
        beam_size: int = 1,
    ):
        self.initial_prompt = initial_prompt
        self.model_name = model
        self.language = language or None
        self.compute_type = compute_type
        self.cpu_threads = cpu_threads
        self.download_root = download_root
        self.beam_size = beam_size  # 1 = greedy (fastest); higher trades latency for accuracy
        self._model = None
        self._lock = threading.Lock()

    def preload(self) -> None:
        self._load()

    def _load(self):
        if self._model is None:
            with self._lock:
                if self._model is None:
                    from faster_whisper import WhisperModel

                    logger.info("loading faster-whisper %s (%s)", self.model_name, self.compute_type)
                    self._model = WhisperModel(
                        self.model_name,
                        device="cpu",
                        compute_type=self.compute_type,
                        cpu_threads=self.cpu_threads,
                        download_root=self.download_root,
                    )
        return self._model

    def transcribe(self, audio: np.ndarray) -> Transcript:
        model = self._load()
        with self._lock:  # one inference at a time
            segments, info = model.transcribe(
                audio,
                language=self.language,
                beam_size=self.beam_size,
                vad_filter=True,
                vad_parameters={"min_silence_duration_ms": 500},
                condition_on_previous_text=False,
                initial_prompt=self.initial_prompt or None,
            )
            kept = [
                s.text.strip()
                for s in segments
                if not (s.no_speech_prob > 0.7 and s.avg_logprob < -1.0)
            ]
        text = " ".join(t for t in kept if t).strip()
        if self.initial_prompt and is_prompt_echo(text, self.initial_prompt):
            logger.info("dropping transcript that just echoes the STT prompt: %r", text)
            text = ""
        return Transcript(
            text=text,
            language=info.language,
            audio_s=len(audio) / 16000,
        )


class FakeSTT(STTBackend):
    name = "fake"

    def __init__(self, text: str = "hola mini ultron"):
        self.text = text
        self.calls = 0

    def transcribe(self, audio: np.ndarray) -> Transcript:
        self.calls += 1
        return Transcript(text=self.text, language="es", audio_s=len(audio) / 16000)
