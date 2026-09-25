import json

import numpy as np
from django.test import Client, SimpleTestCase, override_settings

from brain import audio_utils as au
from brain import services
from brain.llm import FakeLLM
from brain.pipeline import Pipeline
from brain.stt import FakeSTT
from brain.tts import FakeTTS


def reply(**over):
    data = {"text": "Silencio, humano.", "face": "SUSPICIOUS", "movement": "NONE", "neck": "LEFT"}
    data.update(over)
    return json.dumps(data)


def pcm(seconds=1.0):
    return au.float_to_pcm16(0.1 * np.sin(np.linspace(0, 300, int(16000 * seconds))).astype(np.float32))


@override_settings(ULTRON_SAVE_DEBUG_AUDIO=False)
class ApiTests(SimpleTestCase):
    def setUp(self):
        self.pipeline = Pipeline(FakeSTT("hola"), FakeLLM([reply()]), FakeTTS())
        services.set_pipeline(self.pipeline)
        self.client = Client()

    def tearDown(self):
        services.set_pipeline(None)

    def test_converse_returns_behavior_and_audio_url_then_audio_downloads(self):
        r = self.client.post("/api/converse", data=pcm(), content_type="application/octet-stream", headers={"X-Session-Id": "esp32"})
        self.assertEqual(r.status_code, 200)
        data = r.json()
        self.assertTrue(data["ok"] and not data["ignored"])
        self.assertEqual(data["behavior"], {"text": "Silencio, humano.", "face": "SUSPICIOUS", "movement": "NONE", "neck": "LEFT"})
        self.assertEqual(data["audio"]["format"], "pcm_s16le_mono")
        self.assertEqual(data["audio"]["sample_rate"], 16000)
        self.assertIn("stt", data["timings_ms"])

        audio = self.client.get(data["audio"]["url"])
        self.assertEqual(audio.status_code, 200)
        self.assertEqual(len(audio.content), data["audio"]["bytes"])
        self.assertEqual(audio["Content-Length"], str(data["audio"]["bytes"]))
        self.assertEqual(audio["X-Sample-Rate"], "16000")

    def test_converse_accepts_wav_uploads(self):
        r = self.client.post("/api/converse", data=au.wav_bytes(pcm()), content_type="audio/wav")
        self.assertEqual(r.status_code, 200)
        self.assertFalse(r.json()["ignored"])

    def test_short_audio_is_ignored_with_reason(self):
        r = self.client.post("/api/converse", data=pcm(0.1), content_type="application/octet-stream")
        data = r.json()
        self.assertEqual((r.status_code, data["ignored"], data["reason"]), (200, True, "too_short"))
        self.assertNotIn("behavior", data)

    def test_oversized_body_rejected(self):
        r = self.client.post("/api/converse", data=b"\x00" * (2 * 1024 * 1024 + 2), content_type="application/octet-stream")
        self.assertEqual(r.status_code, 413)

    def test_converse_text(self):
        r = self.client.post("/api/converse_text", data=json.dumps({"text": "hola"}), content_type="application/json")
        self.assertEqual(r.json()["transcript"], "hola")
        self.assertEqual(r.json()["behavior"]["face"], "SUSPICIOUS")
        bad = self.client.post("/api/converse_text", data="{not json", content_type="application/json")
        self.assertEqual(bad.status_code, 400)

    def test_event(self):
        r = self.client.post("/api/event", data=json.dumps({"event": "jolt"}), content_type="application/json")
        self.assertEqual(r.json()["transcript"], "[event:JOLT]")
        self.assertFalse(r.json()["ignored"])
        bad = self.client.post("/api/event", data=json.dumps({"event": "NOPE"}), content_type="application/json")
        self.assertEqual((bad.json()["ignored"], bad.json()["reason"]), (True, "unknown_event"))
        malformed = self.client.post("/api/event", data="{not json", content_type="application/json")
        self.assertEqual(malformed.status_code, 400)

    def test_unknown_audio_id_404(self):
        self.assertEqual(self.client.get("/api/audio/nope").status_code, 404)

    def test_methods_are_enforced(self):
        self.assertEqual(self.client.get("/api/converse").status_code, 405)
        self.assertEqual(self.client.post("/api/health").status_code, 405)

    def test_health_reports_config_but_no_secrets(self):
        data = self.client.get("/api/health").json()
        self.assertEqual(data["stt"]["backend"], "fake")
        self.assertFalse(data["movement_allowed"])
        self.assertNotIn("key", json.dumps({k: v for k, v in data.items() if k != "llm"}))
        self.assertNotIn("api_key", json.dumps(data))

    def test_unexpected_error_returns_500_json(self):
        class Boom(FakeSTT):
            def transcribe(self, audio):
                raise RuntimeError("kaboom")

        services.set_pipeline(Pipeline(Boom(), FakeLLM(), FakeTTS()))
        r = self.client.post("/api/converse", data=pcm(), content_type="application/octet-stream")
        self.assertEqual((r.status_code, r.json()["ok"]), (500, False))
