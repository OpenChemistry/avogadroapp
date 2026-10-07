"""Several molecules, the undo stack, tools and the "modified" flag.

The data-driven versions of the molecule sequences are in scenarios/; the tests
here need a launch argument, compare coordinates, or iterate over a list the
application reports.
"""

import json
import math
import os
import signal
import subprocess
import sys
import time

import pytest

import harness
from harness import RPCError
from test_builtins import ETHANE, WATER


def coordinates(avo):
    """Atom coordinates of the active molecule, from its cjson, flattened
    (x0, y0, z0, x1, ...) so that pytest.approx can compare them."""
    content = avo.call("getMolecule", {"format": "cjson"})["content"]
    return json.loads(content)["atoms"]["coords"]["3d"]


def distance(coords, i, j):
    return math.dist(coords[3 * i : 3 * i + 3], coords[3 * j : 3 * j + 3])


# -- avogadroapp #634 ----------------------------------------------------------
def test_launch_with_file_leaves_exactly_one_molecule(launch, butane):
    """Naming a file on the command line must not leave the blank startup
    document next to it. (This already held before #634 was fixed: with a file
    argument the window never creates the blank one. The case that did leave
    one behind is a file opened into a running window, covered by the
    open_replaces_empty_molecule_634 scenario.)"""
    avo = launch(str(butane))
    molecules = avo.wait_for(
        lambda m: any(entry["atomCount"] > 0 for entry in m), "the file to open"
    )
    time.sleep(1.0)  # a late blank document would show up by now
    molecules = avo.molecules()
    assert len(molecules) == 1, molecules
    assert molecules[0]["atomCount"] == 14
    assert molecules[0]["formula"] == "C4H10"
    assert molecules[0]["fileName"] == str(butane)
    assert molecules[0]["modified"] is False
    assert molecules[0]["active"] is True


@pytest.mark.skipif(sys.platform != "darwin", reason="Finder open events are macOS only")
def test_finder_open_event_replaces_the_blank_molecule(avogadro_exe, butane):
    """Double-clicking a file in Finder starts Avogadro with a blank document
    and then delivers the file as an open event (not a command line argument),
    the case behind #634. `open -a` reproduces it, with the RPC name passed as
    an argument. Nothing here goes through harness.Session: the process is
    started by LaunchServices, so it is found and stopped by its arguments.

    About one launch in ten never receives the event at all (the app stays on
    its blank document, with no dialog and nothing logged). There is nothing to
    assert about a replacement that never happened, so the test launches again,
    up to three times."""
    bundle = next((p for p in avogadro_exe.parents if p.suffix == ".app"), None)
    if bundle is None:
        pytest.skip("the executable is not inside a .app bundle")
    attempts = 3
    for attempt in range(attempts):
        name = "avotest-open-%d-%d" % (os.getpid(), attempt)
        subprocess.run(
            ["open", "-n", "-a", str(bundle), str(butane), "--args", "--rpc-name", name]
            + harness.LAUNCH_ARGS,
            check=True,
        )
        try:
            harness.wait_for_rpc(name)
            with harness.connect(name, timeout=10) as client:
                deadline = time.monotonic() + 10
                while time.monotonic() < deadline:
                    molecules = client.send("listMolecules")["result"]
                    if any(m["atomCount"] > 0 for m in molecules):
                        break
                    time.sleep(harness.POLL_INTERVAL)
                else:
                    continue  # the event never came: try another launch
                time.sleep(1.0)  # a late blank document would show up by now
                molecules = client.send("listMolecules")["result"]
            assert len(molecules) == 1, molecules
            assert (molecules[0]["formula"], molecules[0]["fileName"]) == ("C4H10", str(butane))
            return
        finally:
            found = subprocess.run(
                ["pgrep", "-f", "rpc-name " + name], capture_output=True, text=True
            ).stdout.split()
            for pid in found:
                try:
                    os.kill(int(pid), signal.SIGTERM)
                except OSError:
                    pass
    pytest.fail("macOS delivered no open event in %d launches" % attempts)


def test_launch_with_two_files_opens_both(launch, butane, molecules_dir):
    second = molecules_dir / "alkanes" / "decane.cjson"
    if not second.is_file():
        pytest.skip("%s missing" % second)
    avo = launch(str(butane), str(second))
    molecules = avo.wait_for(lambda m: len(m) >= 2, "both files to open")
    assert sorted(entry["formula"] for entry in molecules) == ["C10H22", "C4H10"]
    assert all(entry["atomCount"] > 0 for entry in molecules)


# -- undo and redo -------------------------------------------------------------
def test_edit_undo_redo_restores_geometry(avo):
    avo.load(ETHANE)
    original = coordinates(avo)
    assert distance(original, 0, 1) == pytest.approx(1.53, abs=1e-3)
    assert avo.info()["canUndo"] is False

    avo.data("editDistance", {"atoms": [0, 1], "value": 2.5})
    edited = coordinates(avo)
    assert distance(edited, 0, 1) == pytest.approx(2.5, abs=1e-3)
    assert avo.info()["canUndo"] is True
    # something to undo, nothing to redo
    avo.expect_error("redo", code=RPCError.COMMAND_FAILED)

    state = avo.data("undo")
    assert set(state) == {"canUndo", "canRedo", "undoText", "redoText", "modified"}
    assert state["canUndo"] is False and state["canRedo"] is True
    assert state["redoText"] == "Adjust Distance"
    assert state["modified"] is False
    assert coordinates(avo) == pytest.approx(original, abs=1e-5)

    state = avo.data("redo")
    assert state["canUndo"] is True and state["canRedo"] is False
    assert state["undoText"] == "Adjust Distance"
    assert state["modified"] is True
    assert coordinates(avo) == pytest.approx(edited, abs=1e-5)


def test_undo_stacks_are_per_molecule_and_include_layers(avo):
    """Undo, redo, new layers and molecule switches interleaved. Each molecule
    keeps its own stack, and undoing a layer removes it."""
    def layer_count():
        return avo.data("getLayerVisible", {"layer": 0})["count"]

    avo.load(ETHANE)
    first = coordinates(avo)
    avo.data("editDistance", {"atoms": [0, 1], "value": 2.0})
    avo.data("addLayer")
    assert layer_count() == 2

    avo.data("newMolecule")
    avo.load(WATER)
    water = coordinates(avo)
    avo.data("editDistance", {"atoms": [0, 1], "value": 1.5})
    avo.data("addLayer")
    avo.data("addLayer")
    assert layer_count() == 3

    state = avo.data("undo")  # water's second layer, not ethane's anything
    assert state["redoText"] == "Add Layer" and state["canUndo"] is True
    assert layer_count() == 2

    avo.data("setActiveMolecule", {"index": 0})  # ethane: edit, then a layer
    assert layer_count() == 2
    state = avo.info()
    assert state["canUndo"] is True and state["canRedo"] is False
    avo.data("undo")  # the layer
    assert layer_count() == 1
    avo.data("undo")  # the edit
    assert coordinates(avo) == pytest.approx(first, abs=1e-5)
    assert avo.info()["modified"] is False
    avo.data("redo")
    avo.data("redo")
    assert layer_count() == 2
    assert distance(coordinates(avo), 0, 1) == pytest.approx(2.0, abs=1e-3)

    avo.data("setActiveMolecule", {"index": 1})  # water kept its own stack
    assert layer_count() == 2
    assert avo.info()["canRedo"] is True
    avo.data("redo")
    assert layer_count() == 3
    avo.data("undo")
    avo.data("undo")
    avo.data("undo")
    assert coordinates(avo) == pytest.approx(water, abs=1e-5)
    assert avo.info()["modified"] is False

    avo.data("setActiveMolecule", {"index": 0})
    assert distance(coordinates(avo), 0, 1) == pytest.approx(2.0, abs=1e-3)
    assert [m["formula"] for m in avo.molecules()] == ["C2H6", "H2O"]


# -- avogadroapp #476 ----------------------------------------------------------
def test_modified_follows_edit_undo_redo(avo, butane):
    info = avo.info()
    assert (info["modified"], info["canUndo"], info["canRedo"]) == (False, False, False)

    avo.call("openFile", {"fileName": str(butane)})
    info = avo.info()
    assert info["fileName"] == str(butane)  # recorded the way File > Open does
    assert info["modified"] is False

    avo.data("editDistance", {"atoms": [0, 1], "value": 2.5})
    info = avo.info()
    assert (info["modified"], info["canUndo"], info["canRedo"]) == (True, True, False)
    assert avo.molecules()[0]["modified"] is True

    state = avo.data("undo")  # back at the start
    assert state["modified"] is False
    info = avo.info()
    assert (info["modified"], info["canUndo"], info["canRedo"]) == (False, False, True)
    assert avo.data("redo")["modified"] is True
    assert avo.info()["modified"] is True

    avo.load(ETHANE)  # loadMolecule has no file
    assert avo.info()["fileName"] == ""


def test_selection_effect_on_modified_is_reported(avo, butane, record_property):
    """Records, and deliberately does not assert, what selecting atoms does to
    the document's modified state: this is behaviour pending a product
    decision (should a selection be an undo step, and should it dirty the
    document?), so the test only pins that nothing dies. Run with -s, or read
    the junit properties."""
    def record(name, value):
        record_property(name, value)
        print("selection report: %s = %s" % (name, value))

    avo.call("openFile", {"fileName": str(butane)})
    avo.data("selectAll")
    info = avo.info()
    record("selectAll.modified", info["modified"])
    record("selectAll.canUndo", info["canUndo"])
    record("selectAll.selectedAtomCount", info["selectedAtomCount"])

    avo.data("selectNone")
    info = avo.info()
    record("selectNone.modified", info["modified"])
    record("selectNone.canUndo", info["canUndo"])

    if info["canUndo"]:
        state = avo.data("undo")
        record("undoSelection.modified", state["modified"])
        record("undoSelection.canUndoAfter", avo.info()["canUndo"])
        if state["canRedo"]:
            state = avo.data("redo")
            record("redoSelection.modified", state["modified"])
    assert avo.app.alive()


# -- molecule management ---------------------------------------------------------
def test_selecting_the_active_molecule_again_keeps_it_tracked(avo, butane):
    """Clicking the active molecule in the list calls setMolecule() with it
    again. That used to sever the connection that marks it modified, so a later
    edit never asked to be saved."""
    avo.call("openFile", {"fileName": str(butane)})
    avo.data("setActiveMolecule", {"index": 0})
    assert avo.info()["modified"] is False
    avo.data("editDistance", {"atoms": [0, 1], "value": 2.5})
    assert avo.info()["modified"] is True
    assert avo.data("undo")["modified"] is False
    avo.data("editDistance", {"atoms": [0, 1], "value": 2.2})
    assert avo.info()["modified"] is True


def test_switching_away_and_back_keeps_a_molecule_tracked(avo, butane):
    """Switching to another molecule and back runs the whole of setMolecule()
    (unlike re-selecting the active one, which returns early): the connection
    that marks the molecule modified must be made again, exactly once."""
    avo.call("openFile", {"fileName": str(butane)})
    avo.data("newMolecule")
    avo.data("setActiveMolecule", {"index": 0})
    assert avo.info()["modified"] is False
    avo.data("editDistance", {"atoms": [0, 1], "value": 2.5})
    assert avo.info()["modified"] is True
    assert avo.data("undo")["modified"] is False
    avo.data("editDistance", {"atoms": [0, 1], "value": 2.2})
    assert avo.info()["modified"] is True


# -- tools -------------------------------------------------------------------------
def test_activate_every_tool(fresh):
    fresh.load(ETHANE)
    tools = fresh.call("listTools")
    names = [tool["name"] for tool in tools]
    assert {"Navigator", "Editor", "Selection", "MeasureTool"} <= set(names)
    assert sum(tool["active"] for tool in tools) == 1

    for name in names:
        assert fresh.data("activateTool", {"name": name}) == {"tool": name}
        active = [t["name"] for t in fresh.call("listTools") if t["active"]]
        assert active == [name]
        # a plugin command between activations, with the tool active
        assert fresh.data("selectAll")["indices"] == list(range(8))
        fresh.data("selectNone")
    fresh.data("activateTool", {"name": "Navigator"})
    assert [t["name"] for t in fresh.call("listTools") if t["active"]] == ["Navigator"]
    assert fresh.call("activeDialog")["open"] is False
    assert fresh.info()["atomCount"] == 8
