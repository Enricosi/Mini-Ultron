import io
import wave

import numpy as np
from django.test import SimpleTestCase

from brain import audio_utils as au


class AudioUtilsTests(SimpleTestCase):
    def test_pcm_roundtrip(self):
        x = np.array([0.0, 0.5, -0.5, 1.0, -1.0], dtype=np.float32)
        back = au.pcm16_to_float(au.float_to_pcm16(x))
        self.assertTrue(np.allclose(back, x, atol=1e-3))

    def test_odd_byte_count_is_tolerated(self):
        self.assertEqual(len(au.pcm16_to_float(b"\x00\x01\x02")), 1)

    def test_resample_length_and_tone_frequency(self):
        sr_in, sr_out = 22050, 16000
        t = np.arange(sr_in) / sr_in
        y = au.resample(np.sin(2 * np.pi * 440 * t).astype(np.float32), sr_in, sr_out)
        self.assertAlmostEqual(len(y), sr_out, delta=2)
        peak_bin = np.argmax(np.abs(np.fft.rfft(y)))
        self.assertAlmostEqual(peak_bin * sr_out / len(y), 440, delta=2)

    def test_decode_upload_raw_and_wav(self):
        pcm = au.float_to_pcm16(np.linspace(-0.5, 0.5, 1600, dtype=np.float32))
        self.assertEqual(len(au.decode_upload(pcm)), 1600)
        wav = au.wav_bytes(pcm)
        self.assertEqual(len(au.decode_upload(wav)), 1600)

    def test_decode_upload_resamples_and_downmixes_stereo_wav(self):
        buf = io.BytesIO()
        with wave.open(buf, "wb") as w:
            w.setnchannels(2)
            w.setsampwidth(2)
            w.setframerate(32000)
            w.writeframes(np.zeros(32000 * 2, dtype="<i2").tobytes())
        out = au.decode_upload(buf.getvalue())
        self.assertAlmostEqual(len(out), 16000, delta=2)

    def test_normalize_peak_boosts_quiet_audio_but_not_silence(self):
        quiet = np.full(100, 0.02, dtype=np.float32)
        self.assertAlmostEqual(float(np.max(au.normalize_peak(quiet))), 0.8, places=3)
        silence = np.zeros(100, dtype=np.float32)
        self.assertEqual(float(np.max(au.normalize_peak(silence))), 0.0)
