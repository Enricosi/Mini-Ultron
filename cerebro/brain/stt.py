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

    def __init__(self, model: str, language: str, compute_type: str, cpu_threads: int, download_root: str):
        self.model_name = model
        self.language = language or None
        self.compute_type = compute_type
        self.cpu_threads = cpu_threads
        self.download_root = download_root
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
                beam_size=1,
                vad_filter=True,
                vad_parameters={"min_silence_duration_ms": 500},
                condition_on_previous_text=False,
            )
            kept = [
                s.text.strip()
                for s in segments
                if not (s.no_speech_prob > 0.7 and s.avg_logprob < -1.0)
            ]
        return Transcript(
            text=" ".join(t for t in kept if t).strip(),
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
