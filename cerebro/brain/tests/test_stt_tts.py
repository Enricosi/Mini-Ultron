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


class VoiceFxTests(SimpleTestCase):
    def test_deepen_lowers_pitch_by_the_requested_interval(self):
        from brain import voice_fx

        sr = 16000
        t = np.arange(sr) / sr
        tone = np.sin(2 * np.pi * 400 * t).astype(np.float32)
        low = voice_fx.deepen(tone, -12)  # one octave down
        freq = np.argmax(np.abs(np.fft.rfft(low))) * sr / len(low)
        self.assertAlmostEqual(freq, 200, delta=3)
        self.assertAlmostEqual(len(low) / len(tone), 2.0, delta=0.01)

    def test_monster_is_finite_and_has_more_low_end(self):
        from brain import voice_fx

        sr = 16000
        rng = np.random.default_rng(1)
        speech_like = (np.sin(2 * np.pi * 220 * np.arange(sr) / sr) * rng.uniform(0.3, 1, sr)).astype(np.float32) * 0.3
        y = voice_fx.monster(speech_like, sr, semitones=-5, growl=0.3, drive=2.0)
        self.assertTrue(np.all(np.isfinite(y)))
        self.assertGreater(len(y), len(speech_like))  # slower after the pitch drop
        self.assertEqual(len(voice_fx.monster(np.zeros(0, dtype=np.float32), sr)), 0)

    def test_fx_none_and_pitch_ratio(self):
        from brain import voice_fx

        self.assertAlmostEqual(voice_fx.pitch_ratio(-12), 0.5)
        x = np.ones(100, dtype=np.float32)
        self.assertTrue(np.array_equal(voice_fx.deepen(x, 0), x))


class PromptEchoTests(SimpleTestCase):
    PROMPT = "Ultron, muévete. Avanza. Retrocede. Gira a la izquierda. Gira a la derecha. Detente."

    def test_long_echo_of_the_prompt_is_detected(self):
        from brain.stt import is_prompt_echo

        for echo in (self.PROMPT, "Ultron, muévete. Avanza. Retrocede. Gira a la izquierda. Gira a la derecha."):
            self.assertTrue(is_prompt_echo(echo, self.PROMPT), echo)

    def test_short_real_commands_are_never_dropped(self):
        from brain.stt import is_prompt_echo

        for cmd in ("Avanza.", "Detente", "Gira a la izquierda.", "Ultron, muévete.", "Retrocede", "Gira a la derecha"):
            self.assertFalse(is_prompt_echo(cmd, self.PROMPT), cmd)

    def test_real_speech_is_not_an_echo(self):
        from brain.stt import is_prompt_echo

        for text in ("Ultron, avanza un poco y luego retrocede", "Hola, ¿cómo estás?", ""):
            self.assertFalse(is_prompt_echo(text, self.PROMPT), text)
