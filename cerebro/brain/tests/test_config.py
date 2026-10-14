from types import SimpleNamespace

from django.core.exceptions import ImproperlyConfigured
from django.test import SimpleTestCase

from brain import checks


def cfg(**over):
    base = dict(
        LLM_PROVIDER="groq", LLM_MODEL="openai/gpt-oss-120b", LLM_BASE_URL="", LLM_API_KEY="",
        LLM_FALLBACK_PROVIDER="groq", LLM_FALLBACK_MODEL="openai/gpt-oss-20b",
        LLM_FALLBACK_BASE_URL="", LLM_FALLBACK_API_KEY="",
        LLM_TIMEOUT_S=20.0, LLM_TEMPERATURE=0.8, LLM_MAX_TOKENS=400, LLM_STRUCTURED="json_schema",
        LLM_REASONING_EFFORT="low", LLM_FALLBACK_REASONING_EFFORT="low", STT_BACKEND="fake", TTS_BACKEND="fake", TTS_FX="deep",
        ULTRON_PERSONALITY="ultron", ULTRON_HISTORY_TURNS=4, ULTRON_MAX_REPLY_CHARS=300,
        LOGGING={"loggers": {"brain": {"level": "INFO"}}},
    )
    base.update(over)
    return SimpleNamespace(**base)


KEY = {"GROQ_API_KEY": "k"}


class ConfigValidationTests(SimpleTestCase):
    def test_default_config_with_a_key_is_valid(self):
        checks.check_settings(cfg(), KEY)

    def test_every_problem_is_reported_at_once(self):
        bad = cfg(LLM_PROVIDER="nope", LLM_TIMEOUT_S=0, STT_BACKEND="x", ULTRON_PERSONALITY="zzz")
        with self.assertRaises(ImproperlyConfigured) as ctx:
            checks.check_settings(bad, KEY)
        msg = str(ctx.exception)
        for needle in ("LLM_PROVIDER", "LLM_TIMEOUT_S", "STT_BACKEND", "ULTRON_PERSONALITY"):
            self.assertIn(needle, msg)

    def test_openai_compat_requires_base_url(self):
        with self.assertRaisesRegex(ImproperlyConfigured, "BASE_URL"):
            checks.check_settings(cfg(LLM_PROVIDER="openai_compat", LLM_FALLBACK_MODEL=""), {})
        checks.check_settings(cfg(LLM_PROVIDER="openai_compat", LLM_BASE_URL="http://x/v1", LLM_FALLBACK_MODEL=""), {})

    def test_empty_model_rejected(self):
        with self.assertRaisesRegex(ImproperlyConfigured, "LLM_MODEL"):
            checks.check_settings(cfg(LLM_MODEL=""), KEY)

    def test_no_credentials_anywhere_fails_with_the_variable_name(self):
        with self.assertRaisesRegex(ImproperlyConfigured, "GROQ_API_KEY"):
            checks.check_settings(cfg(), {})

    def test_meta_provider_reads_model_api_key(self):
        meta = cfg(LLM_PROVIDER="meta", LLM_MODEL="muse-spark-1.3", LLM_FALLBACK_PROVIDER="meta", LLM_FALLBACK_MODEL="")
        with self.assertRaisesRegex(ImproperlyConfigured, "MODEL_API_KEY"):
            checks.check_settings(meta, KEY)  # a Groq key is not a Meta key
        checks.check_settings(meta, {"MODEL_API_KEY": "k"})

    def test_missing_main_key_is_tolerated_when_the_fallback_has_one(self):
        meta_main_groq_fallback = cfg(
            LLM_PROVIDER="meta", LLM_MODEL="muse-spark-1.3", LLM_FALLBACK_PROVIDER="groq"
        )
        with self.assertLogs("brain.config", "WARNING"):
            checks.check_settings(meta_main_groq_fallback, KEY)

    def test_llm_api_key_is_not_reused_for_a_different_fallback_provider(self):
        s = cfg(LLM_PROVIDER="meta", LLM_API_KEY="meta-key", LLM_FALLBACK_PROVIDER="groq")
        self.assertEqual(checks.main_key(s, {}), "meta-key")
        self.assertEqual(checks.fallback_key(s, {}), "")
        self.assertEqual(checks.fallback_key(s, KEY), "k")

    def test_fake_provider_needs_nothing(self):
        checks.check_settings(cfg(LLM_PROVIDER="fake", LLM_MODEL="", LLM_FALLBACK_MODEL=""), {})


class ServiceWiringTests(SimpleTestCase):
    def test_main_and_fallback_get_their_own_reasoning_effort(self):
        from django.test import override_settings

        from brain import services

        with override_settings(LLM_PROVIDER="meta", LLM_MODEL="muse-spark-1.3", LLM_API_KEY="k",
                               LLM_REASONING_EFFORT="minimal", LLM_FALLBACK_PROVIDER="groq",
                               LLM_FALLBACK_MODEL="openai/gpt-oss-20b", LLM_FALLBACK_API_KEY="g",
                               LLM_FALLBACK_REASONING_EFFORT="low"):
            self.assertEqual(services.build_llm().reasoning_effort, "minimal")
            self.assertEqual(services.build_fallback_llm().reasoning_effort, "low")


class EnvParsingTests(SimpleTestCase):
    def test_bad_number_names_the_variable(self):
        import os
        from unittest import mock

        from config import settings

        with mock.patch.dict(os.environ, {"LLM_TIMEOUT_S": "abc"}):
            with self.assertRaisesRegex(ImproperlyConfigured, "LLM_TIMEOUT_S"):
                settings.env_float("LLM_TIMEOUT_S", 20.0)
