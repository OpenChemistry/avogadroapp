"""One focused test per built-in method of RpcListener / MainWindow."""

import base64
import json
import math
import re

import pytest

import harness
from harness import APP_ROOT

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

COMMAND_FAILED = -2
REQUEST_FAILED = -1
METHOD_NOT_FOUND = -32601


def builtin_names():
    """Names in the table at the top of rpclistener.cpp, so a newly added
    built-in is checked without anyone remembering to edit this file."""
    source = (APP_ROOT / "avogadro" / "rpclistener.cpp").read_text()
    table = source.split("builtinCommands[] = {", 1)[1].split("\n};", 1)[0]
    return re.findall(r'^  \{ "(\w+)",', table, re.MULTILINE)


# -- version / listCommands ---------------------------------------------
def test_version(avo):
    version = avo.call("version")
    assert {"avogadroApp", "avogadroLibs", "qt", "platform", "rpcProtocol"} <= set(version)
    assert version["rpcProtocol"] == 2
    assert version["platform"] in ("macos", "windows", "linux", "bsd")
    assert version["avogadroApp"] and version["qt"]


def test_ping(avo):
    assert avo.call("internalPing") == "pong"


def test_list_commands_has_every_builtin(avo):
    names = builtin_names()
    assert len(names) >= 20, "failed to parse the builtin table: %s" % names
    commands = avo.call("listCommands")
    by_name = {entry["name"]: entry for entry in commands}
    for name in names:
        assert name in by_name, "builtin %s missing from listCommands" % name
        assert by_name[name]["kind"] == "builtin"
        assert by_name[name]["plugin"] == ""
    # Sorted by name, and plugin commands are listed with their owner.
    listed = [entry["name"] for entry in commands]
    assert listed == sorted(listed)
    assert by_name["selectAll"]["kind"] == "extension"
    assert by_name["editDistance"]["kind"] == "tool"
    assert all({"name", "description", "kind", "plugin", "async"} <= set(e) for e in commands)


def test_unknown_method(avo):
    error = avo.expect_error("noSuchMethod", code=METHOD_NOT_FOUND)
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


def test_open_file_cjson_from_molecules(avo, molecules_dir):
    path = molecules_dir / "alkanes" / "butane.cjson"
    if not path.is_file():
        pytest.skip("%s missing" % path)
    avo.call("openFile", {"fileName": str(path)})
    assert avo.info()["formula"] == "C4H10"


def test_open_file_errors(avo, tmp_path):
    error = avo.expect_error(
        "openFile", {"fileName": str(tmp_path / "missing.xyz")}, code=REQUEST_FAILED
    )
    assert "Failed to read file" in error.message
    avo.expect_error("openFile", {}, code=REQUEST_FAILED)
    assert avo.app.alive()


# -- camera ------------------------------------------------------------
def test_camera_round_trip(avo):
    avo.load(ETHANE)
    camera = avo.call("getCamera")
    assert {"distance", "focus", "projection", "orthographicScale", "modelView"} <= set(camera)
    assert len(camera["modelView"]) == 16

    angle = math.radians(30)
    c, s = math.cos(angle), math.sin(angle)
    wanted = [c, -s, 0, 0, s, c, 0, 0, 0, 0, 1, camera["modelView"][11], 0, 0, 0, 1]
    returned = avo.call("setCamera", {"modelView": wanted})
    assert returned["modelView"] == pytest.approx(wanted, abs=1e-4)
    assert avo.call("getCamera")["modelView"] == pytest.approx(wanted, abs=1e-4)


def test_camera_projection(avo):
    avo.load(ETHANE)
    reply = avo.call("setCamera", {"projection": "orthographic", "orthographicScale": 2.5})
    assert reply["projection"] == "orthographic"
    assert reply["orthographicScale"] == pytest.approx(2.5)
    assert avo.call("getCamera")["projection"] == "orthographic"
    # Leaving everything out changes nothing.
    assert avo.call("setCamera", {})["projection"] == "orthographic"

    assert avo.call("setProjection", {"type": "perspective"}) is True
    assert avo.call("getCamera")["projection"] == "perspective"
    avo.call("setProjection", {"type": "orthographic"})
    assert avo.call("getCamera")["projection"] == "orthographic"


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


def test_set_render_types(avo):
    avo.load(ETHANE)
    assert display_types(avo)["Wireframe"]["enabled"] is False

    avo.call("setRenderTypes", {"Wireframe": True})
    assert display_types(avo)["Wireframe"]["enabled"] is True
    avo.call("setRenderTypes", {"Wireframe": False})
    assert display_types(avo)["Wireframe"]["enabled"] is False

    # The list form enables, and a display name works as well as the id.
    avo.call("setRenderTypes", {"types": ["Licorice", "Ball and Stick"]})
    types = display_types(avo)
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
    avo.expect_error("getLayerVisible", {"layer": 2}, code=COMMAND_FAILED, wait=True)


def test_layer_errors(avo):
    avo.load(ETHANE)
    for method in ("getLayerVisible", "getLayerLocked", "setActiveLayer", "removeLayer"):
        error = avo.expect_error(method, {"layer": 5}, code=COMMAND_FAILED)
        assert "out of range" in error.message
        avo.expect_error(method, {"layer": -1}, code=COMMAND_FAILED)
        avo.expect_error(method, {"layer": 0.5}, code=COMMAND_FAILED)
        assert "Missing" in avo.expect_error(method, {}, code=COMMAND_FAILED).message

    for value in ("yes", 1, None):
        error = avo.expect_error(
            "setLayerVisible", {"layer": 0, "visible": value}, code=COMMAND_FAILED
        )
        assert "true or false" in error.message
        avo.expect_error("setLayerLocked", {"layer": 0, "locked": value}, code=COMMAND_FAILED)
    avo.expect_error("setLayerVisible", {"layer": 0}, code=COMMAND_FAILED)

    # Never remove the last remaining layer.
    error = avo.expect_error("removeLayer", {"layer": 0}, code=COMMAND_FAILED)
    assert "last remaining layer" in error.message
    assert avo.call("getLayerVisible", {"layer": 0}, wait=True)["data"]["count"] == 1
