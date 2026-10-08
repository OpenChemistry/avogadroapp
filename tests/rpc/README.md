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

The app opens a real window while the tests run.

Environment: `AVOGADRO_DATA_ROOT` (default `../avogadrodata`),
`AVOGADRO_MOLECULES_DIR`, `AVOGADRO_CRYSTALS_DIR` (defaults `../molecules`,
`../crystals`), `AVOGADRO_PYTHON_DIR` (where to find `avogadro/connect.py`
when `avogadro` is not installed). Options: `--reproducer-dir`,
`--open-timeout` (corpus `openFile` timeout, default 30 s).

## Ground rules

* Never pass `--testing` to the app: argument parsing rejects it and the app
  exits, so the `kill` RPC is unusable. The harness stops the app by
  terminating the process (SIGTERM, then SIGKILL after 5 s).
* The launch line is `--rpc-name <name> --skip-autosave --disable-settings`
  (`harness.LAUNCH_ARGS`). `--rpc-name` implies `--skip-dialogs`, so no startup
  dialog blocks.
* Nothing in a test may open a modal dialog. Modal dialogs run a nested event
  loop: pings (and `activeDialog`) still answer, but the request that opened
  it never replies. The oracle below fails the test as soon as one is open.
  Do not call `fillUnitCell`, `fillTranslationalCell`, `show*` or
  `fetchPDB`/`fetchByName`. A failed `exportFile` or `saveGraphic` is safe:
  under `--skip-dialogs` it is logged and answered with an error reply.
* Socket names are `avotest-<pid>-<n>` (macOS limits `sun_path` to 104 bytes).

## Fixtures

| Fixture | The app |
| --- | --- |
| `avo` | One app per test module, shared by its tests. Before each test every open molecule is closed (discarding changes), which leaves the startup document: one blank, unmodified molecule. The reset verifies that, and relaunches the app if it cannot reach it. Per-molecule state (undo, layers, selection) goes with the molecules; app-wide state does not |
| `fresh` | A new app for this test alone. Use it when a test changes app-wide state (camera, projection, display types, the active tool), asserts the exact startup state, or taints the app |
| `launch(*args)` | Factory for apps that need command line arguments, such as a file to open |
| `shared` | The module's app exactly as the last test left it, with no reset (the corpus). Override the module fixture `app_ready` to run something after every (re)launch |
| `butane`, `data_dir`, `molecules_dir` | Paths into the sample data; the test is skipped if they are missing |

A shared app is relaunched when a test killed it, hung it or left it blocked.
`record_property` values (the corpus records `ext`, `openFile`, `renderMO` and
`message`) are visible with `--junitxml`.

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
command). On any failure the test fails with a message naming the reproducer
file. Error replies are not failures; tests assert them explicitly with
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

`tests_since_launch` matters for a shared app: the culprit may be an earlier
test or file. Replaying `steps` against a fresh app reproduces a crash.

## Scenarios

`scenarios/*.json`, one test per file, run in order. Multi-step molecule
sequences live here; one verb and its errors live in `test_builtins.py`.

```
{
  "description": "text",
  "issue": "optional reference",
  "fresh": true,                       // optional: run on a `fresh` app, for
                                       // scenarios that change the camera,
                                       // projection or display types
  "xfail": "optional: a real app bug; the test is xfail(strict=True)",
  "steps": [
    {
      "method": "addLayer",
      "params": {"layer": 1},          // optional; "$MOLECULES/..." expands
                                       // to the molecules directory
      "wait": true,                    // optional, default false
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
    {"op": "lt", "a": "e1", "b": 0.0}, // "lt" | "differs"
    {"op": "differs", "a": "d1", "b": 1.9, "tolerance": 0.1}
  ]
}
```

Operands of `checks` are saved names or plain numbers. `result` matches a list
of the same length element by element, each element as a subset, so
`listMolecules` can be checked as
`[{"atomCount": 14}, {"atomCount": 3, "active": true}]`.

## Rules of the verbs

`listCommands` lists every command with its description; these are the parts
that are not obvious from it.

* A waited command replies `{"status": "finished", "data": {...}}`; without
  `wait` the reply is `true`. Failures of the layer and molecule verbs have
  code -2.
* Read-backs (`listMolecules`, `listTools`, `listDisplayTypes`, `getCamera`,
  `activeDialog`, ...) reply with their payload directly as the `result`: no
  `wait`, no `status`/`data` envelope.
* `closeMolecule` fails on a molecule with unsaved changes unless `discard` is
  exactly `true` (`"yes"` and `1` do not count).
* `activateTool` takes the tool's object name, the `name` that `listTools`
  reports (`Navigator`, `Editor`, `MeasureTool`), not the translated display
  name.
* Indices (`layer`, `index`) must be JSON numbers with a whole value; strings
  such as `"0"` and booleans are refused.
* After an RPC `openFile`, `moleculeInfo.fileName` is the opened file, as
  after File > Open; `loadMolecule` has no file.

## Corpus

`test_open_files.py` (marker `corpus`) opens every file under
`avogadrodata/data`, `molecules` and `crystals` (except the extensions in
`harness.SKIP_EXTENSIONS`, hidden files and READMEs) and, when the file has
orbitals, renders the HOMO with `renderMO` (`wait`, 120 s). An error reply is
fine; only a death, hang or blocked request fails. After every (re)launch the
shared app waits until it can read CIF (`harness.wait_for_cif_reader`), because
Open Babel's formats register in the background a second or two after the
window answers RPC.

## Known failures

* `renderMO` for the HOMO of `avogadrodata/data/fchk/CO-cc-6Z.fchk` crashes
  the app (SIGTRAP) on builds without the avogadrolibs commit "Skip
  unsupported h/i shells": a libc++ hardening assertion, `vector[]` out of
  bounds in `GaussianSetTools::calculateShellCutoff()` for its h and i shells.
* An `openFile` sent right after launch can fail with "No file format
  available" for formats that Open Babel provides (CIF, ...): they are
  registered in the background after RPC already answers. The corpus works
  around it as described above; a script has to retry.
