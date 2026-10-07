# End-to-end RPC tests

These tests launch the real Avogadro application and drive it over its
JSON-RPC socket with `avogadro.connect` (from `avogadrolibs/python`; it is
imported, never copied). They find crashes, hangs and wrong replies that unit
tests cannot reach, for example the layer crashes that appear only after the
render plugins have registered per-layer state on a real molecule.

Python standard library and pytest only.

## Running

```
python -m pytest tests/rpc --avogadro /path/to/Avogadro2          # everything
python -m pytest tests/rpc --avogadro /path/to/Avogadro2 -m "not corpus"
python -m pytest tests/rpc --avogadro /path/to/Avogadro2 -m corpus
```

`--avogadro` takes the binary or a macOS `.app` bundle (falls back to
`$AVOGADRO_EXECUTABLE`). With nothing configured the suite is skipped with a
reason; a configured path that does not exist is an error. With CMake the
tests are `avogadro-rpc` and `avogadro-rpc-corpus` (label `rpc`).

The app opens a real window while the tests run. Each test gets a fresh
process; the corpus sweep shares one and relaunches it after a death.

Environment: `AVOGADRO_DATA_ROOT` (default `../avogadrodata`),
`AVOGADRO_MOLECULES_DIR`, `AVOGADRO_CRYSTALS_DIR` (defaults `../molecules`,
`../crystals`), `AVOGADRO_PYTHON_DIR` (where to find `avogadro/connect.py`
when `avogadro` is not installed). Options: `--reproducer-dir`,
`--open-timeout` (corpus `openFile` timeout, default 30 s).

## Ground rules

* Never pass `--testing` to the app: argument parsing rejects it and the app
  exits, so the `kill` RPC is unusable. The harness stops the app by
  terminating the process (SIGTERM, then SIGKILL after 5 s).
* Launch line: `--rpc-name <name> --skip-autosave --disable-settings`.
  `--rpc-name` implies `--skip-dialogs`, so no startup dialog blocks.
* Nothing in a test may open a modal dialog. Modal dialogs run a nested event
  loop: pings (and `activeDialog`) still answer, but the request that opened
  it never replies. The oracle below fails the test as soon as one is open.
  Do not call `fillUnitCell`, `fillTranslationalCell`, `show*` or
  `fetchPDB`/`fetchByName`. Known app bugs that do open dialogs through RPC
  are listed below. A failed `exportFile` or `saveGraphic` is safe: under
  `--skip-dialogs` it is logged and answered with an error reply.
* Socket names are `avotest-<pid>-<n>` (macOS limits `sun_path` to 104 bytes).

## The oracle

Every request goes through `Session`, which records it as a step
`{method, params, wait, timeout, outcome, elapsed}` and then checks that

1. the process is still running,
2. a fresh connection gets `internalPing` answered within 5 s, and

3. no modal dialog is open. The same fresh connection that pings asks
   `activeDialog` (one extra call per step). An open modal is a failure of kind
   "dialog" whatever the step was, and takes precedence over "blocked"; the
   failure message and the reproducer carry the dialog's title and class. The
   one exception is a `QProgressDialog`, which a background file read shows
   while it lasts (the command line, File > Open) and which goes away by
   itself.

A request that gets no reply within its timeout (60 s by default) while the
app still answers pings, and no dialog is open, is "blocked" (a stuck
command). On
any failure the test fails with a message naming the reproducer file.
Error replies are not failures; tests assert them explicitly with
`expect_error`.

## Reproducers

Written to `--reproducer-dir` (default `tests/rpc/reproducers/`, git-ignored)
as `<test id>-<timestamp>.json`:

```
test, failure ("exit" | "hang" | "blocked" | "dialog" | "transport"),
returncode, signal, app_version, executable, launch_args,
tests_since_launch, steps (every request of the test), last_step,
log_tail (last 80 lines), dialog ({title, className}, kind "dialog" only)
```

`tests_since_launch` matters for the shared corpus app: the culprit may be an
earlier file. Replaying `steps` against a fresh app reproduces a crash.

## Scenarios

`scenarios/*.json`, one test per file, run in order against a fresh app:

```
{
  "description": "text",
  "issue": "optional reference",
  "xfail": "optional: a real app bug; the test is xfail(strict=True)",
  "steps": [
    {
      "method": "addLayer",
      "params": {"layer": 1},          // optional; strings starting with
                                       // $MOLECULES, $AVOGADRODATA, $CRYSTALS
                                       // expand to that corpus directory
      "wait": true,                    // optional, default false
      "timeout": 60,                   // optional
      "expect": "ok",                  // "ok" (default) | "error"
      "error_code": -2,                // with "error": required code
      "message": "out of range",       // with "error": substring of message
      "result": {"data": {"count": 2}},// subset of the reply's result; nested
                                       // dicts match recursively, anything
                                       // else must be equal
      "save": {"e1": "data.energy"}    // remember a value from the result
    }
  ],
  "checks": [                          // after all steps, on saved values
    {"op": "lt", "a": "e1", "b": 0.0}, // "lt" | "gt" | "differs"
    {"op": "differs", "a": "d1", "b": 1.9, "tolerance": 0.1}
  ]
}
```

A waited plugin command replies `{"status": "finished", "data": {...}}`;
without `wait` the reply is `true`. Layer and molecule verbs fail with code -2.
Operands of `checks` are saved names or plain numbers.

`result` matches a list of the same length element by element, each element
as a subset, so `listMolecules` can be checked as
`[{"atomCount": 14}, {"atomCount": 3, "active": true}]`.

## Application verbs the tests use

Besides the layer verbs, `MainWindow::handleCommand` answers these (listed by
`listCommands` as `builtin`). They reuse the menu and molecule-list code and
never open a dialog; their failure messages are plain strings, not `tr()`.
Verbs marked "read-back" return their payload as the reply's `result`
(no `wait`/`status` envelope).

| Verb | Parameters | Reply data / result |
| --- | --- | --- |
| `listMolecules` (read-back) | | `[{index, active, atomCount, formula, fileName, modified}]` in molecule-list order; `modified` is the dirty state |
| `newMolecule` | | `{index, count}`, like File > New |
| `setActiveMolecule` | `index` (JSON number) | `{index, count}`; a bad or missing index fails |
| `closeMolecule` | `index` (default: active), `discard` | `{index, count}` of the molecule that is active afterwards. A modified molecule fails ("The molecule has unsaved changes; pass discard: true to close it anyway.") unless `discard` is exactly `true`. Closing the last molecule leaves a new empty one |
| `undo`, `redo` | | `{canUndo, canRedo, undoText, redoText, modified}`; fails when there is nothing to undo or redo |
| `listTools` (read-back) | | `[{name, displayName, active}]` for the active view |
| `activateTool` | `name` | `{tool}`. `name` is the tool's object name, the `name` that `listTools` reports (`Navigator`, `Editor`, `MeasureTool`); the translated display name (`ToolPlugin::name()`, "Navigate tool") is not accepted |
| `activeDialog` (read-back) | | `{open, title, className}` of the active modal widget; answered even before the window exists |

`moleculeInfo` also reports `modified`, `canUndo` and `canRedo`. After an RPC
`openFile` its `fileName` is the opened file, as after File > Open.

Selecting an atom range pushes an undo step ("Change Selection") but does not
mark the document modified; redoing it does (see "avogadroapp #476" below).

Tests that need command line arguments use the `launch(*args)` fixture instead
of `avo`.

## Corpus

`test_open_files.py` (marker `corpus`) opens every file under
`avogadrodata/data`, `molecules` and `crystals` (skipping hidden files and
png/svg/md/sh/py/csv/txt/README/LICENSE, and all `.cif` files while the space-group prompt below is unfixed) and, when the file has orbitals,
renders the HOMO with `renderMO` (`wait`, 120 s). An error reply is fine;
only a death, hang or blocked request fails. Each test records `ext`,
`openFile`, `renderMO` and `message` properties (visible with `--junitxml`).

## Known app bugs seen while writing these

(A failed export used to open a modal "Error saving file" dialog from
`MainWindow::backgroundWriterFinished()` and left the writer busy; under
`--skip-dialogs` it now logs the error and fails the waited request with the
writer's message. `test_export_file_failure_is_an_error_not_a_dialog` guards it.)

* Opening a CIF without a space group (`openFile`, `loadMolecule`) runs
  `SpaceGroup::fillHeuristic()` -> `selectSpaceGroup()`, a modal "Select
  Space Group" dialog. The RPC reply never comes until a person closes it.
* `renderMO` for the HOMO of `avogadrodata/data/fchk/CO-cc-6Z.fchk` crashes
  the app (SIGTRAP): a libc++ hardening assertion, `vector[]` out of bounds in
  `GaussianSetTools::calculateShellCutoff()` via `buildShellData()`.
* Periodic, non-CIF files that open the same space-group dialog, now
  reported by the oracle as failure kind "dialog" ("Select Space Group",
  `QDialog`): `avogadrodata/cjson/rutile.cjson` and `cjson/si.cjson` in the
  last corpus run. It is not deterministic: `rutile.cjson` opened without a
  dialog in about five of six single launches. `nwchem/band.out`,
  `turbomole/periodic/mgo.coord` and `vasp/corundum-conventional.POSCAR`,
  which earlier runs blocked on or were slow with, opened normally in the last
  run. These fail until the avogadrolibs fix lands; they are not excluded.
* In the build these runs used, no `.cif` file can be opened at all: every one
  is answered with "No file format available to read" (the corpus still skips
  them, as above, for builds that do have a CIF reader).

## Findings from the molecule-verb scenarios

* avogadroapp #634 (blank molecule left behind when a file is opened): not
  reproducible with a command line file (the window then never creates the
  blank one; `test_launch_with_file_leaves_exactly_one_molecule` pins that).
  It reproduced with an RPC `openFile`/`loadMolecule` on a fresh start and with
  the macOS open event (`test_finder_open_event_replaces_the_blank_molecule`);
  both are fixed by `MainWindow::setOpenedMolecule()`.
* Selecting the already-active molecule used to disable "modified" tracking
  for it (`setMolecule()` ran again and `GLWidget::setMolecule()` dropped the
  `changed` connection); fixed, see `test_selecting_the_active_molecule_again_keeps_it_tracked`.
* avogadroapp #476 (spurious unsaved-changes prompts): no view-only operation
  marks the document modified any more (camera, projection, render types,
  layer visibility, `renderImage`, `listDisplayTypes`; see
  `modified_flag_view_operations_476`). Selection does not set it either, but
  it does push a "Change Selection" undo step, and *redoing* that step sets
  `modified` (`redoEdit()` always emits `changed` with the atoms flag).
  Whether a selection should be an undo step at all, and what it should do to
  `modified`, is open; `test_selection_effect_on_modified_is_reported` records
  the current behaviour without asserting it.
