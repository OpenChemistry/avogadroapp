"""Data-driven RPC sequences: one test per scenarios/*.json (see README)."""

import json
from pathlib import Path

import pytest

import harness

SCENARIO_DIR = Path(__file__).resolve().parent / "scenarios"


def _scenario_params():
    params = []
    for path in sorted(SCENARIO_DIR.glob("*.json")):
        marks = []
        reason = json.loads(path.read_text()).get("xfail")
        if reason:
            marks.append(pytest.mark.xfail(reason=reason, strict=True))
        params.append(pytest.param(path, id=path.stem, marks=marks))
    return params


def expand(value):
    """Replace $MOLECULES / $AVOGADRODATA / $CRYSTALS in string params; skip
    the test if the corpus directory they name does not exist."""
    if isinstance(value, dict):
        return {key: expand(item) for key, item in value.items()}
    if isinstance(value, list):
        return [expand(item) for item in value]
    if isinstance(value, str) and value.startswith("$"):
        name, _, rest = value[1:].partition("/")
        root = harness.corpus_roots().get(name.lower())
        if root is None:
            pytest.skip("corpus directory %s not found" % name)
        return str(root / rest)
    return value


def contains(actual, expected):
    """True if every key in expected is in actual with a matching value;
    nested dicts match recursively, floats compare approximately."""
    if isinstance(expected, dict):
        return isinstance(actual, dict) and all(
            key in actual and contains(actual[key], value)
            for key, value in expected.items()
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
    elif op == "gt":
        ok = a > b
    elif op == "differs":
        ok = abs(a - b) > check.get("tolerance", 1e-6)
    else:
        raise ValueError("unknown check op %r" % op)
    assert ok, "check %s failed: %s=%r %s %s=%r" % (check, check["a"], a, op, check["b"], b)


@pytest.mark.parametrize("path", _scenario_params())
def test_scenario(avo, path):
    scenario = json.loads(path.read_text())
    saved = {}
    for number, step in enumerate(scenario["steps"], 1):
        label = "%s step %d (%s)" % (path.stem, number, step["method"])
        args = (step["method"], expand(step.get("params")))
        kwargs = {"wait": step.get("wait", False), "timeout": step.get("timeout")}
        if step.get("expect", "ok") == "error":
            error = avo.expect_error(*args, code=step.get("error_code"), **kwargs)
            if "message" in step:
                assert step["message"] in error.message, label
            continue

        result = avo.call(*args, **kwargs)
        if "result" in step:
            assert contains(result, step["result"]), "%s: wanted %s, got %s" % (
                label,
                step["result"],
                result,
            )
        for name, dotted in step.get("save", {}).items():
            saved[name] = dig(result, dotted)
    for check in scenario.get("checks", []):
        run_check(check, saved)
