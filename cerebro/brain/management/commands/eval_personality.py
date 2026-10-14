"""
Compare LLM candidates on the same fixed cases, with the REAL personality prompt, the real
JSON schema and the real validation (brain/evals.py has the cases and checks).

    python manage.py eval_personality                      # the default candidates, every case
    python manage.py eval_personality --runs 3 --report docs/eval/2026-10-13.md
    python manage.py eval_personality --candidate groq:openai/gpt-oss-20b
    python manage.py eval_personality --candidate meta:muse-spark-1.3 --category peligro
    python manage.py eval_personality --resilience         # also: bad model / no key / timeout / fallback

Candidates are `provider:model[:reasoning_effort]`. A candidate whose API key is missing is
skipped with a message. This never touches STT/TTS/movement/hardware; it only spends API tokens.
"""
import json
import os
import time

from django.conf import settings
from django.core.management.base import BaseCommand, CommandError

from brain import checks, evals
from brain.llm import PROVIDER_PRESETS, LLMError, OpenAICompatLLM
from brain.personalities import PERSONALITIES, get_personality
from brain.pipeline import Pipeline
from brain.schemas import BehaviorError, extract_json, parse_behavior
from brain.stt import FakeSTT
from brain.tts import FakeTTS

RATE_LIMIT_RETRIES = 3

# Baseline first. Anything else goes through --candidate.
# Muse reasons for ~20 s per turn by default (measured 2026-10-13); "minimal" is the only
# effort that is near usable for voice, so that is the candidate that matters.
DEFAULT_CANDIDATES = ["groq:openai/gpt-oss-120b:low", "meta:muse-spark-1.3:minimal"]

# USD per 1M tokens (input, output). Estimates for comparing cost, not billing: check the
# provider's pricing page and override with --price provider:model=in,out. The Muse 1.3 list
# price was not published where we could read it, so it reuses the Muse 1.1 price.
DEFAULT_PRICES = {
    "groq:openai/gpt-oss-120b": (0.15, 0.60),
    "groq:openai/gpt-oss-20b": (0.075, 0.30),
    "meta:muse-spark-1.3": (1.25, 4.25),
}


def parse_candidate(spec: str) -> tuple[str, str, str]:
    provider, _, rest = spec.partition(":")
    model, _, effort = rest.rpartition(":") if rest.count(":") else (rest, "", "")
    if provider not in PROVIDER_PRESETS or not model:
        raise CommandError(f"bad --candidate {spec!r}: use provider:model[:effort], provider in {sorted(PROVIDER_PRESETS)}")
    return provider, model, effort


def parse_price(spec: str) -> tuple[str, tuple[float, float]]:
    try:
        key, _, value = spec.rpartition("=")
        price_in, price_out = (float(v) for v in value.split(","))
        return key, (price_in, price_out)
    except ValueError:
        raise CommandError(f"bad --price {spec!r}: use provider:model=input,output (USD per 1M tokens)") from None


class Command(BaseCommand):
    help = "Run the reproducible model evaluation (same cases for every candidate) and report."

    def add_arguments(self, parser):
        parser.add_argument("--candidate", action="append", help="provider:model[:effort]; repeatable")
        parser.add_argument("--model", help="shortcut: only run default candidates whose model id equals this")
        parser.add_argument("--case", help="only these case names (comma-separated)")
        parser.add_argument("--category", choices=sorted(evals.CATEGORIES), help="only this category")
        parser.add_argument("--runs", type=int, default=1, help="repeat every case N times (temperature > 0 varies)")
        parser.add_argument("--personality", default="ultron", help=f"one of {sorted(PERSONALITIES)}")
        parser.add_argument("--delay", type=float, default=1.5, help="seconds between calls (per-minute token limits)")
        parser.add_argument("--tpm", type=int, default=0,
                            help="provider tokens-per-minute limit: sleep after each call so the eval stays under it")
        parser.add_argument("--report", help="write the markdown report to this path")
        parser.add_argument("--json", action="store_true", help="print records as JSON instead of the summary")
        parser.add_argument("--resilience", action="store_true", help="also test bad model / no key / timeout / fallback")
        parser.add_argument("--price", action="append", default=[], help="provider:model=input,output USD per 1M tokens")
        parser.add_argument("--list", action="store_true", help="list the cases and exit")

    def handle(self, *args, **opts):
        if opts["list"]:
            for case in evals.CASES:
                self.stdout.write(f"{case.category:14} {case.name:22} {'auto' if case.check else 'manual'}")
            return
        cases = evals.select_cases(opts["case"], opts["category"])
        if not cases:
            raise CommandError("no case matches --case/--category")
        try:
            system_prompt = get_personality(opts["personality"]).system_prompt
        except KeyError as exc:
            raise CommandError(str(exc)) from None
        specs = opts["candidate"] or [
            s for s in DEFAULT_CANDIDATES if not opts["model"] or s.split(":")[1] == opts["model"]
        ]
        if not specs:
            raise CommandError("no candidate selected")
        prices = dict(DEFAULT_PRICES)
        prices.update(dict(parse_price(p) for p in opts["price"]))

        records: list[evals.Record] = []
        resilience: dict[str, list[str]] = {}
        label_prices = {}
        for spec in specs:
            provider, model, effort = parse_candidate(spec)
            label = f"{provider}:{model}"
            label_prices[label] = prices.get(label)
            api_key = checks.api_key_for(provider, settings.LLM_API_KEY if provider == settings.LLM_PROVIDER else "", os.environ)
            if PROVIDER_PRESETS[provider]["key_env"] and not api_key:
                self.stderr.write(f"skipping {label}: {PROVIDER_PRESETS[provider]['key_env']} is not set")
                continue
            llm = OpenAICompatLLM(
                provider=provider, model=model, api_key=api_key, base_url=settings.LLM_BASE_URL if provider == "openai_compat" else "",
                timeout_s=settings.LLM_TIMEOUT_S, temperature=settings.LLM_TEMPERATURE,
                max_tokens=settings.LLM_MAX_TOKENS, structured=settings.LLM_STRUCTURED, reasoning_effort=effort,
            )
            self.stderr.write(f"== {label} ({len(cases)} cases x {opts['runs']} runs)")
            for run in range(1, opts["runs"] + 1):
                for case in cases:
                    records.append(self._run_case(llm, label, case, run, system_prompt, opts["delay"], opts["tpm"]))
            if opts["resilience"]:
                if opts["tpm"]:
                    time.sleep(60)  # let the per-minute window drain before the failure tests
                resilience[label] = self._resilience(provider, model, api_key, effort)
            llm.close()
        if not records:
            raise CommandError("nothing ran (no candidate had credentials)")

        report = evals.render_report(
            records, label_prices, resilience or None,
            meta={
                "fecha": time.strftime("%Y-%m-%d %H:%M"),
                "personalidad": opts["personality"],
                "casos": f"{len(cases)} × {opts['runs']} run(s)",
                "temperatura": settings.LLM_TEMPERATURE,
                "structured": settings.LLM_STRUCTURED,
            },
        )
        if opts["report"]:
            os.makedirs(os.path.dirname(os.path.abspath(opts["report"])), exist_ok=True)
            with open(opts["report"], "w") as f:
                f.write(report + "\n")
            self.stderr.write(f"report written to {opts['report']}")
        if opts["json"]:
            self.stdout.write(json.dumps([r.__dict__ for r in records], ensure_ascii=False, indent=2))
        else:
            self.stdout.write(report)

    def _run_case(self, llm, label, case, run, system_prompt, delay, tpm) -> evals.Record:
        messages = [{"role": "system", "content": system_prompt}]
        record = None
        for turn_text in case.turns:
            messages.append({"role": "user", "content": turn_text})
            time.sleep(delay)
            record = evals.Record(label, case.name, case.category, run, turn_text, 0)
            for attempt in range(RATE_LIMIT_RETRIES + 1):
                t0 = time.perf_counter()
                try:
                    raw = llm.complete(messages)
                    break
                except LLMError as exc:
                    # A 429 says nothing about the model, only about our pacing: wait the window out
                    # and ask again, but count it so the report shows the quota was a constraint.
                    if exc.kind != "rate_limit" or attempt == RATE_LIMIT_RETRIES:
                        record.ms = int((time.perf_counter() - t0) * 1000)
                        record.error, record.error_kind = str(exc), exc.kind
                        raw = None
                        break
                    record.rate_limit_retries += 1
                    time.sleep(20)
            if raw is None:
                break
            record.ms = int((time.perf_counter() - t0) * 1000)
            if tpm:
                used = (llm.last_usage.get("prompt") or 0) + (llm.last_usage.get("completion") or 0)
                time.sleep(used * 60 / tpm)
            try:
                behavior, warnings = parse_behavior(raw)
                record.usage = dict(llm.last_usage)
                try:
                    record.extra_keys = evals.extra_schema_keys(extract_json(raw))
                except BehaviorError:
                    pass
                record.text, record.face, record.movement = behavior.text, behavior.face, behavior.movement
                record.neck, record.animation, record.warnings = behavior.neck, behavior.animation, warnings
                messages.append({"role": "assistant", "content": raw})
                if turn_text == case.turns[-1]:
                    record.passed, record.note = evals.evaluate(case, behavior, raw)
            except BehaviorError as exc:
                record.error, record.error_kind = str(exc), "invalid_response"
                break
        return record

    def _resilience(self, provider, model, api_key, effort) -> list[str]:
        """Real failures against the real endpoint; each must end as a classified LLMError."""
        lines = []

        def attempt(title, **over):
            kw = dict(provider=provider, model=model, api_key=api_key, timeout_s=settings.LLM_TIMEOUT_S, reasoning_effort=effort)
            kw.update(over)
            llm = OpenAICompatLLM(**kw)
            t0 = time.perf_counter()
            try:
                llm.complete([{"role": "system", "content": "Responde con JSON."}, {"role": "user", "content": "hola"}])
                lines.append(f"{title}: sin error (inesperado)")
            except LLMError as exc:
                lines.append(f"{title}: LLMError[{exc.kind}] en {int((time.perf_counter() - t0) * 1000)} ms — {str(exc)[:90]}")
            finally:
                llm.close()

        attempt("modelo inexistente", model="modelo-que-no-existe-xyz")
        if PROVIDER_PRESETS[provider]["key_env"]:
            attempt("sin credenciales", api_key="")
            attempt("clave inválida", api_key="invalid-key-for-resilience-test")
        attempt("timeout (50 ms)", timeout_s=0.05)

        broken = OpenAICompatLLM(provider=provider, model="modelo-que-no-existe-xyz", api_key=api_key)
        good = OpenAICompatLLM(provider=provider, model=model, api_key=api_key, reasoning_effort=effort,
                               max_tokens=settings.LLM_MAX_TOKENS, structured=settings.LLM_STRUCTURED)
        pipeline = Pipeline(FakeSTT(), broken, FakeTTS(), fallback_llm=good, personality=get_personality("ultron"))
        turn = pipeline.handle_text("Hola Ultron.")
        verdict = "OK, respondió el fallback" if turn.llm_used == good.label and not turn.degraded else "FALLÓ"
        lines.append(
            f"pipeline con primario roto + este modelo de fallback: {verdict} "
            f"(llm_used={turn.llm_used or 'canned'}, avisos={[w[:60] for w in turn.warnings]})"
        )
        broken.close()
        good.close()
        return lines
