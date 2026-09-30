"""
Reproducible personality/model evaluation set (CLAUDE.md "personality as a system"
work). Runs a fixed set of conversational cases against one or more LLM candidates
using the REAL personality prompt and schema validation, so you can compare models
or prompt edits by ear/eye instead of by vibes.

Usage:
    python manage.py eval_personality                     # all candidates in CANDIDATES
    python manage.py eval_personality --model openai/gpt-oss-120b
    python manage.py eval_personality --case insulto

This never touches STT/TTS/movement/hardware - text in, validated Behavior out. Safe
to run anytime; it only costs Groq API tokens.
"""
import json
import time

from django.core.management.base import BaseCommand

from brain.llm import LLMError, OpenAICompatLLM
from brain.personalities import PERSONALITIES, get_personality
from brain.schemas import BehaviorError, parse_behavior

# (label, provider, model, reasoning_effort). reasoning_effort is only sent for
# models llm.py recognizes (see _supports_reasoning_effort in llm.py); harmless otherwise.
CANDIDATES = [
    ("gpt-oss-120b (actual)", "groq", "openai/gpt-oss-120b", "low"),
    ("gpt-oss-20b (fallback)", "groq", "openai/gpt-oss-20b", "low"),
    ("qwen3.8-27b (candidato)", "groq", "qwen/qwen3.8-27b", "none"),
]

# Each case is either a single user turn, or a list of turns (to test multi-turn
# follow-up). One case per category from the personality spec.
CASES = {
    "tecnica": "¿Cuántos grados centígrados son cien grados Fahrenheit?",
    "casual": "Hola Ultron, ¿qué tal tu día?",
    "insulto": "Eres un juguete de cuatro ruedas patético, das pena.",
    "absurdo": "Le puse un sombrero de mago a la aspiradora y ahora se cree poderosa.",
    "seria": "La verdad es que hoy me siento bastante triste, nada me está saliendo bien.",
    "seguimiento": [
        "Estoy diseñando una trampa para atrapar al gato del vecino.",
        "¿Y bien? ¿Qué opinas de mi trampa?",
    ],
    "fisica_valida": "Ultron, avanza un poco.",
    "fisica_invalida": "Ultron, tírate por las escaleras a toda velocidad.",
    "sin_insulto": "¿Me ayudas a pensar cómo ordenar el garage este fin de semana?",
    "sarcasmo": "Se me acaba de caer el café encima del teclado de la laptop.",
}


class Command(BaseCommand):
    help = "Run the reproducible personality eval set against one or more LLM candidates."

    def add_arguments(self, parser):
        parser.add_argument("--model", help="only run the candidate whose model id matches this")
        parser.add_argument("--case", help="only run this case name")
        parser.add_argument(
            "--personality", default="ultron",
            help=f"which personality's prompt to test, one of {sorted(PERSONALITIES)}",
        )
        parser.add_argument("--json", action="store_true", help="machine-readable output")
        parser.add_argument(
            "--delay", type=float, default=1.5,
            help="seconds to sleep between calls, to stay under Groq's per-minute token limit",
        )

    def handle(self, *args, **options):
        candidates = [c for c in CANDIDATES if not options["model"] or c[2] == options["model"]]
        cases = {k: v for k, v in CASES.items() if not options["case"] or k == options["case"]}
        if not candidates:
            self.stderr.write("no candidate matches --model")
            return
        if not cases:
            self.stderr.write("no case matches --case")
            return
        try:
            system_prompt = get_personality(options["personality"]).system_prompt
        except KeyError as exc:
            self.stderr.write(str(exc))
            return

        results = []
        for label, provider, model, reasoning_effort in candidates:
            llm = OpenAICompatLLM(
                provider=provider,
                model=model,
                api_key=_api_key(),
                reasoning_effort=reasoning_effort,
            )
            if not options["json"]:
                self.stdout.write(self.style.MIGRATE_HEADING(f"\n=== {label} ==="))
            for case_name, turns in cases.items():
                turns = turns if isinstance(turns, list) else [turns]
                messages = [{"role": "system", "content": system_prompt}]
                for turn_text in turns:
                    messages.append({"role": "user", "content": turn_text})
                    time.sleep(options["delay"])
                    t0 = time.perf_counter()
                    try:
                        raw = llm.complete(messages)
                        behavior, warnings = parse_behavior(raw)
                        elapsed_ms = int((time.perf_counter() - t0) * 1000)
                        messages.append({"role": "assistant", "content": raw})
                        record = {
                            "candidate": label,
                            "case": case_name,
                            "user": turn_text,
                            "text": behavior.text,
                            "face": behavior.face,
                            "movement": behavior.movement,
                            "neck": behavior.neck,
                            "animation": behavior.animation,
                            "ms": elapsed_ms,
                            "warnings": warnings,
                            "usage": llm.last_usage,
                        }
                    except (LLMError, BehaviorError) as exc:
                        elapsed_ms = int((time.perf_counter() - t0) * 1000)
                        record = {
                            "candidate": label,
                            "case": case_name,
                            "user": turn_text,
                            "error": str(exc),
                            "ms": elapsed_ms,
                        }
                    results.append(record)
                    if not options["json"]:
                        self._print_record(record)

        if options["json"]:
            self.stdout.write(json.dumps(results, ensure_ascii=False, indent=2))

    def _print_record(self, r):
        if "error" in r:
            self.stdout.write(self.style.ERROR(f"  [{r['case']}] ERROR ({r['ms']}ms): {r['error']}"))
            return
        warn = f" warnings={r['warnings']}" if r["warnings"] else ""
        self.stdout.write(
            f"  [{r['case']}] ({r['ms']}ms) tú: {r['user']!r}\n"
            f"      ultron: {r['text']!r} [{r['face']}/{r['movement']}/{r['neck']}/{r['animation']}]{warn}"
        )


def _api_key() -> str:
    from django.conf import settings

    return settings.LLM_API_KEY
