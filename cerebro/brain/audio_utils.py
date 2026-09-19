"""PCM/WAV helpers. Wire format between ESP32 and CEREBRO: raw PCM, signed 16-bit
little-endian, mono, 16 kHz (the ESP32's I2S rate), in both directions."""
import io
import wave

import numpy as np

SAMPLE_RATE = 16000


def pcm16_to_float(pcm: bytes) -> np.ndarray:
    if len(pcm) % 2:
        pcm = pcm[:-1]
    return np.frombuffer(pcm, dtype="<i2").astype(np.float32) / 32768.0


def float_to_pcm16(x: np.ndarray) -> bytes:
    return (np.clip(x, -1.0, 1.0) * 32767.0).astype("<i2").tobytes()


def resample(x: np.ndarray, sr_in: int, sr_out: int) -> np.ndarray:
    """Windowed-sinc low-pass (when downsampling) + linear interpolation. numpy only."""
    if sr_in == sr_out or len(x) == 0:
        return x
    if sr_out < sr_in:
        cutoff = 0.9 * sr_out / sr_in  # fraction of the input Nyquist
        taps = 101
        k = np.arange(taps) - (taps - 1) / 2
        h = cutoff * np.sinc(cutoff * k) * np.kaiser(taps, 8.6)
        h /= h.sum()
        x = np.convolve(x, h, mode="same")
    n_out = int(len(x) * sr_out / sr_in)
    t = np.arange(n_out) * (sr_in / sr_out)
    return np.interp(t, np.arange(len(x)), x).astype(np.float32)


def decode_upload(body: bytes) -> np.ndarray:
    """Request body -> float32 mono 16 kHz. Accepts a WAV file or raw PCM16LE @16 kHz."""
    if body[:4] == b"RIFF":
        try:
            with wave.open(io.BytesIO(body), "rb") as w:
                if w.getsampwidth() != 2:
                    raise ValueError("only 16-bit WAV is supported")
                rate, channels = w.getframerate(), w.getnchannels()
                x = pcm16_to_float(w.readframes(w.getnframes()))
        except (wave.Error, EOFError) as exc:
            raise ValueError(f"invalid WAV: {exc}") from None
        if channels > 1:
            x = x[: len(x) // channels * channels].reshape(-1, channels).mean(axis=1)
        return resample(x, rate, SAMPLE_RATE)
    return pcm16_to_float(body)


def normalize_peak(x: np.ndarray, target: float = 0.8, max_gain: float = 40.0) -> np.ndarray:
    """Scale to a target peak. The ESP32 capture is quiet (raw >> 16); Whisper likes it loud."""
    if len(x) == 0:
        return x
    peak = float(np.max(np.abs(x)))
    if peak < 1e-4:
        return x
    return x * min(target / peak, max_gain)


def wav_bytes(pcm: bytes, sample_rate: int = SAMPLE_RATE) -> bytes:
    buf = io.BytesIO()
    with wave.open(buf, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(sample_rate)
        w.writeframes(pcm)
    return buf.getvalue()


def duration_s(pcm: bytes, sample_rate: int = SAMPLE_RATE) -> float:
    return len(pcm) / 2 / sample_rate
