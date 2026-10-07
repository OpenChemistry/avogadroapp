import collections
import os
from pathlib import Path

import pytest

import harness

HERE = Path(__file__).resolve().parent


def pytest_addoption(parser):
    group = parser.getgroup("avogadro")
    group.addoption(
        "--avogadro",
        default=None,
        help="Avogadro executable or .app bundle (default: $AVOGADRO_EXECUTABLE)",
    )
    group.addoption(
        "--reproducer-dir",
        default=str(HERE / "reproducers"),
        help="where crash/hang reproducer JSON files are written",
    )
    group.addoption(
        "--open-timeout",
        type=float,
        default=30.0,
        help="seconds the corpus sweep waits for one openFile before calling "
        "it blocked (default: 30)",
    )


def pytest_configure(config):
    requested = config.getoption("--avogadro") or os.environ.get("AVOGADRO_EXECUTABLE")
    config.avogadro_exe = None
    if requested:
        # A configured but unusable executable is an error, never a skip.
        try:
            config.avogadro_exe = harness.resolve_executable(requested)
        except harness.ConfigError as exc:
            raise pytest.UsageError(str(exc))


@pytest.fixture(scope="session")
def avogadro_exe(pytestconfig):
    if pytestconfig.avogadro_exe is None:
        pytest.skip(
            "no Avogadro executable configured (use --avogadro PATH or set "
            "AVOGADRO_EXECUTABLE)"
        )
    return pytestconfig.avogadro_exe


@pytest.fixture(scope="session")
def reproducer_dir(pytestconfig):
    return Path(pytestconfig.getoption("--reproducer-dir"))


@pytest.fixture(scope="session")
def app_log_dir(tmp_path_factory):
    return tmp_path_factory.mktemp("avogadro-logs")


@pytest.fixture
def launch(request, avogadro_exe, app_log_dir, reproducer_dir):
    """Factory for a Session on a freshly launched Avogadro, which is stopped
    after the test: launch() or, for command line arguments (a file to open at
    startup), launch("path/to/file")."""
    launched = []

    def _launch(*extra_args):
        app = harness.AvogadroApp(avogadro_exe, app_log_dir)
        app.start(extra_args=extra_args)
        session = harness.Session(app, request.node.nodeid, reproducer_dir)
        launched.append((session, app))
        return session

    yield _launch
    for session, app in launched:
        session.close()
        app.stop()


@pytest.fixture
def fresh(launch):
    """A Session on an Avogadro launched for this test alone, exactly as it
    starts. For a test that changes app-wide state (camera, projection, display
    types) or depends on the startup state beyond `avo`'s blank molecule."""
    return launch()


@pytest.fixture(scope="module")
def app_ready():
    """What to run on the module's shared app after every (re)launch; override
    in a test module that needs the app warmed up."""
    return None


@pytest.fixture(scope="module")
def shared_app(avogadro_exe, app_log_dir, app_ready):
    app = harness.AvogadroApp(avogadro_exe, app_log_dir)
    app.on_ready = app_ready
    app.start()
    yield app
    app.stop()


def _session(request, app, reproducer_dir):
    """A Session on `app`, relaunched first if the last test killed it, hung it
    or left it blocked."""
    if not app.healthy():
        app.restart()
    return harness.Session(app, request.node.nodeid, reproducer_dir)


@pytest.fixture
def shared(request, shared_app, reproducer_dir):
    """A Session on the module's shared app, exactly as the last test left it."""
    session = _session(request, shared_app, reproducer_dir)
    try:
        yield session
    finally:
        session.close()


@pytest.fixture
def avo(request, shared_app, reproducer_dir):
    """A Session on the module's shared app, reset to the startup document: one
    blank, unmodified molecule. Per-molecule state (undo, layers, selection)
    goes with the molecules that reset closes; app-wide state (camera,
    projection, display types, windows) does not, so a test that changes any
    of it uses `fresh`."""
    session = _session(request, shared_app, reproducer_dir)
    try:
        if not session.reset():
            # Whatever the last test did, close can't undo it: start over.
            session.close()
            shared_app.restart()
            session = harness.Session(shared_app, request.node.nodeid, reproducer_dir)
            if not session.reset():
                pytest.fail("a freshly launched Avogadro is not in the startup state")
        yield session
    finally:
        session.close()


@pytest.fixture(scope="session")
def butane(molecules_dir):
    """Path of molecules/alkanes/butane.cjson (14 atoms, C4H10)."""
    path = molecules_dir / "alkanes" / "butane.cjson"
    if not path.is_file():
        pytest.skip("%s missing" % path)
    return path


def _need_dir(path, what):
    if not path.is_dir():
        pytest.skip("%s not found: %s" % (what, path))
    return path


@pytest.fixture(scope="session")
def data_dir():
    roots = harness.corpus_roots()
    return _need_dir(roots.get("avogadrodata", Path("avogadrodata/data")), "avogadrodata")


@pytest.fixture(scope="session")
def molecules_dir():
    return _need_dir(
        harness.corpus_roots().get("molecules", Path("molecules")), "molecules"
    )


# -- corpus summary --------------------------------------------------------
_corpus_reports = []


def pytest_runtest_logreport(report):
    if report.when == "call" or (report.when == "setup" and report.failed):
        properties = dict(report.user_properties)
        if "ext" in properties or report.failed:
            _corpus_reports.append((report, properties))


def pytest_terminal_summary(terminalreporter):
    rows = [(r, p) for r, p in _corpus_reports if "test_open_files.py" in r.nodeid]
    if not rows:
        return
    write = terminalreporter.write_line
    terminalreporter.section("corpus summary")
    roots = collections.Counter(r.nodeid.split("[", 1)[1].split("/", 1)[0] for r, _ in rows)
    write("files per root: %s" % dict(roots))

    errors = collections.Counter(p.get("ext", "?") for r, p in rows if p.get("openFile") == "error")
    write("openFile errors by extension: %s" % dict(errors.most_common()))
    mo = collections.Counter(p.get("renderMO") for r, p in rows if "renderMO" in p)
    write("renderMO results: %s" % dict(mo))
    failed = [r.nodeid for r, _ in rows if r.failed]
    write("failed (died, hung or blocked): %d" % len(failed))
    for nodeid in failed:
        write("  " + nodeid)
    write("slowest 10:")
    for report, _ in sorted(rows, key=lambda row: -row[0].duration)[:10]:
        write("  %7.2fs  %s" % (report.duration, report.nodeid.split("[", 1)[1][:-1]))
