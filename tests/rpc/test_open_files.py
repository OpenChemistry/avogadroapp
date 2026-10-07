"""Open every sample file and render its HOMO; only a death or a hang fails.

An error reply from openFile is fine (not every file is a supported
format). The sweep shares one application and relaunches it whenever a test
killed, hung or blocked it, so one bad file does not hide the rest.
"""

import pytest

import harness

pytestmark = pytest.mark.corpus

# NOTE: .cif files are excluded from the corpus (see harness.SKIP_EXTENSIONS):
# opening one without a space group raises a modal "Select Space Group"
# dialog that blocks the RPC socket. Re-enable once that prompt is fixed.
FILES = harness.corpus_files()


def _param(entry):
    name, root, path = entry
    return pytest.param(path, id="%s/%s" % (name, path.relative_to(root).as_posix()))


@pytest.fixture
def open_timeout(pytestconfig):
    return pytestconfig.getoption("--open-timeout")


@pytest.mark.parametrize("path", [_param(entry) for entry in FILES] or [pytest.param(None, marks=pytest.mark.skip(reason="no corpus directories found"))])
def test_open_file(shared, path, open_timeout, request):
    def record_property(name, value):
        request.node.user_properties.append((name, value))

    record_property("ext", path.suffix.lower() or "(none)")
    try:
        shared.call("openFile", {"fileName": str(path)}, timeout=open_timeout)
    except harness.RPCError as error:
        record_property("openFile", "error")
        record_property("message", error.message.strip()[:200])
        return
    record_property("openFile", "ok")

    info = shared.info()
    if info["hasBasisSet"] and info["orbitalCount"] > 0:
        try:
            shared.call("renderMO", {"orbital": "homo"}, wait=True, timeout=120)
            record_property("renderMO", "ok")
        except harness.RPCError as error:
            record_property("renderMO", "error")
            record_property("message", error.message.strip()[:200])
