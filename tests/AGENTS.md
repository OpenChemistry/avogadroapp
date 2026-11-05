# tests

The application has no unit tests of its own; logic that can be unit tested
belongs in avogadrolibs, which has GoogleTest suites for every library. The
tests here run the built `avogadro2` as a whole:

- **RPC end-to-end tests** (`tests/rpc/`, pytest): launch the app and drive
  it over its JSON-RPC socket. ctest names `avogadro-rpc` and
  `avogadro-rpc-corpus`, label `rpc`. This is where new tests go.
- **QtTesting playback** (`avogadro-<name>`): replays recorded GUI sessions
  from `avogadrodata/tests/avogadro/xml/*.xml` with `--test-file`. Only built
  with `ENABLE_TESTING=ON`, which also needs the QtTesting library. There is
  one recording (`basic-drawing.xml`); recordings break whenever the UI
  changes, so prefer an RPC test.

[rpc/README.md](rpc/README.md) is the full reference for the RPC suite:
fixtures, the oracle, reproducers, the scenario format and the rules of each
verb. Read it before writing a test. See the top-level
[AGENTS.md](../AGENTS.md) for build commands.

## Running

```sh
ctest --test-dir <build>/avogadroapp -L rpc -E corpus -V --no-tests=error
python -m pytest tests/rpc --avogadro /path/to/Avogadro2 -m "not corpus"
```

ctest only registers tests when `ENABLE_TESTING=ON` and CMake finds
avogadrodata (a sibling checkout, or the `AVOGADRO_DATA_ROOT` environment
variable or cache entry). The RPC tests also need the Python that CMake found
to `import pytest`; otherwise configure prints a warning and they are
missing. `--no-tests=error` makes ctest fail instead of passing with nothing
run.
The suite opens real windows. On Linux CI it runs under Xvfb with Mesa
software OpenGL, in the UBSan build only, and never the corpus.

**Running either kind of test clears your Avogadro settings.** Both launch the
app with `--disable-settings`, which empties the real settings store
(recent files, window layout, crash-report consent, plugin settings) rather
than using a separate one. Back up your settings, or run the tests as a
different user, if that matters.

## Adding an RPC test

- One built-in verb and its error cases: a function in
  [rpc/test_builtins.py](rpc/test_builtins.py) (or `test_edit_verbs.py`,
  `test_molecules.py` for those areas).
- A sequence of steps on a molecule, such as a bug reproduction: a JSON file
  in [rpc/scenarios/](rpc/scenarios/), one test per file. Mark a known app bug
  with `"xfail"` and the issue, rather than leaving the scenario out.
- Use the `avo` fixture (a shared app, reset between tests) unless the test
  changes app-wide state such as the camera, projection, display types or
  active tool; then use `fresh`.
- Assert error replies explicitly with `expect_error`. An error reply is not
  a failure by itself; a crash, hang, blocked request or open modal dialog is.
- Never open a modal dialog from a test, and do not pass `--testing`.
- The harness uses only the Python standard library and pytest, and imports
  the RPC client from avogadrolibs (`avogadro.connect`) rather than copying
  it. Keep it that way.

## Test data

Molecules come from the sibling checkouts of
[avogadrodata](https://github.com/OpenChemistry/avogadrodata),
[molecules](https://github.com/OpenChemistry/molecules) and
[crystals](https://github.com/OpenChemistry/crystals) (override with
`AVOGADRO_DATA_ROOT`, `AVOGADRO_MOLECULES_DIR`, `AVOGADRO_CRYSTALS_DIR`). Small
inline molecules, as in `test_builtins.py`, are better than new data files.
A test that needs a new file needs a matching avogadrodata change, merged
first.

Reproducers written to `rpc/reproducers/` contain every request of the failed
test, which may include molecule data and local paths. They are git-ignored;
do not commit them or attach them to a public issue without checking what
they contain.
