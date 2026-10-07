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
from test_builtins import ETHANE, WATER, COMMAND_FAILED

BUTANE = "alkanes/butane.cjson"


def coordinates(avo):
    """Atom coordinates of the active molecule, from its cjson, flattened
    (x0, y0, z0, x1, ...) so that pytest.approx can compare them."""
    content = avo.call("getMolecule", {"format": "cjson"})["content"]
    return json.loads(content)["atoms"]["coords"]["3d"]


def distance(coords, i, j):
    return math.dist(coords[3 * i : 3 * i + 3], coords[3 * j : 3 * j + 3])


def butane(molecules_dir):
    path = molecules_dir / BUTANE
    if not path.is_file():
        pytest.skip("%s missing" % path)
    return path


# -- avogadroapp #634 ----------------------------------------------------------


def test_launch_with_file_leaves_exactly_one_molecule(launch, molecules_dir):
    """Naming a file on the command line must not leave the blank startup
    document next to it. (This already held before #634 was fixed: with a file
    argument the window never creates the blank one. The case that did leave
    one behind is a file opened into a running window, covered by the
    open_replaces_empty_molecule_634 scenario.)"""
    path = butane(molecules_dir)
    avo = launch(str(path))
    molecules = avo.wait_for(
        lambda m: any(entry["atomCount"] > 0 for entry in m), "the file to open"
    )
    time.sleep(1.0)  # a late blank document would show up by now
    molecules = avo.molecules()
    assert len(molecules) == 1, molecules
    assert molecules[0]["atomCount"] == 14
    assert molecules[0]["formula"] == "C4H10"
    assert molecules[0]["fileName"] == str(path)
    assert molecules[0]["modified"] is False
    assert molecules[0]["active"] is True


@pytest.mark.skipif(sys.platform != "darwin", reason="Finder open events are macOS only")
def test_finder_open_event_replaces_the_blank_molecule(
    avogadro_exe, molecules_dir, app_log_dir
):
    """Double-clicking a file in Finder starts Avogadro with a blank document
    and then delivers the file as an open event (not a command line argument),
    the case behind #634. `open -a` reproduces it, with the RPC name passed as
    an argument. Nothing here goes through harness.Session: the process is
    started by LaunchServices, so it is found and stopped by its arguments."""
    path = butane(molecules_dir)
    bundle = next((p for p in avogadro_exe.parents if p.suffix == ".app"), None)
    if bundle is None:
        pytest.skip("the executable is not inside a .app bundle")
    name = "avotest-open-%d" % os.getpid()
    subprocess.run(
        ["open", "-n", "-a", str(bundle), str(path), "--args", "--rpc-name", name]
        + ["--skip-autosave", "--disable-settings"],
        check=True,
    )
    try:
        deadline = time.monotonic() + 60
        client = None
        while client is None:
            try:
                client = harness.connect(name, timeout=10)
                client.molecule_info()
            except (ConnectionError, OSError, harness.RPCError):
                client = None
                assert time.monotonic() < deadline, "Avogadro did not start"
                time.sleep(0.5)
        with client:
            while True:
                molecules = client.send("listMolecules")["result"]
                if any(m["atomCount"] > 0 for m in molecules):
                    break
                assert time.monotonic() < deadline, "the file never opened: %s" % molecules
                time.sleep(0.25)
            time.sleep(1.0)
            molecules = client.send("listMolecules")["result"]
        assert len(molecules) == 1, molecules
        assert (molecules[0]["formula"], molecules[0]["fileName"]) == ("C4H10", str(path))
    finally:
        found = subprocess.run(
            ["pgrep", "-f", "rpc-name " + name], capture_output=True, text=True
        ).stdout.split()
        for pid in found:
            try:
                os.kill(int(pid), signal.SIGTERM)
            except OSError:
                pass


def test_launch_with_two_files_opens_both(launch, molecules_dir):
    path = butane(molecules_dir)
    second = molecules_dir / "alkanes" / "decane.cjson"
    if not second.is_file():
        pytest.skip("%s missing" % second)
    avo = launch(str(path), str(second))
    molecules = avo.wait_for(lambda m: len(m) >= 2, "both files to open")
    assert sorted(entry["formula"] for entry in molecules) == ["C10H22", "C4H10"]
    assert all(entry["atomCount"] > 0 for entry in molecules)


# -- molecule management ---------------------------------------------------------
def test_selecting_the_active_molecule_again_keeps_it_tracked(avo, molecules_dir):
    """Clicking the active molecule in the list calls setMolecule() with it
    again. That used to sever the connection that marks it modified, so a later
    edit never asked to be saved."""
    avo.call("openFile", {"fileName": str(butane(molecules_dir))})
    avo.data("setActiveMolecule", {"index": 0})
    assert avo.info()["modified"] is False
    avo.data("editDistance", {"atoms": [0, 1], "value": 2.5})
    assert avo.info()["modified"] is True
    assert avo.data("undo")["modified"] is False
    avo.data("editDistance", {"atoms": [0, 1], "value": 2.2})
    assert avo.info()["modified"] is True
