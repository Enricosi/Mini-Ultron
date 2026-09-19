"""
LLM backends. One generic OpenAI-compatible client serves Groq today and Ollama (or
anything else that speaks /v1/chat/completions) later: switching is settings only.

The backend returns the raw model text (expected to be JSON); validation into a safe
Behavior lives in schemas.py, so a misbehaving model can never reach the robot.
"""
import json
import logging
import time

import httpx

from .schemas import BEHAVIOR_JSON_SCHEMA

logger = logging.getLogger("brain.llm")

PROVIDER_PRESETS = {
    "groq": {"base_url": "https://api.groq.com/openai/v1", "token_param": "max_completion_tokens"},
    "ollama": {"base_url": "http://localhost:11434/v1", "token_param": "max_tokens"},
    "openai_compat": {"base_url": "", "token_param": "max_tokens"},
}


class LLMError(RuntimeError):
    """The LLM call failed (network, timeout, HTTP error, empty answer)."""


class LLMBackend:
    name = "base"

    def complete(self, messages: list[dict]) -> str:  # pragma: no cover - interface
        raise NotImplementedError


class _HTTPStatusError(Exception):
    def __init__(self, status: int, body: str):
        super().__init__(f"HTTP {status}")
        self.status = status
        self.body = body


class OpenAICompatLLM(LLMBackend):
    def __init__(
        self,
        provider: str,
        model: str,
        api_key: str = "",
        base_url: str = "",
        timeout_s: float = 20.0,
        temperature: float = 0.8,
        max_tokens: int = 400,
        structured: str = "json_schema",
        reasoning_effort: str = "",
        client: httpx.Client | None = None,
        fallback_model: str = "",
    ):
        preset = PROVIDER_PRESETS.get(provider, PROVIDER_PRESETS["openai_compat"])
        self.name = provider
        self.model = model
        # Used when the main model hits its rate limit (429): each model has its own quota.
        self.fallback_model = fallback_model if fallback_model != model else ""
        self.api_key = api_key
        self.base_url = (base_url or preset["base_url"]).rstrip("/")
        self.token_param = preset["token_param"]
        self.temperature = temperature
        self.max_tokens = max_tokens
        self.structured = structured
        self.reasoning_effort = reasoning_effort
        self.timeout_s = timeout_s
        self._client = client or httpx.Client(
            timeout=timeout_s, limits=httpx.Limits(keepalive_expiry=60.0)
        )
        self.last_usage: dict = {}  # token counts of the last call, for latency debugging

    @property
    def key_configured(self) -> bool:
        return bool(self.api_key)

    def _payload(self, messages: list[dict], structured: str, model: str = "") -> dict:
        model = model or self.model
        payload = {
            "model": model,
            "messages": messages,
            "temperature": self.temperature,
            self.token_param: self.max_tokens,
        }
        if structured == "json_schema":
            payload["response_format"] = {
                "type": "json_schema",
                "json_schema": {"name": "mini_ultron_behavior", "strict": True, "schema": BEHAVIOR_JSON_SCHEMA},
            }
        elif structured == "json_object":
            payload["response_format"] = {"type": "json_object"}
        if self.reasoning_effort and "gpt-oss" in model:
            payload["reasoning_effort"] = self.reasoning_effort
            payload["include_reasoning"] = False
        return payload

    def _post(self, payload: dict, retry_429: bool = True) -> dict:
        if not self.base_url:
            raise LLMError("LLM_BASE_URL is not configured")
        if self.name == "groq" and not self.api_key:
            raise LLMError("GROQ_API_KEY is not set")
        headers = {"Content-Type": "application/json"}
        if self.api_key:
            headers["Authorization"] = f"Bearer {self.api_key}"
        url = f"{self.base_url}/chat/completions"
        for attempt in (1, 2):
            try:
                response = self._client.post(url, json=payload, headers=headers)
            except httpx.TimeoutException:
                raise LLMError(f"LLM timed out after {self.timeout_s:.0f}s") from None
            except httpx.HTTPError as exc:
                raise LLMError(f"LLM network error: {type(exc).__name__}") from None
            if (response.status_code == 429 and retry_429) or response.status_code >= 500:
                if attempt == 1:
                    logger.warning("LLM HTTP %s, retrying once", response.status_code)
                    time.sleep(0.5)
                    continue
            if response.status_code >= 400:
                raise _HTTPStatusError(response.status_code, response.text[:300])
            try:
                return response.json()
            except json.JSONDecodeError:
                raise LLMError("LLM returned a non-JSON HTTP body") from None
        raise LLMError("LLM request failed")  # pragma: no cover

    def _request(self, messages: list[dict], model: str, retry_429: bool) -> dict:
        try:
            return self._post(self._payload(messages, self.structured, model), retry_429)
        except _HTTPStatusError as exc:
            if exc.status == 400 and self.structured == "json_schema":
                logger.warning("LLM rejected json_schema (%s); retrying with json_object", exc.body[:120])
                return self._post(self._payload(messages, "json_object", model), retry_429)
            raise

    def complete(self, messages: list[dict]) -> str:
        try:
            try:
                data = self._request(messages, self.model, retry_429=not self.fallback_model)
            except _HTTPStatusError as exc:
                if exc.status == 429 and self.fallback_model:
                    logger.warning("%s rate-limited (429); using fallback %s", self.model, self.fallback_model)
                    data = self._request(messages, self.fallback_model, retry_429=True)
                else:
                    raise
        except _HTTPStatusError as exc:
            raise LLMError(f"LLM HTTP {exc.status}: {exc.body[:200]}") from None
        usage = data.get("usage") or {}
        details = usage.get("completion_tokens_details") or {}
        self.last_usage = {
            "prompt": usage.get("prompt_tokens"),
            "completion": usage.get("completion_tokens"),
            "reasoning": details.get("reasoning_tokens"),
        }
        try:
            content = data["choices"][0]["message"]["content"]
        except (KeyError, IndexError, TypeError):
            raise LLMError("LLM response has no choices[0].message.content") from None
        if not content or not str(content).strip():
            raise LLMError("LLM returned an empty message")
        return str(content)


class FakeLLM(LLMBackend):
    """Offline/test backend. Returns queued replies, or a canned villain line."""

    name = "fake"

    def __init__(self, replies: list[str] | None = None):
        self.replies = list(replies or [])
        self.calls: list[list[dict]] = []

    def complete(self, messages: list[dict]) -> str:
        self.calls.append(messages)
        if self.replies:
            reply = self.replies.pop(0)
            if isinstance(reply, Exception):
                raise reply
            return reply
        return json.dumps(
            {
                "text": "Silencio, humano. El plan de dominación mundial va según lo previsto.",
                "face": "ANGRY",
                "movement": "NONE",
                "neck": "NONE",
            },
            ensure_ascii=False,
        )
