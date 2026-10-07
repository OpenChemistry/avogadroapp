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
  loop: pings still answer, but the request that opened it never replies.
  Do not call `fillUnitCell`, `fillTranslationalCell`, `show*` or
  `fetchPDB`/`fetchByName`. Known app bugs that do open dialogs through RPC
  are listed below. A failed `exportFile` or `saveGraphic` is safe: under
  `--skip-dialogs` it is logged and answered with an error reply.
* Socket names are `avotest-<pid>-<n>` (macOS limits `sun_path` to 104 bytes).

## The oracle

Every request goes through `Session`, which records it as a step
`{method, params, wait, timeout, outcome, elapsed}` and then checks that

1. the process is still running, and
2. a fresh connection gets `internalPing` answered within 5 s.

A request that gets no reply within its timeout (60 s by default) while the
app still answers pings is "blocked" (a modal dialog or stuck command). On
any failure the test fails with a message naming the reproducer file.
Error replies are not failures; tests assert them explicitly with
`expect_error`.

## Reproducers

Written to `--reproducer-dir` (default `tests/rpc/reproducers/`, git-ignored)
as `<test id>-<timestamp>.json`:

```
test, failure ("exit" | "hang" | "blocked" | "transport"), returncode,
signal, app_version, executable, launch_args, tests_since_launch,
steps (every request of the test), last_step, log_tail (last 80 lines)
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
without `wait` the reply is `true`. Layer verbs fail with code -2.
Operands of `checks` are saved names or plain numbers.

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
* Periodic, non-CIF files that probably hit the same space-group dialog
  (the request blocks): `avogadrodata/cjson/rutile.cjson`,
  `nwchem/band.out`, `turbomole/periodic/mgo.coord`; `vasp/corundum-conventional.POSCAR`
  took 21 s, most likely waiting on the same dialog until it was closed.
