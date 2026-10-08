"""Data-driven RPC sequences: one test per scenarios/*.json (see README)."""

import json
from pathlib import Path

import pytest

import harness

SCENARIO_DIR = Path(__file__).resolve().parent / "scenarios"


def _scenario_params():
    params = []
    for path in sorted(SCENARIO_DIR.glob("*.json")):
        scenario = json.loads(path.read_text())
        marks = []
        if scenario.get("xfail"):
            marks.append(pytest.mark.xfail(reason=scenario["xfail"], strict=True))
        params.append(pytest.param(path.stem, scenario, id=path.stem, marks=marks))
    return params


def expand(value):
    """Replace a leading $MOLECULES/ in string params with the molecules
    directory; skip the test if it does not exist."""
    if isinstance(value, dict):
        return {key: expand(item) for key, item in value.items()}
    if isinstance(value, list):
        return [expand(item) for item in value]
    if isinstance(value, str) and value.startswith("$MOLECULES/"):
        root = harness.corpus_roots().get("molecules")
        if root is None:
            pytest.skip("molecules directory not found")
        return str(root / value[len("$MOLECULES/") :])
    return value


def contains(actual, expected):
    """True if every key in expected is in actual with a matching value;
    nested dicts match recursively, floats compare approximately. A list
    matches a list of the same length, element by element (each element
    matched the same way, so a list of dicts may name only some keys)."""
    if isinstance(expected, list):
        return (
            isinstance(actual, list)
            and len(actual) == len(expected)
            and all(contains(a, e) for a, e in zip(actual, expected))
        )
    if isinstance(expected, dict):
        return isinstance(actual, dict) and all(
            key in actual and contains(actual[key], value)
            for key, value in expected.items()
        )
    # bool is an int in Python: keep true/false from matching 1/0 (and 1.0)
    if isinstance(expected, bool) or isinstance(actual, bool):
        return (
            isinstance(expected, bool)
            and isinstance(actual, bool)
            and actual == expected
        )
    if isinstance(expected, float):
        return isinstance(actual, (int, float)) and actual == pytest.approx(expected)
    return actual == expected


def dig(value, dotted):
    for part in dotted.split("."):
        value = value[int(part)] if isinstance(value, list) else value[part]
    return value


def operand(spec, saved):
    return saved[spec] if isinstance(spec, str) else spec


def run_check(check, saved):
    a, b = operand(check["a"], saved), operand(check["b"], saved)
    op = check["op"]
    if op == "lt":
        ok = a < b
    elif op == "differs":
        ok = abs(a - b) > check.get("tolerance", 1e-6)
    else:
        raise ValueError("unknown check op %r" % op)
    assert ok, "check %s failed: %s=%r %s %s=%r" % (check, check["a"], a, op, check["b"], b)


@pytest.mark.parametrize("name, scenario", _scenario_params())
def test_scenario(request, name, scenario):
    avo = request.getfixturevalue("fresh" if scenario.get("fresh") else "avo")
    saved = {}
    for number, step in enumerate(scenario["steps"], 1):
        label = "%s step %d (%s)" % (name, number, step["method"])
        args = (step["method"], expand(step.get("params")))
        wait = step.get("wait", False)
        if step.get("expect", "ok") == "error":
            error = avo.expect_error(*args, code=step.get("error_code"), wait=wait)
            if "message" in step:
                assert step["message"] in error.message, label
            continue

        result = avo.call(*args, wait=wait)
        if "result" in step:
            assert contains(result, step["result"]), "%s: wanted %s, got %s" % (
                label,
                step["result"],
                result,
            )
        for key, dotted in step.get("save", {}).items():
            saved[key] = dig(result, dotted)
    for check in scenario.get("checks", []):
        run_check(check, saved)
