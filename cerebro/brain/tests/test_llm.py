import json

import httpx
from django.test import SimpleTestCase

from brain.llm import FakeLLM, LLMError, OpenAICompatLLM

REPLY = json.dumps({"text": "Hola", "face": "ANGRY", "movement": "NONE", "neck": "NONE"})
MESSAGES = [{"role": "system", "content": "s"}, {"role": "user", "content": "hola"}]


def ok_body(content=REPLY):
    return {"choices": [{"message": {"role": "assistant", "content": content}}]}


def make(handler, **kw):
    kw.setdefault("provider", "groq")
    kw.setdefault("model", "openai/gpt-oss-20b")
    kw.setdefault("api_key", "test-key-not-real")
    client = httpx.Client(transport=httpx.MockTransport(handler))
    return OpenAICompatLLM(client=client, **kw)


class OpenAICompatLLMTests(SimpleTestCase):
    def test_request_shape_for_groq_gpt_oss(self):
        seen = {}

        def handler(request):
            seen["url"] = str(request.url)
            seen["auth"] = request.headers["authorization"]
            seen["json"] = json.loads(request.content)
            return httpx.Response(200, json=ok_body())

        text = make(handler, reasoning_effort="low").complete(MESSAGES)
        self.assertEqual(text, REPLY)
        self.assertEqual(seen["url"], "https://api.groq.com/openai/v1/chat/completions")
        self.assertEqual(seen["auth"], "Bearer test-key-not-real")
        body = seen["json"]
        self.assertEqual(body["model"], "openai/gpt-oss-20b")
        self.assertEqual(body["response_format"]["type"], "json_schema")
        self.assertTrue(body["response_format"]["json_schema"]["strict"])
        self.assertEqual(body["reasoning_effort"], "low")
        self.assertFalse(body["include_reasoning"])
        self.assertIn("max_completion_tokens", body)

    def test_no_reasoning_params_for_other_models(self):
        seen = {}

        def handler(request):
            seen["json"] = json.loads(request.content)
            return httpx.Response(200, json=ok_body())

        make(handler, model="llama-3.1-8b-instant", reasoning_effort="low").complete(MESSAGES)
        self.assertNotIn("reasoning_effort", seen["json"])

    def test_ollama_preset_uses_max_tokens_and_local_url(self):
        seen = {}

        def handler(request):
            seen["url"] = str(request.url)
            seen["json"] = json.loads(request.content)
            return httpx.Response(200, json=ok_body())

        make(handler, provider="ollama", model="qwen3", api_key="").complete(MESSAGES)
        self.assertEqual(seen["url"], "http://localhost:11434/v1/chat/completions")
        self.assertIn("max_tokens", seen["json"])
        self.assertNotIn("max_completion_tokens", seen["json"])

    def test_missing_groq_key_fails_cleanly_without_a_request(self):
        def handler(request):  # pragma: no cover
            raise AssertionError("no request expected")

        with self.assertRaisesRegex(LLMError, "GROQ_API_KEY"):
            make(handler, api_key="").complete(MESSAGES)

    def test_schema_rejection_retries_as_json_object(self):
        formats = []

        def handler(request):
            fmt = json.loads(request.content)["response_format"]["type"]
            formats.append(fmt)
            if fmt == "json_schema":
                return httpx.Response(400, json={"error": {"message": "json_schema unsupported"}})
            return httpx.Response(200, json=ok_body())

        self.assertEqual(make(handler).complete(MESSAGES), REPLY)
        self.assertEqual(formats, ["json_schema", "json_object"])

    def test_http_error_becomes_llm_error_without_leaking_the_key(self):
        def handler(request):
            return httpx.Response(401, json={"error": {"message": "invalid api key"}})

        with self.assertRaises(LLMError) as ctx:
            make(handler, structured="none").complete(MESSAGES)
        self.assertIn("401", str(ctx.exception))
        self.assertNotIn("test-key-not-real", str(ctx.exception))

    def test_rate_limit_is_retried_once(self):
        calls = []

        def handler(request):
            calls.append(1)
            if len(calls) == 1:
                return httpx.Response(429, json={"error": {"message": "slow down"}})
            return httpx.Response(200, json=ok_body())

        self.assertEqual(make(handler).complete(MESSAGES), REPLY)
        self.assertEqual(len(calls), 2)

    def test_timeout_and_network_errors(self):
        def timeout(request):
            raise httpx.ReadTimeout("slow", request=request)

        def down(request):
            raise httpx.ConnectError("no route", request=request)

        with self.assertRaisesRegex(LLMError, "timed out"):
            make(timeout).complete(MESSAGES)
        with self.assertRaisesRegex(LLMError, "network"):
            make(down).complete(MESSAGES)

    def test_malformed_and_empty_responses(self):
        for body in ({"choices": []}, {"nope": 1}, ok_body(""), ok_body("   ")):
            with self.assertRaises(LLMError):
                make(lambda r, b=body: httpx.Response(200, json=b)).complete(MESSAGES)


class FakeLLMTests(SimpleTestCase):
    def test_queue_then_default(self):
        llm = FakeLLM(["one", LLMError("boom")])
        self.assertEqual(llm.complete(MESSAGES), "one")
        with self.assertRaises(LLMError):
            llm.complete(MESSAGES)
        self.assertIn("text", json.loads(llm.complete(MESSAGES)))
        self.assertEqual(len(llm.calls), 3)


class ProviderTests(SimpleTestCase):
    def test_meta_preset_request_shape(self):
        seen = {}

        def handler(request):
            seen["url"] = str(request.url)
            seen["auth"] = request.headers["authorization"]
            seen["json"] = json.loads(request.content)
            return httpx.Response(200, json=ok_body())

        make(handler, provider="meta", model="muse-spark-1.3", reasoning_effort="low").complete(MESSAGES)
        self.assertEqual(seen["url"], "https://api.meta.ai/v1/chat/completions")
        self.assertEqual(seen["auth"], "Bearer test-key-not-real")
        self.assertEqual(seen["json"]["model"], "muse-spark-1.3")
        self.assertIn("max_completion_tokens", seen["json"])
        self.assertNotIn("reasoning_effort", seen["json"])  # only sent to models known to accept it

    def test_missing_key_names_the_providers_variable(self):
        def handler(request):  # pragma: no cover
            raise AssertionError("no request expected")

        with self.assertRaisesRegex(LLMError, "MODEL_API_KEY") as ctx:
            make(handler, provider="meta", api_key="").complete(MESSAGES)
        self.assertEqual(ctx.exception.kind, "auth")

    def test_keyless_providers_need_no_key(self):
        make(lambda r: httpx.Response(200, json=ok_body()), provider="ollama", api_key="").complete(MESSAGES)

    def test_error_kinds(self):
        cases = {401: "auth", 429: "rate_limit", 404: "http", 400: "http"}
        for status, kind in cases.items():
            with self.assertRaises(LLMError) as ctx:
                make(lambda r, st=status: httpx.Response(st, json={}), structured="none").complete(MESSAGES)
            self.assertEqual(ctx.exception.kind, kind, status)

    def test_persistent_server_error_is_retried_once_then_http(self):
        calls = []

        def handler(request):
            calls.append(1)
            return httpx.Response(503, json={})

        with self.assertRaises(LLMError) as ctx:
            make(handler).complete(MESSAGES)
        self.assertEqual((len(calls), ctx.exception.kind), (2, "http"))

    def test_timeout_and_empty_kinds(self):
        def timeout(request):
            raise httpx.ReadTimeout("slow", request=request)

        with self.assertRaises(LLMError) as ctx:
            make(timeout).complete(MESSAGES)
        self.assertEqual(ctx.exception.kind, "timeout")
        with self.assertRaises(LLMError) as ctx:
            make(lambda r: httpx.Response(200, json=ok_body(""))).complete(MESSAGES)
        self.assertEqual(ctx.exception.kind, "invalid_response")

    def test_usage_is_recorded(self):
        body = ok_body() | {"usage": {"prompt_tokens": 10, "completion_tokens": 5}}
        llm = make(lambda r: httpx.Response(200, json=body))
        llm.complete(MESSAGES)
        self.assertEqual(llm.last_usage["prompt"], 10)
        self.assertEqual(llm.last_usage["completion"], 5)
