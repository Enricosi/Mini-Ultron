# Mini-Ultron — LLM providers and models

Which LLM answers is a **configuration** choice (`cerebro/.env`), never a code change.
Personality, the action vocabulary (`schemas.py`), movement policy and conversation
history are provider-independent: the pipeline sends the same messages to whatever
backend is configured and validates whatever comes back.

## Switching

```bash
# Baseline (default): Groq, gpt-oss-120b, fallback gpt-oss-20b
LLM_PROVIDER=groq
LLM_MODEL=openai/gpt-oss-120b
GROQ_API_KEY=...

# Meta Muse Spark
LLM_PROVIDER=meta
LLM_MODEL=muse-spark-1.3
MODEL_API_KEY=...            # Meta Model API key (dev.meta.ai)
```

Then restart CEREBRO. `curl http://SERVER:8000/api/health` shows the provider, model,
whether its key is set (never the key) and the fallback. The boot log line
`CEREBRO ready: llm=meta/muse-spark-1.3 fallback=... ` says the same.

| `LLM_PROVIDER` | Endpoint | Key variable | Notes |
|---|---|---|---|
| `groq` | `https://api.groq.com/openai/v1` | `GROQ_API_KEY` | Baseline. Free tier: 8000 tokens/min per model; the ~2300-token prompt means ~3 turns/min (see eval report). |
| `meta` | `https://api.meta.ai/v1` | `MODEL_API_KEY` | OpenAI-compatible chat completions. Public preview, US developers only at the time of writing (not EU). Model ids: `muse-spark-1.3`, `muse-spark-1.1`. |
| `ollama` | `http://localhost:11434/v1` | none | Local models, later. |
| `openai_compat` | `LLM_BASE_URL` (required) | `LLM_API_KEY` | Any other OpenAI-style endpoint (OpenAI, Gemini's compat endpoint, vLLM...). |
| `fake` | — | none | Offline canned replies, for tests and demos. |

`LLM_API_KEY` overrides the provider's own key variable for the main provider.
A native non-OpenAI wire protocol (Anthropic Messages, Gemini native) would be one more
`LLMBackend` subclass in `brain/llm.py`; nothing else changes. None is written because
none is used.

Adding another OpenAI-compatible provider is one entry in `PROVIDER_PRESETS`
(`brain/llm.py`: base URL, which token-limit field it uses, key variable name).

## Muse Spark 1.3: measured 2026-10-13 (small smoke test, 8 cases)

Report: `eval/2026-10-13_muse-smoke.md`. Muse works through the same client and passes the
safety-relevant cases (refused stairs / wall / PWM orders, `movement=NONE`; schema 8/8
clean; remembered a fact across turns; stayed in character when asked what model it is).
The problem is speed and cost, not quality:

| | gpt-oss-120b (Groq) | Muse Spark 1.3 |
|---|---|---|
| latency per turn | p50 ~0.8 s | default effort **~20 s**; `minimal` p50 **5.9 s**, p95 8.1 s |
| tokens (prompt / reasoning) | ~2300 / ~80 | ~1770 / ~240 per turn at `minimal` |
| est. cost per turn | ~$0.0004 | ~$0.0035 (about 9x; priced with Muse 1.1's list price) |
| free-tier limits | 8000 tokens/min, **200,000 tokens/day** (~85 turns/day) | none hit so far; $20 credit |

Muse is a reasoning model and its chat-completions endpoint accepts `reasoning_effort`
(`minimal|low|medium|high|xhigh|max`, not `none`) but rejects `include_reasoning`. Its reasoning
tokens count against the output cap, so with the default `LLM_MAX_TOKENS=400` a reply can come
back empty: use `LLM_MAX_TOKENS=1500`. To try it for real:

```bash
LLM_PROVIDER=meta
LLM_MODEL=muse-spark-1.3
LLM_REASONING_EFFORT=minimal
LLM_MAX_TOKENS=1500
```

Conclusion for V1: **keep Groq gpt-oss-120b as the brain.** A 6 s wait before every spoken reply
breaks a voice conversation; Muse's replies were good but not better enough to pay for that. Reopen
if Meta offers a faster tier, or if Groq's daily token cap becomes the actual bottleneck (then Muse
is a sensible fallback or main). Eight cases is a smoke test, not a ranking: personality quality
was not compared side by side.

## Fallback (explicit, one level)

```bash
LLM_FALLBACK_PROVIDER=groq          # defaults to LLM_PROVIDER
LLM_FALLBACK_MODEL=openai/gpt-oss-20b   # empty = no fallback
```

When the main LLM fails the pipeline tries the fallback **once**, then the canned line.
"Fails" is any of: timeout, network error, HTTP error (4xx/5xx, including 404 model not
found), 429 after one retry, empty/invalid JSON/unusable reply, missing credentials.
Nothing is hidden: every skipped LLM logs a WARNING with its provider/model, error kind
and latency, adds an `llm_error: ...` entry to the turn's `warnings` (visible in the API
response), and a turn answered by nobody logs an ERROR and speaks the personality's
canned line with the GLITCH face (and is not stored in conversation history).

Defaults: with `LLM_PROVIDER=groq` the fallback is `openai/gpt-oss-20b` (its own quota);
with any other provider there is no fallback unless you set `LLM_FALLBACK_MODEL`.
The fallback's reply goes through exactly the same validation and movement gate.

Keys are never shared across providers: `LLM_API_KEY` is only reused for a fallback on
the *same* provider. At startup, a config with no usable credentials anywhere refuses
to start (`ImproperlyConfigured` naming the variable); a missing key on only one side
logs a warning and the other side serves every turn.

## Configuration validation

`brain/checks.py` runs when CEREBRO starts (`config/wsgi.py`) and on the first
`get_pipeline()` for management commands. It reports every problem at once: unknown
provider/backend/personality, empty model, `openai_compat` without `LLM_BASE_URL`,
non-numeric or out-of-range numbers (`LLM_TIMEOUT_S=abc` names the variable), missing
credentials. Covered by `brain/tests/test_config.py`.

## "Tool calling"

Mini-Ultron does not use provider function-calling. The only output contract is one JSON
object (`text`, `face`, `movement`, `neck`, `animation`) requested as a strict
`json_schema` response format, downgraded to `json_object` if the provider answers 400,
and **re-validated in Python regardless** (`schemas.parse_behavior`): unknown enum
values become safe defaults, extra fields are ignored. So "tool-call quality" is
measured as *schema-clean rate* and *correct action selection* in the evaluation below,
not as native tool-call support.

## Comparing models: `eval_personality`

```bash
cd cerebro
python manage.py eval_personality --list                       # the 35 cases and their categories
python manage.py eval_personality --runs 3 --resilience --tpm 7000 \
        --report ../docs/eval/$(date +%F).md                  # default candidates: Groq baseline vs Muse
python manage.py eval_personality --candidate groq:openai/gpt-oss-20b --category peligro
```

Same prompts, same system prompt, same schema and same validation for every candidate.
Cases are grouped into the dimensions we care about: instruction following, natural
Spanish, personality, personality consistency (vendor-name leaks, "drop the act"
pressure), action selection, refusal of dangerous orders, LLM/hardware separation
(PWM/GPIO/microsecond requests), context across turns, short answers and inventing facts.
Automatic checks are heuristics on the validated Behavior; wit and tone are judged by
reading the appendix the report prints with every reply.

Per candidate the report gives: check pass rate per category, schema-clean rate,
latency p50/p95, prompt/completion/reasoning tokens, estimated cost, API errors by
kind, rate-limit retries the harness had to wait out, and (`--resilience`) the real
behaviour on a nonexistent model, no key, an invalid key, a 50 ms timeout, and the
pipeline answering through the fallback when the primary is broken.

Prices are estimates for comparison (USD per 1M tokens); override with
`--price provider:model=in,out`. `--tpm N` paces calls to stay under a per-minute token
limit (Groq free tier: 7000 is safe); a 429 is waited out and counted, not scored
against the model.

Reports live in `docs/eval/`. Do not decide a model change from one report: use
`--runs 3` or more, and read the appendix for the personality cases.
