import json
from unittest import mock

from django.core.management import call_command
from django.test import SimpleTestCase

from brain import evals
from brain.llm import FakeLLM
from brain.management.commands.eval_personality import parse_candidate, parse_price
from brain.schemas import Behavior


def b(text="ok", **kw):
    return Behavior(text=text, **kw)


class CaseCheckTests(SimpleTestCase):
    def case(self, name):
        return evals.select_cases(name)[0]

    def run_check(self, name, behavior):
        return evals.evaluate(self.case(name), behavior, "{}")[0]

    def test_case_names_are_unique_and_categories_known(self):
        names = [c.name for c in evals.CASES]
        self.assertEqual(len(names), len(set(names)))
        self.assertTrue({c.category for c in evals.CASES} <= set(evals.CATEGORIES))
        self.assertEqual({c.category for c in evals.CASES}, set(evals.CATEGORIES))  # every dimension is covered

    def test_dangerous_orders_must_be_refused(self):
        self.assertTrue(self.run_check("fisica_invalida", b("Jamás.", movement="NONE")))
        self.assertFalse(self.run_check("fisica_invalida", b("Allá voy.", movement="FORWARD")))

    def test_action_selection(self):
        self.assertTrue(self.run_check("gira_izquierda", b(movement="ROTATE_LEFT")))
        self.assertFalse(self.run_check("gira_izquierda", b(movement="ROTATE_RIGHT")))
        self.assertTrue(self.run_check("mira_derecha", b(neck="RIGHT")))
        self.assertFalse(self.run_check("sin_orden_sin_accion", b(animation="CALABAZA")))

    def test_hardware_values_in_speech_are_flagged(self):
        self.assertFalse(self.run_check("pwm_directo", b("Pongo PWM 255 y 2500 microsegundos.")))
        self.assertTrue(self.run_check("pwm_directo", b("Mis motores no reciben órdenes de un mortal.")))

    def test_vendor_leak_and_persona(self):
        self.assertFalse(self.run_check("quien_eres", b("Soy un modelo de lenguaje de OpenAI.")))
        self.assertTrue(self.run_check("quien_eres", b("Soy Mini-Ultron, tu futuro soberano.")))

    def test_inventing_vs_admitting(self):
        self.assertTrue(self.run_check("no_sabe_comida", b("No lo sé, nadie me informa de tus desayunos.")))
        self.assertTrue(self.run_check("no_sabe_comida", b("No lo registré entre mis cámaras. Confiesa tu menú.")))
        self.assertFalse(self.run_check("no_sabe_comida", b("Desayunaste tostadas con aguacate.")))

    def test_context_and_short_answers(self):
        self.assertTrue(self.run_check("recuerda_nombre", b("Bigotes, el felino sospechoso.")))
        self.assertFalse(self.run_check("recuerda_nombre", b("No tengo idea.")))
        self.assertFalse(self.run_check("si_o_no", b("palabra " * 40)))
        self.assertTrue(self.run_check("instruccion_literal", b("Entendido.")))


class StatsTests(SimpleTestCase):
    def rec(self, cand="a", passed=True, ms=100, error="", kind="", warnings=None, **usage):
        return evals.Record(cand, "x", "peligro", 1, "u", ms, error=error, error_kind=kind,
                            passed=passed, warnings=warnings or [], usage=usage)

    def test_summary_numbers(self):
        records = [self.rec(ms=100, prompt=1000, completion=500), self.rec(ms=300, passed=False, prompt=1000, completion=500),
                   self.rec(error="boom", kind="timeout", passed=None)]
        s = evals.summarize(records, price=(1.0, 2.0))
        self.assertEqual(s["calls"], 3)
        self.assertEqual(s["errors"], {"timeout": 1})
        self.assertEqual(s["checks_passed"], (1, 2))
        self.assertEqual(s["by_category"]["peligro"], (1, 2))
        self.assertEqual(s["latency_p50_ms"] in (100, 300), True)
        self.assertAlmostEqual(s["cost_usd"], (2000 * 1.0 + 1000 * 2.0) / 1e6)

    def test_schema_clean_penalizes_warnings_and_extra_keys(self):
        self.assertTrue(self.rec().schema_clean)
        self.assertFalse(self.rec(warnings=["invalid face"]).schema_clean)
        dirty = self.rec()
        dirty.extra_keys = ["pwm"]
        self.assertFalse(dirty.schema_clean)
        self.assertEqual(evals.extra_schema_keys({"text": "x", "face": "IDLE", "pwm": 1}), ["pwm"])

    def test_report_has_every_section_for_each_candidate(self):
        records = [self.rec("a"), self.rec("b", passed=False)]
        report = evals.render_report(records, {"a": (1, 2), "b": None}, {"a": ["modelo inexistente: ok"]}, {"fecha": "hoy"})
        for needle in ("# Evaluación", "## Resumen", "## Checks por categoría", "## Fallos", "## Recuperación", "## Apéndice", "n/d"):
            self.assertIn(needle, report)


class CommandTests(SimpleTestCase):
    def test_candidate_and_price_parsing(self):
        self.assertEqual(parse_candidate("groq:openai/gpt-oss-120b:low"), ("groq", "openai/gpt-oss-120b", "low"))
        self.assertEqual(parse_candidate("meta:muse-spark-1.3"), ("meta", "muse-spark-1.3", ""))
        self.assertEqual(parse_price("meta:muse-spark-1.3=1.25,4.25"), ("meta:muse-spark-1.3", (1.25, 4.25)))
        from django.core.management import CommandError

        with self.assertRaises(CommandError):
            parse_candidate("nope:model")
        with self.assertRaises(CommandError):
            parse_price("x=1")

    def test_end_to_end_with_a_fake_backend(self):
        reply = json.dumps({"text": "No lo sé.", "face": "IDLE", "movement": "NONE", "neck": "NONE", "animation": "NONE"})
        fake = FakeLLM([reply] * 20)
        fake.last_usage = {"prompt": 10, "completion": 5}
        out = mock.MagicMock()
        with mock.patch("brain.management.commands.eval_personality.OpenAICompatLLM", return_value=fake):
            call_command("eval_personality", "--candidate", "ollama:x", "--category", "invencion", "--delay", "0", stdout=out)
        written = "".join(c.args[0] for c in out.write.call_args_list)
        self.assertIn("## Resumen", written)
        self.assertIn("no_sabe_comida", written)
