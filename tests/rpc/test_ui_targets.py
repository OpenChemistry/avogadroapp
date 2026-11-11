"""The UI target verbs: listUiTargets, highlightTarget, revealTarget and
clearHighlight (TargetRegistry in avogadro/targetregistry.cpp).

These change app-wide state (the active tool, the overlay), so every test uses
a `fresh` app.

Not covered: the translated "text" of each target. The harness launches with
--disable-settings and has no locale option, so a non-English run is a manual
checklist item.
"""

import pytest

from harness import REQUEST_FAILED

# Every v1 target except action:optimize, which resolves only once the
# Optimize Geometry action has an object name (GTut PR 2).
TOOL_TARGETS = [
    "tool:navigate",
    "tool:draw",
    "tool:template",
    "tool:select",
    "tool:manipulate",
    "tool:measure",
]
EDITOR_TARGETS = [
    "editor:element",
    "editor:bondOrder",
    "editor:adjustHydrogens",
    "editor:adjustBondLengths",
]
V1_TARGETS = (
    ["view", "tool"] + TOOL_TARGETS + ["action:new", "action:open"] + EDITOR_TARGETS
)


def list_targets(avo):
    targets = avo.call("listUiTargets")
    assert isinstance(targets, dict)
    return targets


def test_verbs_are_listed(fresh):
    names = {entry["name"] for entry in fresh.call("listCommands")}
    assert {
        "listUiTargets",
        "highlightTarget",
        "revealTarget",
        "clearHighlight",
    } <= names


def test_every_v1_target_resolves(fresh):
    targets = list_targets(fresh)
    for name in V1_TARGETS:
        assert name in targets, name
        entry = targets[name]
        assert set(entry) == {"text", "value", "resolved", "visible"}, name
        assert entry["resolved"] is True, name
    # Everything with a widget has text to show; "tool" is state only.
    for name in V1_TARGETS:
        if name != "tool":
            assert targets[name]["text"], name
    # The tool buttons and the 3D view are showing at startup; the Editor's
    # settings are not, until the Draw tool is active.
    for name in ["view"] + TOOL_TARGETS + ["action:new", "action:open"]:
        assert targets[name]["visible"] is True, name
    assert targets["action:new"]["text"].count("▸") >= 1
    assert isinstance(targets["tool"]["value"], str)


@pytest.mark.xfail(reason="GTut PR 2: objectName", strict=False)
def test_optimize_target_resolves(fresh):
    entry = list_targets(fresh)["action:optimize"]
    assert entry["resolved"] is True


def test_editor_targets_follow_the_active_tool(fresh):
    fresh.call("activateTool", {"name": "Navigator"})
    targets = list_targets(fresh)
    assert targets["tool"]["value"] == "navigate"
    for name in EDITOR_TARGETS:
        assert targets[name]["resolved"] is True, name
        assert targets[name]["visible"] is False, name

    fresh.call("activateTool", {"name": "Editor"})
    targets = list_targets(fresh)
    assert targets["tool"]["value"] == "draw"
    for name in EDITOR_TARGETS:
        assert targets[name]["visible"] is True, name
        assert targets[name]["text"], name
    element = targets["editor:element"]["value"]
    assert isinstance(element, int) and 1 <= element < 255
    assert targets["editor:bondOrder"]["value"] in (0, 1, 2, 3)
    assert isinstance(targets["editor:adjustHydrogens"]["value"], bool)
    assert isinstance(targets["editor:adjustBondLengths"]["value"], bool)


def test_highlight_visible_targets(fresh):
    for name in ["view"] + TOOL_TARGETS + ["action:new", "action:open"]:
        assert fresh.call("highlightTarget", {"target": name}) is True, name
    assert (
        fresh.call("highlightTarget", {"target": "view", "callout": "Look here"})
        is True
    )

    fresh.call("activateTool", {"name": "Editor"})
    for name in EDITOR_TARGETS:
        assert fresh.call("highlightTarget", {"target": name}) is True, name

    assert fresh.call("clearHighlight") is True
    # Clearing again, with nothing shown, is fine.
    assert fresh.call("clearHighlight") is True


def test_highlight_errors(fresh):
    fresh.call("activateTool", {"name": "Navigator"})

    error = fresh.expect_error("highlightTarget", {"target": "no:such"}, REQUEST_FAILED)
    assert "Unknown" in error.message
    # An unresolved or state-only name is refused with its own reason.
    fresh.expect_error("highlightTarget", {"target": "tool"}, REQUEST_FAILED)
    fresh.expect_error("highlightTarget", {"target": "action:nope"}, REQUEST_FAILED)
    fresh.expect_error("highlightTarget", {"target": ""}, REQUEST_FAILED)

    # The Editor's settings are not showing while Navigator is active.
    error = fresh.expect_error(
        "highlightTarget", {"target": "editor:element"}, REQUEST_FAILED
    )
    assert "Draw" in error.message

    # Strict parameter types.
    fresh.expect_error("highlightTarget", None, REQUEST_FAILED)
    fresh.expect_error("highlightTarget", {}, REQUEST_FAILED)
    fresh.expect_error("highlightTarget", {"target": 3}, REQUEST_FAILED)
    fresh.expect_error("highlightTarget", {"target": True}, REQUEST_FAILED)
    fresh.expect_error("highlightTarget", {"target": ["view"]}, REQUEST_FAILED)
    fresh.expect_error(
        "highlightTarget", {"target": "view", "callout": 5}, REQUEST_FAILED
    )
    fresh.expect_error("revealTarget", {}, REQUEST_FAILED)
    fresh.expect_error("revealTarget", {"target": 1}, REQUEST_FAILED)
    fresh.expect_error("revealTarget", {"target": "no:such"}, REQUEST_FAILED)
    fresh.expect_error("revealTarget", {"target": "tool"}, REQUEST_FAILED)


def test_reveal_menu_action_does_not_block(fresh):
    # A menu is a popup, not a modal dialog: nothing waits on it.
    assert fresh.call("revealTarget", {"target": "action:open"}) is True
    assert fresh.call("activeDialog")["open"] is False
    assert fresh.call("clearHighlight") is True
    assert fresh.call("activeDialog")["open"] is False
    assert fresh.app.alive()


def test_reveal_widgets(fresh):
    assert fresh.call("revealTarget", {"target": "view"}) is True
    assert fresh.call("revealTarget", {"target": "tool:measure"}) is True
    fresh.call("activateTool", {"name": "Editor"})
    assert fresh.call("revealTarget", {"target": "editor:bondOrder"}) is True
    assert fresh.call("activeDialog")["open"] is False
    assert fresh.call("clearHighlight") is True


def test_tools_in_the_toolbar_overflow(narrow):
    # Some tool buttons are hidden behind the tool bar's "»" button; they can
    # still be highlighted (at the "»" button) and revealed (it is opened).
    for name in TOOL_TARGETS:
        assert narrow.call("highlightTarget", {"target": name}) is True, name
        assert narrow.call("revealTarget", {"target": name}) is True, name

    assert narrow.call("revealTarget", {"target": "tool:measure"}) is True
    assert narrow.call("highlightTarget", {"target": "tool:draw"}) is True
    assert narrow.call("clearHighlight") is True

    # The overflow opens and closes later, so repeated calls must not lose the
    # target: the same tool twice, then another hidden tool right after.
    assert narrow.call("revealTarget", {"target": "tool:measure"}) is True
    assert narrow.call("revealTarget", {"target": "tool:measure"}) is True
    assert narrow.call("revealTarget", {"target": "tool:measure"}) is True
    assert narrow.call("highlightTarget", {"target": "tool:playertool"}) is True
    assert narrow.call("clearHighlight") is True

    targets = list_targets(narrow)
    for name in TOOL_TARGETS:
        assert targets[name]["visible"] is True, name
    assert narrow.call("activeDialog")["open"] is False
