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
    config.addinivalue_line("markers", "corpus: sweep over every sample data file")
    requested = config.getoption("--avogadro") or os.environ.get("AVOGADRO_EXECUTABLE")
    config.avogadro_exe = None
    if requested:
        # A configured but unusable executable is an error, never a skip.
        try:
            config.avogadro_exe = harness.resolve_executable(requested)
        except harness.ConfigError as exc:
            raise pytest.UsageError(str(exc))


def pytest_collection_modifyitems(config, items):
    if config.avogadro_exe is not None:
        return
    skip = pytest.mark.skip(
        reason="no Avogadro executable configured (use --avogadro PATH or "
        "set AVOGADRO_EXECUTABLE)"
    )
    for item in items:
        if HERE in Path(str(item.path)).parents:
            item.add_marker(skip)


@pytest.fixture(scope="session")
def avogadro_exe(pytestconfig):
    return pytestconfig.avogadro_exe


@pytest.fixture(scope="session")
def reproducer_dir(pytestconfig):
    return Path(pytestconfig.getoption("--reproducer-dir"))


@pytest.fixture(scope="session")
def app_log_dir(tmp_path_factory):
    return tmp_path_factory.mktemp("avogadro-logs")


@pytest.fixture
def avo(request, avogadro_exe, app_log_dir, reproducer_dir):
    """A Session on a freshly launched Avogadro, stopped after the test."""
    app = harness.AvogadroApp(avogadro_exe, app_log_dir)
    app.start()
    session = harness.Session(app, request.node.nodeid, reproducer_dir)
    try:
        yield session
    finally:
        session.close()
        app.stop()


@pytest.fixture
def launch(request, avogadro_exe, app_log_dir, reproducer_dir):
    """Factory for tests that need command line arguments (a file to open at
    startup): launch("path/to/file") returns a Session on a fresh Avogadro
    started with those extra arguments. Every app it launched is stopped
    after the test."""
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


@pytest.fixture(scope="module")
def shared_app(avogadro_exe, app_log_dir):
    app = harness.AvogadroApp(avogadro_exe, app_log_dir)
    app.start()
    yield app
    app.stop()


@pytest.fixture
def shared(request, shared_app, reproducer_dir):
    """A Session on the module's shared app, relaunched first if the last
    test killed it, hung it or left it blocked."""
    if not shared_app.healthy():
        shared_app.restart()
    session = harness.Session(shared_app, request.node.nodeid, reproducer_dir)
    try:
        yield session
    finally:
        session.close()


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
