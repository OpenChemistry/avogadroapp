"""The scripted selection and editing verbs: selectAtoms (select plugin) and
addAtom, addBond, removeBond, removeSelectedAtoms (Editor tool), merged in
avogadrolibs #3135.

The contract, from avogadro/qtplugins/select/select.cpp and
avogadro/qtplugins/editor/editor.cpp: a recognised command with bad options
fails with COMMAND_FAILED (-2) and changes nothing, not even the undo stack; a
success replies {"indices": [...]} (selectAtoms), {"index": n} (addAtom,
addBond), {"removed": n} (removeSelectedAtoms) or no data at all (removeBond).
Every edit is one undo step. A locked active layer refuses every edit, and so
does a locked layer holding an atom that the edit bonds to, unbonds or deletes.

The mouse handlers share the lock checks, but nothing here can click.
"""

import json

import pytest

from harness import RPCError
from test_builtins import ETHANE

FAILED = RPCError.COMMAND_FAILED

# ETHANE: atoms 0, 1 are carbons, 2-7 hydrogens; 7 bonds (C-C and six C-H),
# so the hydrogens 2 and 3 are not bonded to each other.
ETHANE_ATOMS = 8
ETHANE_BONDS = 7


# -- helpers -------------------------------------------------------------------
def bonds(avo):
    """The bonds of the active molecule as sorted ((i, j), order) pairs."""
    content = avo.call("getMolecule", {"format": "cjson"})["content"]
    table = json.loads(content).get("bonds", {})
    flat = table.get("connections", {}).get("index", [])
    orders = table.get("order", [])
    pairs = [tuple(sorted(flat[k : k + 2])) for k in range(0, len(flat), 2)]
    return sorted(zip(pairs, orders))


def coords(avo):
    content = avo.call("getMolecule", {"format": "cjson"})["content"]
    return json.loads(content).get("atoms", {}).get("coords", {}).get("3d", [])


KEYS = ("atomCount", "bondCount", "selectedAtomCount", "formula", "canUndo", "canRedo", "modified")


def state(avo):
    """Everything an edit could change, including the undo stack."""
    info = avo.info()
    return {key: info[key] for key in KEYS}, coords(avo), bonds(avo)


def model(avo):
    """The molecule without the undo stack: what undo/redo must restore."""
    info = avo.info()
    return {key: info[key] for key in KEYS[:4]}, coords(avo), bonds(avo)


def selected(avo):
    """How many atoms are selected, read back without changing anything."""
    return avo.info()["selectedAtomCount"]


def assert_refused(avo, method, options, fragment, wait=False):
    """The verb fails with COMMAND_FAILED, names `fragment`, and the molecule
    and the undo stack are exactly as they were."""
    before = state(avo)
    error = avo.expect_error(method, options, code=FAILED, wait=wait)
    assert fragment in error.message, error.message
    assert state(avo) == before


def undo_redo_roundtrip(avo, before, after, text):
    """undo returns to `before`, redo returns to `after` (both from model())."""
    undone = avo.data("undo")
    assert undone["redoText"] == text
    assert model(avo) == before
    redone = avo.data("redo")
    assert redone["undoText"] == text
    assert model(avo) == after


# =============================================================================
# selectAtoms
# =============================================================================
def test_select_atoms_modes(avo):
    avo.load(ETHANE)
    assert avo.data("selectAtoms", {"indices": [0, 1]}) == {"indices": [0, 1]}
    assert selected(avo) == 2
    # "replace" is the default, and replaces
    assert avo.data("selectAtoms", {"indices": [4]}) == {"indices": [4]}
    assert avo.data("selectAtoms", {"indices": [2, 3], "mode": "replace"}) == {"indices": [2, 3]}
    assert avo.data("selectAtoms", {"indices": [3, 5], "mode": "add"}) == {"indices": [2, 3, 5]}
    assert avo.data("selectAtoms", {"indices": [2, 7], "mode": "remove"}) == {"indices": [3, 5]}
    assert selected(avo) == 2
    assert avo.data("selectAtoms", {"indices": []}) == {"indices": []}
    assert selected(avo) == 0
    # the selection changes nothing about the molecule itself
    assert bonds(avo) and avo.info()["atomCount"] == ETHANE_ATOMS


def test_select_atoms_accepts_unsorted_duplicate_and_whole_float_indices(avo):
    avo.load(ETHANE)
    # JSON 1.0 is a whole number; duplicates and order do not matter
    assert avo.data("selectAtoms", {"indices": [7, 1.0, 1, 7, 3]}) == {"indices": [1, 3, 7]}


def test_select_atoms_unwaited_reply(avo):
    avo.load(ETHANE)
    assert avo.call("selectAtoms", {"indices": [0]}) is True
    assert selected(avo) == 1


INVALID_SELECT = [
    ({}, "requires an \"indices\""),
    ({"indices": None}, "requires an \"indices\""),
    ({"indices": "0"}, "requires an \"indices\""),
    ({"indices": 0}, "requires an \"indices\""),
    ({"indices": {"0": 1}}, "requires an \"indices\""),
    ({"indices": [0.5]}, "must be a whole number"),
    ({"indices": ["0"]}, "must be a whole number"),
    ({"indices": [True]}, "must be a whole number"),
    ({"indices": [None]}, "must be a whole number"),
    ({"indices": [[0]]}, "must be a whole number"),
    ({"indices": [-1]}, "negative"),
    ({"indices": [ETHANE_ATOMS]}, "out of range"),
    ({"indices": [0, 99]}, "out of range"),
    ({"indices": [1e30]}, "out of range"),
    ({"indices": [0], "mode": "Add"}, "mode must be"),
    ({"indices": [0], "mode": "toggle"}, "mode must be"),
    ({"indices": [0], "mode": ""}, "mode must be"),
    ({"indices": [0], "mode": 1}, "mode must be"),
    ({"indices": [0], "mode": None}, "mode must be"),
]


@pytest.mark.parametrize("wait", [False, True], ids=["unwaited", "waited"])
@pytest.mark.parametrize("options, fragment", INVALID_SELECT)
def test_select_atoms_invalid_options(avo, options, fragment, wait):
    avo.load(ETHANE)
    avo.data("selectAtoms", {"indices": [1, 2]})  # a selection that must survive
    assert_refused(avo, "selectAtoms", options, fragment, wait=wait)
    assert selected(avo) == 2


def test_select_atoms_one_bad_index_selects_nothing(avo):
    """The list is validated whole before anything is selected."""
    avo.load(ETHANE)
    assert_refused(avo, "selectAtoms", {"indices": [0, 1, 99]}, "out of range")
    assert selected(avo) == 0


def test_select_atoms_on_empty_molecule(avo):
    assert avo.info()["atomCount"] == 0
    assert avo.data("selectAtoms", {"indices": []}) == {"indices": []}
    assert avo.info()["canUndo"] is False
    assert_refused(avo, "selectAtoms", {"indices": [0]}, "out of range")
    assert_refused(avo, "selectAtoms", {"indices": [0], "mode": "add"}, "out of range")


def test_select_atoms_undo_redo(avo):
    avo.load(ETHANE)
    assert avo.info()["canUndo"] is False
    avo.data("selectAtoms", {"indices": [2, 3]})
    assert avo.info()["canUndo"] is True
    avo.data("undo")
    assert selected(avo) == 0
    avo.data("redo")
    assert selected(avo) == 2
    assert avo.data("selectAtoms", {"indices": [], "mode": "add"}) == {"indices": [2, 3]}


def test_select_atoms_that_changes_nothing_pushes_no_undo_entry(avo):
    avo.load(ETHANE)
    avo.data("selectAtoms", {"indices": []})  # nothing selected, nothing requested
    assert avo.info()["canUndo"] is False
    avo.data("selectAtoms", {"indices": [4, 5]})
    avo.data("undo")  # the single entry
    assert avo.info()["canUndo"] is False
    avo.data("redo")
    # asking for the selection it already is, in each mode
    avo.data("selectAtoms", {"indices": [4, 5]})
    avo.data("selectAtoms", {"indices": [4], "mode": "add"})
    avo.data("selectAtoms", {"indices": [0], "mode": "remove"})
    avo.data("undo")
    assert selected(avo) == 0
    assert avo.info()["canUndo"] is False  # one entry in all, not four


def test_select_atoms_skips_locked_atoms_without_an_error(avo):
    """Atoms of a locked layer cannot be selected (as with selectAll); the
    reply is the resulting selection, so the caller sees what was skipped."""
    avo.load(ETHANE)
    avo.data("setLayerLocked", {"layer": 0, "locked": True})
    assert avo.data("selectAtoms", {"indices": [0, 1, 2]}) == {"indices": []}
    assert avo.data("selectAtoms", {"indices": [3], "mode": "add"}) == {"indices": []}
    avo.data("setLayerLocked", {"layer": 0, "locked": False})
    assert avo.data("selectAtoms", {"indices": [0, 1, 2]}) == {"indices": [0, 1, 2]}


# =============================================================================
# addAtom
# =============================================================================
def test_add_atom_by_symbol_and_atomic_number(avo):
    avo.load(ETHANE)
    assert avo.data("addAtom", {"element": "O", "position": [3, 0, 0]}) == {"index": 8}
    assert avo.data("addAtom", {"element": 7.0, "position": [0, 3, 0.5]}) == {"index": 9}
    info = avo.info()
    assert info["atomCount"] == 10
    assert info["bondCount"] == ETHANE_BONDS  # no bondTo: no bond
    assert info["formula"] == "C2H6NO"
    assert coords(avo)[24:27] == pytest.approx([3, 0, 0])
    assert coords(avo)[27:30] == pytest.approx([0, 3, 0.5])
    assert info["modified"] is True


def test_add_atom_bonded(avo):
    avo.load(ETHANE)
    assert avo.data(
        "addAtom", {"element": "O", "position": [2.5, 0, 0], "bondTo": 0, "bondOrder": 2}
    ) == {"index": 8}
    assert ((0, 8), 2) in bonds(avo)
    assert avo.info()["bondCount"] == ETHANE_BONDS + 1
    # bondOrder defaults to 1
    avo.data("addAtom", {"element": "F", "position": [-2.5, 0, 0], "bondTo": 1})
    assert ((1, 9), 1) in bonds(avo)


def test_add_atom_unwaited_reply(avo):
    avo.load(ETHANE)
    assert avo.call("addAtom", {"element": "H", "position": [4, 4, 4]}) is True
    assert avo.info()["atomCount"] == ETHANE_ATOMS + 1


def test_add_atom_to_an_empty_molecule(avo):
    assert avo.info()["atomCount"] == 0
    assert avo.data("addAtom", {"element": "C", "position": [0, 0, 0]}) == {"index": 0}
    info = avo.info()
    assert (info["atomCount"], info["formula"], info["canUndo"]) == (1, "C", True)
    # there is nothing to bond to yet
    assert_refused(avo, "addAtom", {"element": "H", "position": [1, 0, 0], "bondTo": 1}, "bondTo")
    avo.data("undo")
    assert avo.info()["atomCount"] == 0
    assert_refused(avo, "addAtom", {"element": "H", "position": [1, 0, 0], "bondTo": 0}, "bondTo")


def test_add_atom_undo_redo_is_one_step_including_its_bond(avo):
    avo.load(ETHANE)
    before = model(avo)
    avo.data("addAtom", {"element": "O", "position": [2.5, 0, 0], "bondTo": 0, "bondOrder": 3})
    after = model(avo)
    assert after != before
    undo_redo_roundtrip(avo, before, after, "Draw Atom")
    avo.data("undo")
    assert model(avo) == before
    assert avo.info()["canUndo"] is False


BAD_ELEMENT = [
    ({"position": [0, 0, 0]}, "requires an \"element\""),
    ({"element": "Zz", "position": [0, 0, 0]}, "Unknown element"),
    ({"element": "c", "position": [0, 0, 0]}, "Unknown element"),
    ({"element": "", "position": [0, 0, 0]}, "Unknown element"),
    ({"element": None, "position": [0, 0, 0]}, "Unknown element"),
    ({"element": True, "position": [0, 0, 0]}, "Unknown element"),
    ({"element": [8], "position": [0, 0, 0]}, "Unknown element"),
    ({"element": 1.5, "position": [0, 0, 0]}, "Unknown element"),
    ({"element": 0, "position": [0, 0, 0]}, "out of range"),
    ({"element": -1, "position": [0, 0, 0]}, "out of range"),
    ({"element": 119, "position": [0, 0, 0]}, "out of range"),
    ({"element": 1000, "position": [0, 0, 0]}, "out of range"),
]
BAD_POSITION = [
    ({"element": "H"}, "position must be"),
    ({"element": "H", "position": None}, "position must be"),
    ({"element": "H", "position": "0,0,0"}, "position must be"),
    ({"element": "H", "position": []}, "position must be"),
    ({"element": "H", "position": [0, 0]}, "position must be"),
    ({"element": "H", "position": [0, 0, 0, 0]}, "position must be"),
    ({"element": "H", "position": [0, 0, "a"]}, "finite numbers"),
    ({"element": "H", "position": [0, 0, None]}, "finite numbers"),
    ({"element": "H", "position": [0, 0, True]}, "finite numbers"),
    ({"element": "H", "position": [0, 0, [1]]}, "finite numbers"),
]
BAD_BOND = [
    ({"element": "H", "position": [5, 5, 5], "bondTo": value}, "bondTo")
    for value in (-1, 1.5, True, "1", None, [1], 1e30, ETHANE_ATOMS)
] + [
    ({"element": "H", "position": [5, 5, 5], "bondTo": 0, "bondOrder": value}, "bondOrder")
    for value in (0, 4, -1, 1.5, 2.5, True, "1", None, [1])
] + [
    # a bad order is refused even when there is nothing to bond to
    ({"element": "H", "position": [5, 5, 5], "bondOrder": 4}, "bondOrder"),
]


@pytest.mark.parametrize("wait", [False, True], ids=["unwaited", "waited"])
@pytest.mark.parametrize("options, fragment", BAD_ELEMENT + BAD_POSITION + BAD_BOND)
def test_add_atom_invalid_options(avo, options, fragment, wait):
    avo.load(ETHANE)
    assert_refused(avo, "addAtom", options, fragment, wait=wait)


# =============================================================================
# addBond
# =============================================================================
def test_add_bond(avo):
    avo.load(ETHANE)
    assert ((2, 3), 1) not in bonds(avo)
    reply = avo.data("addBond", {"atoms": [2, 3]})
    assert reply == {"index": ETHANE_BONDS}
    assert ((2, 3), 1) in bonds(avo)
    # the order of the pair is irrelevant, and "order" is honoured
    assert avo.data("addBond", {"atoms": [5, 4], "order": 2}) == {"index": ETHANE_BONDS + 1}
    assert ((4, 5), 2) in bonds(avo)
    assert avo.info()["bondCount"] == ETHANE_BONDS + 2
    assert avo.info()["atomCount"] == ETHANE_ATOMS


def test_add_bond_unwaited_reply(avo):
    avo.load(ETHANE)
    assert avo.call("addBond", {"atoms": [2, 3], "order": 3}) is True
    assert ((2, 3), 3) in bonds(avo)


def test_add_bond_undo_redo(avo):
    avo.load(ETHANE)
    before = model(avo)
    avo.data("addBond", {"atoms": [2, 3], "order": 2})
    after = model(avo)
    assert after != before
    undo_redo_roundtrip(avo, before, after, "Draw")
    assert ((2, 3), 2) in bonds(avo)


def test_add_bond_between_bonded_atoms_is_refused_not_a_reorder(avo):
    """RWMolecule::addBond() would silently change the order of an existing
    bond; the verb refuses instead."""
    avo.load(ETHANE)
    assert_refused(avo, "addBond", {"atoms": [0, 1]}, "already bonded")
    assert_refused(avo, "addBond", {"atoms": [1, 0], "order": 3}, "already bonded")
    assert ((0, 1), 1) in bonds(avo)
    avo.data("addBond", {"atoms": [2, 3]})
    assert_refused(avo, "addBond", {"atoms": [2, 3], "order": 2}, "already bonded")
    assert ((2, 3), 1) in bonds(avo)


BAD_ATOMS = [
    ({}, "exactly 2"),
    ({"atoms": None}, "exactly 2"),
    ({"atoms": "01"}, "exactly 2"),
    ({"atoms": 0}, "exactly 2"),
    ({"atoms": []}, "exactly 2"),
    ({"atoms": [2]}, "exactly 2"),
    ({"atoms": [2, 3, 4]}, "exactly 2"),
    ({"atoms": [2, 2]}, "two different atoms"),
    ({"atoms": [-1, 3]}, "within the molecule"),
    ({"atoms": [2, ETHANE_ATOMS]}, "within the molecule"),
    ({"atoms": [2, 99]}, "within the molecule"),
    ({"atoms": [99, 98]}, "within the molecule"),
    ({"atoms": [1e30, 3]}, "within the molecule"),
    ({"atoms": [1.5, 3]}, "within the molecule"),
    ({"atoms": ["2", "3"]}, "within the molecule"),
    ({"atoms": [True, 3]}, "within the molecule"),
    ({"atoms": [None, 3]}, "within the molecule"),
    ({"atoms": [[2], 3]}, "within the molecule"),
]


@pytest.mark.parametrize("wait", [False, True], ids=["unwaited", "waited"])
@pytest.mark.parametrize("options, fragment", BAD_ATOMS)
def test_add_bond_invalid_atoms(avo, options, fragment, wait):
    avo.load(ETHANE)
    assert_refused(avo, "addBond", options, fragment, wait=wait)


@pytest.mark.parametrize("order", [0, 4, -1, 1.5, 2.5, True, "2", None, [2]])
def test_add_bond_invalid_order(avo, order):
    avo.load(ETHANE)
    assert_refused(avo, "addBond", {"atoms": [2, 3], "order": order}, "order must be")


def test_add_bond_on_an_empty_molecule(avo):
    assert avo.info()["atomCount"] == 0
    assert_refused(avo, "addBond", {"atoms": [0, 1]}, "within the molecule")
    avo.data("addAtom", {"element": "C", "position": [0, 0, 0]})
    assert_refused(avo, "addBond", {"atoms": [0, 1]}, "within the molecule")


# =============================================================================
# removeBond
# =============================================================================
def test_remove_bond(avo):
    avo.load(ETHANE)
    assert ((0, 1), 1) in bonds(avo)
    reply = avo.call("removeBond", {"atoms": [1, 0]}, wait=True)
    assert reply["status"] == "finished"
    assert reply.get("data", {}) == {}  # no payload
    assert ((0, 1), 1) not in bonds(avo)
    info = avo.info()
    assert (info["bondCount"], info["atomCount"]) == (ETHANE_BONDS - 1, ETHANE_ATOMS)
    assert info["modified"] is True


def test_remove_bond_unwaited_reply(avo):
    avo.load(ETHANE)
    assert avo.call("removeBond", {"atoms": [0, 2]}) is True
    assert avo.info()["bondCount"] == ETHANE_BONDS - 1


def test_remove_bond_undo_redo_keeps_the_order(avo):
    avo.load(ETHANE)
    avo.data("addBond", {"atoms": [2, 3], "order": 2})
    before = model(avo)
    avo.call("removeBond", {"atoms": [2, 3]}, wait=True)
    after = model(avo)
    assert ((2, 3), 2) not in bonds(avo)
    undo_redo_roundtrip(avo, before, after, "Remove Bond")
    avo.data("undo")
    assert ((2, 3), 2) in bonds(avo)


def test_remove_bond_that_does_not_exist(avo):
    avo.load(ETHANE)
    assert_refused(avo, "removeBond", {"atoms": [2, 3]}, "no bond between")
    assert_refused(avo, "removeBond", {"atoms": [3, 2]}, "no bond between", wait=True)
    avo.call("removeBond", {"atoms": [0, 1]}, wait=True)
    # a second removal of the same bond is the same error, not a no-op success
    assert_refused(avo, "removeBond", {"atoms": [0, 1]}, "no bond between")
    assert avo.info()["bondCount"] == ETHANE_BONDS - 1


@pytest.mark.parametrize("wait", [False, True], ids=["unwaited", "waited"])
@pytest.mark.parametrize("options, fragment", BAD_ATOMS)
def test_remove_bond_invalid_atoms(avo, options, fragment, wait):
    avo.load(ETHANE)
    assert_refused(avo, "removeBond", options, fragment, wait=wait)


def test_remove_bond_on_an_empty_molecule(avo):
    assert avo.info()["atomCount"] == 0
    assert_refused(avo, "removeBond", {"atoms": [0, 1]}, "within the molecule")
    avo.data("addAtom", {"element": "C", "position": [0, 0, 0]})
    assert_refused(avo, "removeBond", {"atoms": [0, 1]}, "within the molecule")


# =============================================================================
# removeSelectedAtoms
# =============================================================================
def test_remove_selected_atoms(avo):
    avo.load(ETHANE)
    avo.data("selectAtoms", {"indices": [2, 3, 6]})
    assert avo.data("removeSelectedAtoms") == {"removed": 3}
    info = avo.info()
    assert (info["atomCount"], info["bondCount"], info["selectedAtomCount"]) == (5, 4, 0)
    assert info["formula"] == "C2H3"
    assert info["modified"] is True
    # the remaining atoms close up: the carbons are still 0 and 1, bonded
    assert ((0, 1), 1) in bonds(avo)
    # nothing is selected any more
    assert avo.data("removeSelectedAtoms") == {"removed": 0}


def test_remove_selected_atoms_takes_the_bonds_with_it(avo):
    avo.load(ETHANE)
    avo.data("selectAtoms", {"indices": [0]})
    assert avo.data("removeSelectedAtoms") == {"removed": 1}
    info = avo.info()
    assert (info["atomCount"], info["bondCount"]) == (ETHANE_ATOMS - 1, ETHANE_BONDS - 4)


def test_remove_selected_atoms_all(avo):
    avo.load(ETHANE)
    avo.data("selectAll")
    assert avo.data("removeSelectedAtoms") == {"removed": ETHANE_ATOMS}
    info = avo.info()
    assert (info["atomCount"], info["bondCount"]) == (0, 0)
    avo.data("undo")
    assert avo.info()["atomCount"] == ETHANE_ATOMS


def test_remove_selected_atoms_unwaited_reply(avo):
    avo.load(ETHANE)
    avo.data("selectAtoms", {"indices": [7]})
    assert avo.call("removeSelectedAtoms") is True
    assert avo.info()["atomCount"] == ETHANE_ATOMS - 1


def test_remove_selected_atoms_with_no_selection_changes_nothing(avo):
    avo.load(ETHANE)
    before = state(avo)
    assert avo.data("removeSelectedAtoms") == {"removed": 0}
    assert avo.call("removeSelectedAtoms") is True
    assert state(avo) == before  # in particular, no undo entry and not modified
    assert before[0]["canUndo"] is False and before[0]["modified"] is False


def test_remove_selected_atoms_on_an_empty_molecule(avo):
    assert avo.info()["atomCount"] == 0
    before = state(avo)
    assert avo.data("removeSelectedAtoms") == {"removed": 0}
    assert state(avo) == before


def test_remove_selected_atoms_undo_redo(avo):
    avo.load(ETHANE)
    avo.data("selectAtoms", {"indices": [1, 4, 5]})
    before = model(avo)
    avo.data("removeSelectedAtoms")
    after = model(avo)
    assert after[0]["atomCount"] == ETHANE_ATOMS - 3
    undo_redo_roundtrip(avo, before, after, "Remove Atom")
    avo.data("undo")
    assert model(avo) == before  # atoms, coordinates and bonds in their old places
    assert selected(avo) == 3  # and the selection that was removed with them


@pytest.mark.parametrize("options", [{"unexpected": 1}, {"indices": [0]}, {"removed": 5}])
def test_remove_selected_atoms_ignores_options(avo, options):
    avo.load(ETHANE)
    avo.data("selectAtoms", {"indices": [2]})
    assert avo.data("removeSelectedAtoms", options) == {"removed": 1}


# =============================================================================
# the verbs together
# =============================================================================
def test_build_up_then_undo_everything_in_order(avo):
    avo.load(ETHANE)
    states = [model(avo)]
    steps = [
        ("addAtom", {"element": "O", "position": [2.5, 0, 0], "bondTo": 0}),
        ("addBond", {"atoms": [2, 3]}),
        ("removeBond", {"atoms": [0, 1]}),
        ("selectAtoms", {"indices": [4, 5, 6]}),
        ("removeSelectedAtoms", {}),
    ]
    for method, options in steps:
        avo.data(method, options)
        states.append(model(avo))
    for expected in reversed(states[:-1]):
        avo.data("undo")
        assert model(avo) == expected
    assert avo.info()["canUndo"] is False
    for expected in states[1:]:
        avo.data("redo")
        assert model(avo) == expected


# =============================================================================
# locked layers
# =============================================================================
def locked_ethane(avo):
    """ETHANE with atoms 2, 3 and 4 selected and then locked on layer 0, and a
    new unlocked layer 1 as the active one. Returns the state to compare."""
    avo.load(ETHANE)
    avo.data("selectAtoms", {"indices": [2, 3, 4]})
    avo.data("setLayerLocked", {"layer": 0, "locked": True})
    avo.data("addLayer")
    avo.data("setActiveLayer", {"layer": 1})
    # locking does not deselect: the selection is still there to delete
    assert selected(avo) == 3
    return state(avo)


def test_locked_atoms_cannot_be_removed(avo):
    before = locked_ethane(avo)
    assert_refused(avo, "removeSelectedAtoms", {}, "locked layer")
    assert_refused(avo, "removeSelectedAtoms", {}, "locked layer", wait=True)
    assert state(avo) == before
    assert avo.info()["atomCount"] == ETHANE_ATOMS
    # unlocking is all it takes
    avo.data("setLayerLocked", {"layer": 0, "locked": False})
    assert avo.data("removeSelectedAtoms") == {"removed": 3}
    assert avo.info()["atomCount"] == ETHANE_ATOMS - 3


def test_atoms_of_a_locked_layer_cannot_be_bonded_or_unbonded(avo):
    before = locked_ethane(avo)
    assert_refused(avo, "addBond", {"atoms": [2, 3]}, "locked layer")
    assert_refused(avo, "addBond", {"atoms": [3, 2], "order": 2}, "locked layer", wait=True)
    assert_refused(avo, "removeBond", {"atoms": [0, 1]}, "locked layer")
    assert_refused(avo, "removeBond", {"atoms": [1, 0]}, "locked layer", wait=True)
    assert state(avo) == before
    assert ((0, 1), 1) in bonds(avo) and ((2, 3), 1) not in bonds(avo)
    avo.data("setLayerLocked", {"layer": 0, "locked": False})
    avo.data("addBond", {"atoms": [2, 3]})
    avo.call("removeBond", {"atoms": [0, 1]}, wait=True)
    assert ((2, 3), 1) in bonds(avo) and ((0, 1), 1) not in bonds(avo)


def test_a_new_atom_cannot_bond_to_a_locked_atom_but_can_stand_alone(avo):
    before = locked_ethane(avo)
    assert_refused(avo, "addAtom", {"element": "O", "position": [3, 0, 0], "bondTo": 0}, "locked layer")
    assert_refused(
        avo, "addAtom", {"element": "O", "position": [3, 0, 0], "bondTo": 4}, "locked layer", wait=True
    )
    assert state(avo) == before
    # on the unlocked active layer, with nothing locked to touch, it is fine
    assert avo.data("addAtom", {"element": "O", "position": [3, 0, 0]}) == {"index": ETHANE_ATOMS}
    assert avo.info()["atomCount"] == ETHANE_ATOMS + 1


def test_a_locked_active_layer_refuses_every_edit(avo):
    avo.load(ETHANE)
    avo.data("selectAtoms", {"indices": [2]})
    avo.data("setLayerLocked", {"layer": 0, "locked": True})
    before = state(avo)
    for method, options in (
        ("addAtom", {"element": "O", "position": [3, 0, 0]}),
        ("addAtom", {"element": "O", "position": [3, 0, 0], "bondTo": 0}),
        ("addBond", {"atoms": [3, 4]}),
        ("removeBond", {"atoms": [0, 1]}),
        ("removeSelectedAtoms", {}),
    ):
        assert_refused(avo, method, options, "active layer is locked")
    assert_refused(avo, "addAtom", {"element": "O", "position": [3, 0, 0]}, "locked", wait=True)
    assert state(avo) == before
    avo.data("setLayerLocked", {"layer": 0, "locked": False})
    assert avo.data("addAtom", {"element": "O", "position": [3, 0, 0]}) == {"index": ETHANE_ATOMS}


def test_a_locked_active_layer_with_no_selection_removes_nothing_without_error(avo):
    """removeSelectedAtoms only consults the lock when there is something to
    delete (editor.cpp: removed > 0)."""
    avo.load(ETHANE)
    avo.data("setLayerLocked", {"layer": 0, "locked": True})
    assert avo.data("removeSelectedAtoms") == {"removed": 0}


def test_an_unlocked_atom_bonded_to_a_locked_one_cannot_be_removed(avo):
    """Deleting an atom deletes its bonds, so a locked neighbour protects it.
    The new atom lives on layer 1 and is bonded to atom 0 on layer 0."""
    avo.load(ETHANE)
    avo.data("addLayer")
    avo.data("setActiveLayer", {"layer": 1})
    assert avo.data("addAtom", {"element": "O", "position": [2.5, 0, 0], "bondTo": 0}) == {
        "index": ETHANE_ATOMS
    }
    avo.data("selectAtoms", {"indices": [ETHANE_ATOMS]})
    avo.data("setLayerLocked", {"layer": 0, "locked": True})
    before = state(avo)
    assert selected(avo) == 1
    assert_refused(avo, "removeSelectedAtoms", {}, "bonded atom is in a locked layer")
    assert state(avo) == before
    avo.data("setLayerLocked", {"layer": 0, "locked": False})
    assert avo.data("removeSelectedAtoms") == {"removed": 1}


def test_locking_the_layer_of_a_new_atom_protects_it(avo):
    """Layer membership of an atom added over RPC: it goes on the active layer."""
    avo.load(ETHANE)
    avo.data("addLayer")
    avo.data("setActiveLayer", {"layer": 1})
    avo.data("addAtom", {"element": "O", "position": [2.5, 0, 0]})  # atom 8, layer 1
    avo.data("setActiveLayer", {"layer": 0})
    avo.data("setLayerLocked", {"layer": 1, "locked": True})
    before = state(avo)
    assert_refused(avo, "addBond", {"atoms": [0, 8]}, "Atom 8 is in a locked layer")
    assert_refused(avo, "addAtom", {"element": "H", "position": [3, 3, 3], "bondTo": 8}, "Atom 8")
    # atoms of the unlocked layer 0 are still editable
    assert avo.data("addBond", {"atoms": [2, 3]}) == {"index": ETHANE_BONDS}
    avo.call("removeBond", {"atoms": [2, 3]}, wait=True)
    assert before[2] == bonds(avo)


def test_refused_edits_leave_undo_and_redo_alone(avo):
    avo.load(ETHANE)
    avo.data("addAtom", {"element": "O", "position": [3, 0, 0]})  # one step to undo
    avo.data("undo")  # and one to redo
    avo.data("setLayerLocked", {"layer": 0, "locked": True})
    before = state(avo)
    assert before[0]["canRedo"] is True
    assert_refused(avo, "addAtom", {"element": "O", "position": [3, 0, 0]}, "locked")
    assert state(avo) == before  # the redo entry survived the refused edit
