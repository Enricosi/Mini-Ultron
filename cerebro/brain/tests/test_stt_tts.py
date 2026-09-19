import os
import unittest

import numpy as np
from django.conf import settings
from django.test import SimpleTestCase

from brain import audio_utils as au
from brain.stt import FakeSTT, is_hallucination
from brain.tts import FakeTTS, PiperTTS, TTSError

MODELS = os.environ.get("RUN_MODEL_TESTS") == "1"


class STTTests(SimpleTestCase):
    def test_fake_stt(self):
        stt = FakeSTT("hola")
        t = stt.transcribe(np.zeros(16000, dtype=np.float32))
        self.assertEqual((t.text, round(t.audio_s, 2)), ("hola", 1.0))

    def test_hallucination_filter(self):
        for text in ("", "  ", ".", "Subtítulos realizados por la comunidad de Amara.org", "Gracias por ver el video", "Suscríbete"):
            self.assertTrue(is_hallucination(text), text)
        for text in ("Hola Ultron", "Gira a la izquierda", "Sí"):
            self.assertFalse(is_hallucination(text), text)


class TTSTests(SimpleTestCase):
    def test_fake_tts_format(self):
        s = FakeTTS().synthesize("Hola mundo")
        self.assertEqual(s.sample_rate, 16000)
        self.assertEqual(len(s.pcm) % 2, 0)
        self.assertGreater(s.duration_s, 0)

    def test_piper_missing_voice_is_a_clear_error(self):
        tts = PiperTTS("does-not-exist", settings.ULTRON_DATA_DIR / "voices")
        with self.assertRaisesRegex(TTSError, "fetch_models"):
            tts.synthesize("hola")


@unittest.skipUnless(MODELS, "set RUN_MODEL_TESTS=1 to run the real Whisper/Piper tests")
class RealModelTests(SimpleTestCase):
    """Piper speaks a sentence, Whisper must hear it back. Proves both services locally."""

    def test_tts_then_stt_loopback(self):
        from brain import services

        speech = services.build_tts().synthesize("Quítate de mi camino, animal.")
        self.assertEqual(speech.sample_rate, 16000)
        self.assertGreater(speech.duration_s, 1.0)
        peak = np.max(np.abs(np.frombuffer(speech.pcm, dtype="<i2"))) / 32768
        self.assertAlmostEqual(peak, settings.TTS_PEAK, delta=0.02)

        quiet = au.pcm16_to_float(speech.pcm) * 0.08  # like the mic capture
        text = services.build_stt().transcribe(au.normalize_peak(quiet)).text.lower()
        self.assertIn("camino", text)
        self.assertIn("animal", text)
