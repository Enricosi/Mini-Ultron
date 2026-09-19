"""Text-to-speech. Output is always PCM16 mono at 16 kHz: the exact format the ESP32
plays through its I2S port, so the firmware never has to convert anything."""
import logging
import threading
from dataclasses import dataclass
from pathlib import Path

import numpy as np

from . import voice_fx
from .audio_utils import SAMPLE_RATE, float_to_pcm16, resample

logger = logging.getLogger("brain.tts")


class TTSError(RuntimeError):
    pass


@dataclass
class Speech:
    pcm: bytes  # PCM16LE mono
    sample_rate: int = SAMPLE_RATE

    @property
    def duration_s(self) -> float:
        return len(self.pcm) / 2 / self.sample_rate


class TTSBackend:
    name = "base"

    def synthesize(self, text: str) -> Speech:  # pragma: no cover - interface
        raise NotImplementedError

    def preload(self) -> None:
        pass


class PiperTTS(TTSBackend):
    name = "piper"

    def __init__(
        self,
        voice: str,
        voices_dir: Path,
        length_scale: float = 1.0,
        peak: float = 0.35,
        fx: str = "none",
        pitch_semitones: float = -5.0,
        growl: float = 0.3,
        drive: float = 2.0,
    ):
        self.voice_name = voice
        self.fx = fx  # "monster" | "none"
        self.pitch_semitones = pitch_semitones
        self.growl = growl
        self.drive = drive
        self.model_path = Path(voices_dir) / f"{voice}.onnx"
        self.length_scale = length_scale
        self.peak = peak
        self._voice = None
        self._lock = threading.Lock()

    def preload(self) -> None:
        self._load()

    def _load(self):
        if self._voice is None:
            with self._lock:
                if self._voice is None:
                    if not self.model_path.is_file():
                        raise TTSError(
                            f"Piper voice not found: {self.model_path} "
                            f"(run: python manage.py fetch_models)"
                        )
                    from piper import PiperVoice

                    logger.info("loading Piper voice %s", self.voice_name)
                    self._voice = PiperVoice.load(str(self.model_path))
        return self._voice

    def synthesize(self, text: str) -> Speech:
        from piper.config import SynthesisConfig

        voice = self._load()
        length_scale = self.length_scale
        if self.fx == "monster":  # the pitch drop slows speech down; pre-compensate
            length_scale *= voice_fx.pitch_ratio(self.pitch_semitones)
        config = SynthesisConfig(length_scale=length_scale)
        with self._lock:  # one synthesis at a time
            chunks = list(voice.synthesize(text, syn_config=config))
        if not chunks:
            raise TTSError("Piper produced no audio")
        source_rate = chunks[0].sample_rate
        audio = np.concatenate([c.audio_float_array for c in chunks]).astype(np.float32)
        audio = resample(audio, source_rate, SAMPLE_RATE)
        if self.fx == "monster":
            audio = voice_fx.monster(audio, SAMPLE_RATE, self.pitch_semitones, self.growl, self.drive)
        peak = float(np.max(np.abs(audio))) if len(audio) else 0.0
        if peak > 0:
            audio = audio * (self.peak / peak)  # fixed loudness, independent of the voice
        return Speech(pcm=float_to_pcm16(audio))


class FakeTTS(TTSBackend):
    """A short tone whose length follows the text. For tests and offline development."""

    name = "fake"

    def __init__(self):
        self.calls: list[str] = []

    def synthesize(self, text: str) -> Speech:
        self.calls.append(text)
        seconds = min(0.05 * max(len(text), 1), 3.0)
        t = np.arange(int(SAMPLE_RATE * seconds)) / SAMPLE_RATE
        return Speech(pcm=float_to_pcm16(0.2 * np.sin(2 * np.pi * 220 * t).astype(np.float32)))
