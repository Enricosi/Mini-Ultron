import json

import numpy as np
from django.test import SimpleTestCase

from brain import audio_utils as au
from brain.llm import FakeLLM, LLMError
from brain.pipeline import History, Pipeline
from brain.schemas import Behavior
from brain.stt import FakeSTT
from brain.tts import FakeTTS, TTSBackend, TTSError


def behavior_json(**over):
    data = {"text": "Silencio, humano.", "face": "ANGRY", "movement": "NONE", "neck": "NONE"}
    data.update(over)
    return json.dumps(data)


def utterance(seconds=1.0):
    return au.float_to_pcm16(0.1 * np.sin(np.linspace(0, 200, int(16000 * seconds))).astype(np.float32))


def make(llm=None, stt=None, tts=None, **kw):
    return Pipeline(stt=stt or FakeSTT("hola ultron"), llm=llm or FakeLLM([behavior_json()]), tts=tts or FakeTTS(), **kw)


class PipelineTests(SimpleTestCase):
    def test_happy_path_audio_to_reply(self):
        p = make()
        turn = p.handle_audio(utterance())
        self.assertFalse(turn.ignored)
        self.assertEqual(turn.transcript, "hola ultron")
        self.assertEqual(turn.behavior.face, "ANGRY")
        self.assertIsNotNone(turn.audio_id)
        self.assertEqual(p.audio_store.get(turn.audio_id).sample_rate, 16000)
        for key in ("stt", "llm", "tts", "total"):
            self.assertIn(key, turn.timings_ms)

    def test_too_short_audio_is_ignored_without_calling_stt(self):
        stt = FakeSTT()
        turn = make(stt=stt).handle_audio(utterance(0.1))
        self.assertTrue(turn.ignored)
        self.assertEqual((turn.ignored_reason, stt.calls), ("too_short", 0))

    def test_bad_audio_is_ignored(self):
        turn = make().handle_audio(b"RIFFgarbage")
        self.assertTrue(turn.ignored)

    def test_hallucinated_transcript_is_ignored(self):
        llm = FakeLLM()
        turn = make(llm=llm, stt=FakeSTT("Subtítulos realizados por la comunidad de Amara.org")).handle_audio(utterance())
        self.assertTrue(turn.ignored)
        self.assertEqual(llm.calls, [])  # never reached the LLM

    def test_llm_failure_uses_fallback_with_glitch_face(self):
        turn = make(llm=FakeLLM([LLMError("down")])).handle_audio(utterance())
        self.assertEqual(turn.behavior.face, "GLITCH")
        self.assertTrue(any("fallback" in w for w in turn.warnings))
        self.assertIsNotNone(turn.audio_id)  # it still speaks

    def test_garbage_llm_output_uses_fallback(self):
        turn = make(llm=FakeLLM(["I am not JSON"])).handle_audio(utterance())
        self.assertEqual(turn.behavior.face, "GLITCH")

    def test_movement_dropped_by_default_and_reported(self):
        turn = make(llm=FakeLLM([behavior_json(movement="FORWARD")])).handle_audio(utterance())
        self.assertEqual(turn.behavior.movement, "NONE")
        self.assertTrue(any("dropped" in w for w in turn.warnings))

    def test_movement_passes_when_explicitly_allowed(self):
        turn = make(llm=FakeLLM([behavior_json(movement="ROTATE_LEFT")]), allow_movement=True).handle_audio(utterance())
        self.assertEqual(turn.behavior.movement, "ROTATE_LEFT")

    def test_tts_failure_still_returns_behavior(self):
        class BrokenTTS(TTSBackend):
            name = "broken"

            def synthesize(self, text):
                raise TTSError("no voice")

        turn = make(tts=BrokenTTS()).handle_audio(utterance())
        self.assertIsNone(turn.audio)
        self.assertEqual(turn.behavior.face, "ANGRY")

    def test_history_is_sent_to_the_llm_per_session_and_bounded(self):
        llm = FakeLLM()
        p = make(llm=llm, history=History(max_turns=2))
        for i in range(3):
            p.handle_text(f"mensaje {i}", session="a")
        p.handle_text("otra sesión", session="b")
        last_a = llm.calls[2]  # third call for session a
        contents = [m["content"] for m in last_a if m["role"] == "user"]
        self.assertEqual(contents, ["mensaje 0", "mensaje 1", "mensaje 2"])
        first_b = llm.calls[3]
        self.assertEqual([m["content"] for m in first_b if m["role"] == "user"], ["otra sesión"])
        # after the third turn only the last 2 are kept
        p.handle_text("mensaje 3", session="a")
        users = [m["content"] for m in llm.calls[4] if m["role"] == "user"]
        self.assertEqual(users, ["mensaje 1", "mensaje 2", "mensaje 3"])

    def test_fallback_turns_are_not_remembered(self):
        llm = FakeLLM([LLMError("down")])
        p = make(llm=llm)
        p.handle_text("primero", session="s")
        p.handle_text("segundo", session="s")
        users = [m["content"] for m in llm.calls[1] if m["role"] == "user"]
        self.assertEqual(users, ["segundo"])

    def test_empty_text_is_ignored(self):
        self.assertTrue(make().handle_text("   ").ignored)

    def test_known_event_reaches_the_llm_and_speaks(self):
        llm = FakeLLM()
        turn = make(llm=llm).handle_event("JOLT")
        self.assertFalse(turn.ignored)
        self.assertIsNotNone(turn.audio_id)
        msgs = llm.calls[0]
        self.assertIn("EVENTO FÍSICO", msgs[-1]["content"])

    def test_dance_order_plays_the_animation(self):
        llm = FakeLLM([behavior_json(animation="CALABAZA")])
        turn = make(llm=llm).handle_text("Ultron, baila la calabaza")
        self.assertEqual(turn.behavior.animation, "CALABAZA")

    def test_llm_cannot_dance_on_its_own(self):
        llm = FakeLLM([behavior_json(animation="CALABAZA")])
        turn = make(llm=llm).handle_text("hola ultron")
        self.assertEqual(turn.behavior.animation, "NONE")
        self.assertIn("animation CALABAZA dropped: no explicit dance order", turn.warnings)

    def test_events_never_dance(self):
        llm = FakeLLM([behavior_json(animation="CALABAZA")])
        self.assertEqual(make(llm=llm).handle_event("JOLT").behavior.animation, "NONE")

    def test_unknown_event_is_ignored_without_calling_the_llm(self):
        llm = FakeLLM()
        turn = make(llm=llm).handle_event("NOT_A_REAL_EVENT")
        self.assertEqual((turn.ignored, turn.ignored_reason, llm.calls), (True, "unknown_event", []))

    def test_prompt_contains_system_and_user_text(self):
        llm = FakeLLM()
        make(llm=llm).handle_text("hola")
        msgs = llm.calls[0]
        self.assertEqual((msgs[0]["role"], msgs[-1]["role"], msgs[-1]["content"]), ("system", "user", "hola"))
