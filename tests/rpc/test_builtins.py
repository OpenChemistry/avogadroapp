"""One focused test per built-in method of RpcListener / MainWindow."""

import base64
import json
import math

import pytest

import harness
from harness import REQUEST_FAILED, RPCError

ETHANE = """8
ethane
C   0.000000   0.000000   0.765000
C   0.000000   0.000000  -0.765000
H   1.019000   0.000000   1.157000
H  -0.509500   0.882000   1.157000
H  -0.509500  -0.882000   1.157000
H  -1.019000   0.000000  -1.157000
H   0.509500  -0.882000  -1.157000
H   0.509500   0.882000  -1.157000
"""

WATER = """3
water
O   0.000000   0.000000   0.117300
H   0.000000   0.757200  -0.469200
H   0.000000  -0.757200  -0.469200
"""


# -- version / listCommands ---------------------------------------------
def test_version(avo):
    version = avo.call("version")
    assert {"avogadroApp", "avogadroLibs", "qt", "platform", "rpcProtocol"} <= set(version)
    assert version["rpcProtocol"] == 2
    assert version["platform"] in ("macos", "windows", "linux", "bsd")
    assert version["avogadroApp"] and version["qt"]


def test_ping(avo):
    assert avo.call("internalPing") == "pong"


def test_list_commands(avo):
    commands = avo.call("listCommands")
    by_name = {entry["name"]: entry for entry in commands}
    # Sorted by name, and plugin commands are listed with their owner.
    listed = [entry["name"] for entry in commands]
    assert listed == sorted(listed)
    assert all({"name", "description", "kind", "plugin", "async"} <= set(e) for e in commands)
    assert all(e["plugin"] == "" for e in commands if e["kind"] == "builtin")
    assert by_name["listCommands"]["kind"] == "builtin"
    assert by_name["selectAll"]["kind"] == "extension"
    assert by_name["editDistance"]["kind"] == "tool"


def test_every_builtin_command_is_answered(fresh):
    """Every built-in that listCommands reports is known: with no parameters it
    answers, with a result or an error of its own, never "Method not found".
    A newly added built-in is covered without editing this file.

    Only "kill" is left out: it is refused without --testing, which the app
    cannot be given (see README). Nothing else needs excluding. With empty
    parameters each verb either fails validation or does something harmless
    (a read-back, a new blank molecule, closing the blank one), and none of them
    opens a dialog: the oracle would fail the test if one did. The app is
    launched for this test alone because some of the calls change it."""
    builtins = [e["name"] for e in fresh.call("listCommands") if e["kind"] == "builtin"]
    assert len(builtins) >= 20, builtins
    assert "kill" in builtins
    for name in builtins:
        if name == "kill":
            continue
        try:
            fresh.call(name)
        except RPCError as error:
            assert error.code != RPCError.METHOD_NOT_FOUND, name


def test_unknown_method(avo):
    error = avo.expect_error("noSuchMethod", code=RPCError.METHOD_NOT_FOUND)
    assert error.message == "Method not found"


def test_kill_is_refused_without_testing_flag(avo):
    avo.expect_error("kill", code=REQUEST_FAILED)
    assert avo.app.alive()


# -- molecule in / out -----------------------------------------------------
def test_molecule_info_on_startup_molecule(avo):
    info = avo.info()
    assert info["atomCount"] == 0
    assert info["bondCount"] == 0
    assert info["formula"] == ""
    assert info["spinMultiplicity"] == 1
    assert info["totalCharge"] == 0
    assert info["hasBasisSet"] is False
    assert info["homoIndex"] == -1
    assert info["selectedAtomCount"] == 0
    assert info["hasUnitCell"] is False
    assert (info["modified"], info["canUndo"], info["canRedo"]) == (False, False, False)


def test_load_molecule_then_info(avo):
    assert avo.load(ETHANE, "xyz") is True
    info = avo.info()
    assert (info["atomCount"], info["bondCount"]) == (8, 7)
    assert info["formula"] == "C2H6"
    assert info["mass"] == pytest.approx(30.069, abs=1e-3)

    avo.load(WATER, "xyz")
    info = avo.info()
    assert (info["atomCount"], info["bondCount"], info["formula"]) == (3, 2, "H2O")


def test_load_molecule_garbage_is_an_error(avo):
    avo.expect_error("loadMolecule", {"content": "garbage", "format": "cjson"}, code=REQUEST_FAILED)
    avo.expect_error("loadMolecule", {"content": ETHANE, "format": "nosuchformat"}, code=REQUEST_FAILED)
    avo.expect_error("loadMolecule", {"content": ETHANE}, code=REQUEST_FAILED)  # no format
    assert avo.info()["atomCount"] == 0  # the failed loads changed nothing


def test_get_molecule_cjson_round_trip(avo):
    avo.load(ETHANE, "xyz")
    reply = avo.call("getMolecule", {"format": "cjson"})
    assert reply["format"] == "cjson"
    document = json.loads(reply["content"])
    assert document["chemicalJson"] == 1
    assert len(document["atoms"]["elements"]["number"]) == 8

    avo.load(WATER, "xyz")  # replace it, then bring ethane back
    avo.load(reply["content"], "cjson")
    info = avo.info()
    assert (info["atomCount"], info["bondCount"], info["formula"]) == (8, 7, "C2H6")


def test_get_molecule_xyz_and_bad_format(avo):
    avo.load(ETHANE, "xyz")
    content = avo.call("getMolecule", {"format": "xyz"})["content"]
    assert content.splitlines()[0].strip() == "8"
    avo.expect_error("getMolecule", {"format": "nosuch"}, code=REQUEST_FAILED)


def test_open_file(avo, data_dir):
    path = data_dir / "xyz" / "H2O.xyz"
    if not path.is_file():
        pytest.skip("%s missing" % path)
    assert avo.call("openFile", {"fileName": str(path)}) is True
    info = avo.info()
    assert (info["atomCount"], info["bondCount"], info["formula"]) == (3, 2, "H2O")


def test_open_file_cjson_from_molecules(avo, butane):
    avo.call("openFile", {"fileName": str(butane)})
    assert avo.info()["formula"] == "C4H10"


def test_open_file_errors(avo, tmp_path):
    error = avo.expect_error(
        "openFile", {"fileName": str(tmp_path / "missing.xyz")}, code=REQUEST_FAILED
    )
    assert "Failed to read file" in error.message
    avo.expect_error("openFile", {}, code=REQUEST_FAILED)
    assert avo.app.alive()


# -- camera ------------------------------------------------------------
def test_camera_round_trip(fresh):
    fresh.load(ETHANE)
    camera = fresh.call("getCamera")
    assert {"distance", "focus", "projection", "orthographicScale", "modelView"} <= set(camera)
    assert len(camera["modelView"]) == 16

    angle = math.radians(30)
    c, s = math.cos(angle), math.sin(angle)
    wanted = [c, -s, 0, 0, s, c, 0, 0, 0, 0, 1, camera["modelView"][11], 0, 0, 0, 1]
    returned = fresh.call("setCamera", {"modelView": wanted})
    assert returned["modelView"] == pytest.approx(wanted, abs=1e-4)
    assert fresh.call("getCamera")["modelView"] == pytest.approx(wanted, abs=1e-4)


def test_camera_projection(fresh):
    fresh.load(ETHANE)
    reply = fresh.call("setCamera", {"projection": "orthographic", "orthographicScale": 2.5})
    assert reply["projection"] == "orthographic"
    assert reply["orthographicScale"] == pytest.approx(2.5)
    assert fresh.call("getCamera")["projection"] == "orthographic"
    # Leaving everything out changes nothing.
    assert fresh.call("setCamera", {})["projection"] == "orthographic"

    assert fresh.call("setProjection", {"type": "perspective"}) is True
    assert fresh.call("getCamera")["projection"] == "perspective"
    fresh.call("setProjection", {"type": "orthographic"})
    assert fresh.call("getCamera")["projection"] == "orthographic"


def test_set_camera_rejects_wrong_matrix_size(avo):
    avo.expect_error("setCamera", {"modelView": [1.0] * 15}, code=REQUEST_FAILED)


# -- display types ---------------------------------------------------------
def display_types(avo):
    return {entry["name"]: entry for entry in avo.call("listDisplayTypes")}


def test_list_display_types(avo):
    types = display_types(avo)
    assert types
    for entry in types.values():
        assert {"name", "displayName", "enabled", "applicable", "hasSettings"} <= set(entry)
    assert types["BallStick"]["displayName"] == "Ball and Stick"
    assert "Wireframe" in types


def test_set_render_types(fresh):
    fresh.load(ETHANE)
    assert display_types(fresh)["Wireframe"]["enabled"] is False

    fresh.call("setRenderTypes", {"Wireframe": True})
    assert display_types(fresh)["Wireframe"]["enabled"] is True
    fresh.call("setRenderTypes", {"Wireframe": False})
    assert display_types(fresh)["Wireframe"]["enabled"] is False

    # The list form enables, and a display name works as well as the id.
    fresh.call("setRenderTypes", {"types": ["Licorice", "Ball and Stick"]})
    types = display_types(fresh)
    assert types["Licorice"]["enabled"] and types["BallStick"]["enabled"]


# -- images ------------------------------------------------------------
def test_render_image_inline(avo):
    avo.load(ETHANE)
    result = avo.call("renderImage", {"width": 64, "height": 48})
    assert result["format"] == "png"
    assert (result["width"], result["height"]) == (64, 48)
    assert result["nativeWidth"] > 0 and result["nativeHeight"] > 0
    data = base64.b64decode(result["data"])
    assert harness.png_size(data) == (64, 48)


def test_render_image_native_size(avo):
    avo.load(ETHANE)
    result = avo.call("renderImage")
    assert (result["width"], result["height"]) == (
        result["nativeWidth"],
        result["nativeHeight"],
    )
    assert harness.png_size(base64.b64decode(result["data"])) == (
        result["width"],
        result["height"],
    )


def test_render_image_needs_both_dimensions(avo):
    avo.load(ETHANE)
    avo.expect_error("renderImage", {"width": 64}, code=REQUEST_FAILED)
    avo.expect_error("renderImage", {"height": 64}, code=REQUEST_FAILED)


def test_render_image_to_file(avo, tmp_path):
    avo.load(ETHANE)
    path = tmp_path / "view"  # no extension: .png is appended
    result = avo.call("renderImage", {"width": 40, "height": 30, "fileName": str(path)})
    assert result["fileName"] == str(path) + ".png"
    assert "data" not in result
    assert harness.png_size((tmp_path / "view.png").read_bytes()) == (40, 30)
    avo.expect_error(
        "renderImage",
        {"width": 40, "height": 30, "fileName": str(tmp_path / "no" / "such" / "dir.png")},
        code=REQUEST_FAILED,
    )


def test_save_graphic(avo, tmp_path):
    avo.load(ETHANE)
    path = tmp_path / "graphic.png"
    assert avo.call("saveGraphic", {"fileName": str(path)}) is True
    width, height = harness.png_size(path.read_bytes())
    assert width > 0 and height > 0


# -- export ------------------------------------------------------------
def test_export_file_wait(avo, tmp_path):
    avo.load(ETHANE)
    path = tmp_path / "ethane.xyz"
    result = avo.call("exportFile", {"fileName": str(path)}, wait=True)
    assert result == {"status": "finished", "data": {"fileName": str(path)}}
    assert path.read_text().splitlines()[0].strip() == "8"

    # The file we wrote can be read straight back in.
    avo.load(WATER)
    avo.call("openFile", {"fileName": str(path)})
    assert avo.info()["formula"] == "C2H6"

    cjson = tmp_path / "ethane.cjson"
    avo.call("exportFile", {"fileName": str(cjson)}, wait=True)
    assert json.loads(cjson.read_text())["chemicalJson"] == 1


def test_export_file_unknown_extension(avo, tmp_path):
    avo.load(ETHANE)
    avo.expect_error(
        "exportFile", {"fileName": str(tmp_path / "x.nosuchext")}, code=REQUEST_FAILED, wait=True
    )


def test_export_file_failure_is_an_error_not_a_dialog(avo, tmp_path):
    """A failed write must not open "Error saving file" (--rpc-name implies
    --skip-dialogs): the waited request fails fast with the writer's error and
    the writer state is released for the next export."""
    avo.load(ETHANE)
    bad = tmp_path / "no_such_directory" / "ethane.xyz"
    error = avo.expect_error(
        "exportFile", {"fileName": str(bad)}, code=RPCError.COMMAND_FAILED, wait=True, timeout=15
    )
    assert error.message  # the writer's own text, not the generic fallback
    assert error.message != "The command failed."
    assert error.message != "The command timed out."
    assert not bad.exists()

    # The same goes for a fire-and-forget export: it starts, and a later
    # export is not refused afterwards.
    assert avo.call("exportFile", {"fileName": str(bad)}) is True

    good = tmp_path / "ethane.xyz"
    result = avo.call("exportFile", {"fileName": str(good)}, wait=True, timeout=15)
    assert result == {"status": "finished", "data": {"fileName": str(good)}}
    assert good.read_text().splitlines()[0].strip() == "8"
    assert avo.app.alive()


def test_save_graphic_failure_is_an_error_not_a_dialog(avo, tmp_path):
    avo.load(ETHANE)
    bad = tmp_path / "no_such_directory" / "graphic.png"
    avo.expect_error("saveGraphic", {"fileName": str(bad)}, code=REQUEST_FAILED, timeout=15)
    good = tmp_path / "graphic.png"
    assert avo.call("saveGraphic", {"fileName": str(good)}) is True
    assert good.exists()


# -- layers ------------------------------------------------------------
def test_layer_lifecycle(avo):
    avo.load(ETHANE)
    # Without wait the reply is a bare True; with wait it carries data.
    assert avo.call("addLayer") is True
    added = avo.call("addLayer", wait=True)
    assert added == {"status": "finished", "data": {"layer": 2, "count": 3}}

    active = avo.call("setActiveLayer", {"layer": 1}, wait=True)
    assert active["data"] == {"layer": 1, "count": 3}

    visible = avo.call("setLayerVisible", {"layer": 1, "visible": False}, wait=True)
    assert visible["data"] == {"layer": 1, "visible": False, "count": 3}
    assert avo.call("getLayerVisible", {"layer": 1}, wait=True)["data"]["visible"] is False
    assert avo.call("getLayerVisible", {"layer": 0}, wait=True)["data"]["visible"] is True

    locked = avo.call("setLayerLocked", {"layer": 2, "locked": True}, wait=True)
    assert locked["data"] == {"layer": 2, "locked": True, "count": 3}
    assert avo.call("getLayerLocked", {"layer": 2}, wait=True)["data"]["locked"] is True
    assert avo.call("getLayerLocked", {"layer": 0}, wait=True)["data"]["locked"] is False

    assert avo.call("removeLayer", {"layer": 1}, wait=True)["data"] == {"count": 2}
    # The old layer 2 slid down to index 1 and kept its lock.
    assert avo.call("getLayerLocked", {"layer": 1}, wait=True)["data"]["locked"] is True
    avo.expect_error("getLayerVisible", {"layer": 2}, code=RPCError.COMMAND_FAILED, wait=True)


LAYER_VERBS = [
    ("getLayerVisible", {}),
    ("getLayerLocked", {}),
    ("setActiveLayer", {}),
    ("removeLayer", {}),
    ("setLayerVisible", {"visible": True}),
    ("setLayerLocked", {"locked": True}),
]


@pytest.mark.parametrize("method, extra", LAYER_VERBS)
@pytest.mark.parametrize(
    "layer, expected",
    [
        (5, "out of range"),
        (-1, "out of range"),
        (0.5, "must be a whole number"),
        (1e20, "must be a whole number"),
        # not numbers, although QVariant would convert them
        ("0", "must be a whole number"),
        (True, "must be a whole number"),
    ],
    ids=["too-big", "negative", "fractional", "huge", "string", "boolean"],
)
def test_layer_index_errors(avo, method, extra, layer, expected):
    avo.load(ETHANE)
    error = avo.expect_error(method, dict(extra, layer=layer), code=RPCError.COMMAND_FAILED)
    assert expected in error.message
    assert avo.data("getLayerVisible", {"layer": 0})["count"] == 1  # nothing happened


@pytest.mark.parametrize("method, extra", LAYER_VERBS)
def test_layer_index_is_required(avo, method, extra):
    avo.load(ETHANE)
    error = avo.expect_error(method, extra, code=RPCError.COMMAND_FAILED)
    assert "Missing" in error.message


@pytest.mark.parametrize(
    "method, key", [("setLayerVisible", "visible"), ("setLayerLocked", "locked")]
)
@pytest.mark.parametrize("value", ["yes", 1, None, "missing"])
def test_layer_flag_must_be_a_boolean(avo, method, key, value):
    avo.load(ETHANE)
    params = {"layer": 0} if value == "missing" else {"layer": 0, key: value}
    error = avo.expect_error(method, params, code=RPCError.COMMAND_FAILED)
    assert "true or false" in error.message


def test_cannot_remove_the_last_layer(avo):
    avo.load(ETHANE)
    error = avo.expect_error("removeLayer", {"layer": 0}, code=RPCError.COMMAND_FAILED)
    assert "last remaining layer" in error.message
    assert avo.call("getLayerVisible", {"layer": 0}, wait=True)["data"]["count"] == 1


# -- molecules ---------------------------------------------------------------
# (Sequences of several molecules are the scenarios/; what is here is one verb
# and its errors.)
def test_list_molecules_on_startup(avo):
    molecules = avo.call("listMolecules")
    assert len(molecules) == 1
    assert molecules[0] == {
        "index": 0,
        "active": True,
        "atomCount": 0,
        "formula": "",
        "fileName": "",
        "modified": False,
    }


def test_new_molecule_and_set_active(avo):
    assert avo.call("newMolecule") is True  # without wait the reply is a bare true
    avo.load(WATER)  # fills the new, empty one
    assert avo.data("newMolecule") == {"index": 2, "count": 3}
    molecules = avo.molecules()
    assert [m["index"] for m in molecules] == [0, 1, 2]
    assert [m["active"] for m in molecules] == [False, False, True]
    assert [m["atomCount"] for m in molecules] == [0, 3, 0]

    assert avo.data("setActiveMolecule", {"index": 1}) == {"index": 1, "count": 3}
    assert avo.info()["formula"] == "H2O"
    assert avo.data("setActiveMolecule", {"index": 1}) == {"index": 1, "count": 3}


OUT_OF_RANGE = [-1, 1, 99]  # one molecule is open
NOT_WHOLE_NUMBERS = [0.5, 1e20, "0", None, True, [0]]


@pytest.mark.parametrize("method", ["setActiveMolecule", "closeMolecule"])
@pytest.mark.parametrize("index", OUT_OF_RANGE)
def test_molecule_index_out_of_range(avo, method, index):
    error = avo.expect_error(method, {"index": index}, code=RPCError.COMMAND_FAILED)
    assert error.message == "'index' %d is out of range (1 molecules are open)." % index
    assert [m["active"] for m in avo.molecules()] == [True]  # nothing changed


@pytest.mark.parametrize("method", ["setActiveMolecule", "closeMolecule"])
@pytest.mark.parametrize("index", NOT_WHOLE_NUMBERS, ids=repr)
def test_molecule_index_must_be_a_number(avo, method, index):
    error = avo.expect_error(method, {"index": index}, code=RPCError.COMMAND_FAILED)
    assert error.message == "'index' must be a whole number."
    assert [m["active"] for m in avo.molecules()] == [True]


def test_set_active_molecule_needs_an_index(avo):
    avo.data("newMolecule")
    error = avo.expect_error("setActiveMolecule", code=RPCError.COMMAND_FAILED)
    assert error.message == "Missing 'index' parameter."
    assert [m["active"] for m in avo.molecules()] == [False, True]


def test_close_molecule_refuses_unsaved_changes(avo):
    avo.load(ETHANE)
    avo.data("removeAllHydrogens")  # an edit: the molecule is now modified
    assert avo.info()["modified"] is True
    # Only a real true discards.
    for params in ({}, {"index": 0}, {"discard": False}, {"discard": "yes"}, {"discard": 1}):
        error = avo.expect_error("closeMolecule", params, code=RPCError.COMMAND_FAILED)
        assert error.message == (
            "The molecule has unsaved changes; pass discard: true to close it anyway."
        )
    # nothing changed
    molecules = avo.molecules()
    assert len(molecules) == 1
    assert (molecules[0]["atomCount"], molecules[0]["modified"]) == (2, True)

    # closing the last molecule leaves a new blank one
    assert avo.data("closeMolecule", {"discard": True}) == {"index": 0, "count": 1}
    molecules = avo.molecules()
    assert (molecules[0]["atomCount"], molecules[0]["modified"]) == (0, False)


@pytest.mark.parametrize(
    "method, message",
    [("undo", "There is nothing to undo."), ("redo", "There is nothing to redo.")],
)
@pytest.mark.parametrize("molecule", ["blank", "loaded"])
def test_undo_redo_with_nothing_to_do(avo, method, message, molecule):
    if molecule == "loaded":
        avo.load(ETHANE)
    error = avo.expect_error(method, code=RPCError.COMMAND_FAILED)
    assert error.message == message
    avo.expect_error(method, code=RPCError.COMMAND_FAILED, wait=True)


def test_list_tools_and_activate_tool_errors(fresh):
    tools = fresh.call("listTools")
    assert tools
    assert all({"name", "displayName", "active"} <= set(tool) for tool in tools)
    by_name = {tool["name"]: tool for tool in tools}
    # The empty startup molecule gets the editor.
    assert by_name["Editor"]["active"] is True
    assert by_name["Navigator"]["displayName"] != by_name["Navigator"]["name"]

    assert fresh.data("activateTool", {"name": "Navigator"}) == {"tool": "Navigator"}
    for params in ({}, {"name": ""}, {"name": "NoSuchTool"}, {"name": "navigator"}, {"name": 3}):
        error = fresh.expect_error("activateTool", params, code=RPCError.COMMAND_FAILED)
        assert "Unknown tool" in error.message
    # The display name is not accepted: only listTools' "name" is.
    fresh.expect_error(
        "activateTool", {"name": by_name["Navigator"]["displayName"]}, code=RPCError.COMMAND_FAILED
    )
    assert [t["name"] for t in fresh.call("listTools") if t["active"]] == ["Navigator"]
