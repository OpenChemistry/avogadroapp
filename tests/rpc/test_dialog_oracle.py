"""The dialog half of the liveness oracle (see README): after every step a
modal dialog fails the test with kind "dialog"."""

import json

import pytest

import harness


def test_active_dialog_reports_none_when_nothing_is_open(avo):
    assert avo.call("activeDialog") == {"open": False, "title": "", "className": ""}


def test_oracle_fails_the_test_when_a_dialog_is_open(fresh, monkeypatch):
    """A modal dialog after any step fails the test with kind "dialog" and the
    dialog's title. No stable command opens one, so the probe is made to report
    one. That taints the app, hence a launch of its own."""
    monkeypatch.setattr(
        harness,
        "probe",
        lambda name, timeout=harness.PING_TIMEOUT: (
            True,
            {"title": "Select Space Group", "className": "QDialog"},
        ),
    )
    with pytest.raises(pytest.fail.Exception) as failure:
        fresh.call("moleculeInfo")

    message = str(failure.value)
    assert "modal dialog" in message and "Select Space Group" in message
    assert fresh.app.tainted  # the next test gets a fresh process
    path = message.split("reproducer: ", 1)[1].splitlines()[0]
    document = json.loads(open(path).read())
    assert document["failure"] == "dialog"
    assert document["dialog"] == {"title": "Select Space Group", "className": "QDialog"}
    assert document["last_step"]["method"] == "moleculeInfo"


def test_oracle_ignores_the_progress_dialog(avo, monkeypatch):
    monkeypatch.setattr(
        harness,
        "probe",
        lambda name, timeout=harness.PING_TIMEOUT: (
            True,
            {"title": "Reading File", "className": "QProgressDialog"},
        ),
    )
    avo.call("moleculeInfo")  # no failure
