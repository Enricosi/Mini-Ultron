"""
Monster voice: numpy-only effects applied to Piper's output (no extra dependencies).

deepen()  lowers the pitch by resampling (the voice also gets slower); PiperTTS
          compensates by asking Piper to talk faster (length_scale = pitch ratio),
          so the final speed stays roughly natural.
monster() = deepen + a rough "growl" (amplitude modulation), saturation, a short
          slap-back for thickness and a low-end boost.
Everything is tunable from settings (TTS_FX, TTS_PITCH_SEMITONES, TTS_GROWL, TTS_DRIVE).
"""
import numpy as np


def pitch_ratio(semitones: float) -> float:
    return 2.0 ** (semitones / 12.0)


def deepen(x: np.ndarray, semitones: float) -> np.ndarray:
    """Lower the pitch by `semitones` (negative). Output is longer by 1/ratio."""
    ratio = pitch_ratio(semitones)
    if ratio == 1.0 or len(x) == 0:
        return x
    t = np.arange(int(len(x) / ratio)) * ratio
    return np.interp(t, np.arange(len(x)), x).astype(np.float32)


def _lowpass(x: np.ndarray, sr: int, cutoff_hz: float, taps: int = 129) -> np.ndarray:
    fc = cutoff_hz / sr
    k = np.arange(taps) - (taps - 1) / 2
    h = 2 * fc * np.sinc(2 * fc * k) * np.hamming(taps)
    h /= h.sum()
    return np.convolve(x, h, mode="same").astype(np.float32)


def monster(x: np.ndarray, sr: int, semitones: float = -5.0, growl: float = 0.3, drive: float = 2.0) -> np.ndarray:
    """Deep, gravelly voice. `x` is float32 mono; returns float32 (not normalized)."""
    y = deepen(x, semitones)
    if len(y) == 0:
        return y
    if growl > 0:  # roughness: fast amplitude wobble around 65 Hz
        n = np.arange(len(y)) / sr
        y = y * (1.0 - growl + growl * (0.5 + 0.5 * np.sin(2 * np.pi * 65.0 * n)))
    if drive > 0:  # saturation
        y = np.tanh(drive * y) / np.tanh(drive)
    delay = int(0.012 * sr)  # 12 ms slap-back for body
    thick = y.copy()
    thick[delay:] += 0.35 * y[:-delay]
    y = thick + 0.6 * _lowpass(thick, sr, 280.0)  # low-end boost
    return y.astype(np.float32)
