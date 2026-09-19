import json

from django.test import SimpleTestCase

from brain.schemas import (
    BEHAVIOR_JSON_SCHEMA,
    FACES,
    MOVEMENTS,
    NECKS,
    Behavior,
    BehaviorError,
    apply_policy,
    extract_json,
    parse_behavior,
)


def raw(**over):
    data = {"text": "Hola, humano.", "face": "ANGRY", "movement": "NONE", "neck": "NONE"}
    data.update(over)
    return json.dumps(data)


class ParseBehaviorTests(SimpleTestCase):
    def test_valid_output(self):
        b, warnings = parse_behavior(raw())
        self.assertEqual(b, Behavior("Hola, humano.", "ANGRY", "NONE", "NONE"))
        self.assertEqual(warnings, [])

    def test_enums_are_case_insensitive(self):
        b, _ = parse_behavior(raw(face="angry", movement=" forward ", neck="left"))
        self.assertEqual((b.face, b.movement, b.neck), ("ANGRY", "FORWARD", "LEFT"))

    def test_unknown_values_fall_back_to_safe_defaults(self):
        b, warnings = parse_behavior(raw(face="EXPLODING", movement="JUMP_OFF_TABLE", neck="SPIN"))
        self.assertEqual((b.face, b.movement, b.neck), ("SPEAKING", "NONE", "NONE"))
        self.assertEqual(len(warnings), 3)

    def test_system_faces_are_not_selectable_by_the_llm(self):
        for state in ("LISTENING", "THINKING"):
            b, warnings = parse_behavior(raw(face=state))
            self.assertEqual(b.face, "SPEAKING")
            self.assertTrue(warnings)

    def test_missing_optional_fields_use_defaults(self):
        b, warnings = parse_behavior(json.dumps({"text": "Hola"}))
        self.assertEqual((b.face, b.movement, b.neck), ("SPEAKING", "NONE", "NONE"))
        self.assertEqual(warnings, [])

    def test_missing_or_empty_text_is_fatal(self):
        for bad in ('{"face": "ANGRY"}', raw(text="   "), raw(text=None), raw(text=123), raw(text="***")):
            with self.assertRaises(BehaviorError):
                parse_behavior(bad)

    def test_raw_hardware_values_are_ignored_not_passed_through(self):
        b, _ = parse_behavior(raw(pwm=255, servo_us=1500, speed=100, gpio=13))
        self.assertEqual(set(b.to_dict()), {"text", "face", "movement", "neck"})

    def test_text_is_cleaned_and_truncated(self):
        b, _ = parse_behavior(raw(text="**Hola**   `mundo`\n\ncruel"))
        self.assertEqual(b.text, "Hola mundo cruel")
        long_text = "Frase uno. " + "palabra " * 80
        b, _ = parse_behavior(raw(text=long_text), max_chars=60)
        self.assertLessEqual(len(b.text), 61)

    def test_extract_json_tolerates_fences_and_chatter(self):
        self.assertEqual(extract_json('```json\n{"a": 1}\n```'), {"a": 1})
        self.assertEqual(extract_json('Claro: {"a": 1} listo'), {"a": 1})
        for bad in ("", "no json here", "[1, 2]", '{"a": '):
            with self.assertRaises(BehaviorError):
                extract_json(bad)


class PolicyTests(SimpleTestCase):
    def test_movement_dropped_unless_allowed(self):
        b = Behavior("x", "ANGRY", "FORWARD", "LEFT")
        dropped = apply_policy(b, allow_movement=False)
        self.assertEqual((dropped.movement, dropped.face, dropped.neck), ("NONE", "ANGRY", "LEFT"))
        self.assertEqual(apply_policy(b, allow_movement=True).movement, "FORWARD")


class SchemaVocabularyTests(SimpleTestCase):
    """The schema handed to the LLM must equal the vocabulary the validator accepts."""

    def test_schema_enums_match_vocabulary(self):
        props = BEHAVIOR_JSON_SCHEMA["properties"]
        self.assertEqual(props["face"]["enum"], list(FACES))
        self.assertEqual(props["movement"]["enum"], list(MOVEMENTS))
        self.assertEqual(props["neck"]["enum"], list(NECKS))
        self.assertEqual(set(BEHAVIOR_JSON_SCHEMA["required"]), set(props))
        self.assertFalse(BEHAVIOR_JSON_SCHEMA["additionalProperties"])
