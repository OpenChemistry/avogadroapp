"""
Process and JSON-RPC session helpers for the end-to-end Avogadro tests.

AvogadroApp launches the real application; Session talks to it and, after
every single request, checks that the application is still alive and
responsive. A death or a hang is turned into a test failure plus a
reproducer file holding every request sent so far.
"""

import importlib
import itertools
import json
import os
import re
import signal
import socket
import subprocess
import sys
import tempfile
import time
import types
from pathlib import Path

import pytest

TESTS_DIR = Path(__file__).resolve().parent
APP_ROOT = TESTS_DIR.parents[1]  # the avogadroapp checkout

PING_TIMEOUT = 5.0
CALL_TIMEOUT = 60.0
STARTUP_TIMEOUT = 60.0


# --------------------------------------------------------------------------
# The client library. It lives in avogadrolibs; we import it, never copy it.
# --------------------------------------------------------------------------
def _load_client():
    try:
        return importlib.import_module("avogadro.connect")
    except ImportError:
        pass

    # No usable installed package (a source checkout has no compiled
    # extensions, and avogadro/__init__.py imports them eagerly). connect.py
    # only needs the standard library, so load it through a stand-in package.
    python_dir = Path(
        os.environ.get("AVOGADRO_PYTHON_DIR")
        or APP_ROOT.parent / "avogadrolibs" / "python"
    )
    package_dir = python_dir / "avogadro"
    if not (package_dir / "connect.py").is_file():
        raise RuntimeError(
            "Cannot import avogadro.connect: install the avogadro package or "
            "set AVOGADRO_PYTHON_DIR to avogadrolibs/python (looked in %s)"
            % package_dir
        )
    for name in [n for n in sys.modules if n == "avogadro" or n.startswith("avogadro.")]:
        del sys.modules[name]
    stub = types.ModuleType("avogadro")
    stub.__path__ = [str(package_dir)]
    sys.modules["avogadro"] = stub
    return importlib.import_module("avogadro.connect")


client_module = _load_client()
RPCError = client_module.RPCError
connect = client_module.connect


# --------------------------------------------------------------------------
# Test corpus locations
# --------------------------------------------------------------------------
def _env_dir(variable, default):
    return Path(os.environ.get(variable) or default)


def corpus_roots():
    """Map of root name -> directory to sweep; roots that do not exist are
    omitted."""
    data_root = _env_dir("AVOGADRO_DATA_ROOT", APP_ROOT.parent / "avogadrodata")
    roots = {
        "avogadrodata": data_root / "data",
        "molecules": _env_dir("AVOGADRO_MOLECULES_DIR", APP_ROOT.parent / "molecules"),
        "crystals": _env_dir("AVOGADRO_CRYSTALS_DIR", APP_ROOT.parent / "crystals"),
    }
    return {name: path for name, path in roots.items() if path.is_dir()}


# CIFs are excluded until the "Select Space Group" prompt on open is fixed:
# SpaceGroup::fillHeuristic() runs a modal dialog when a crystal has no space
# group, and a modal dialog blocks the RPC reply (and the sweep with it).
SKIP_EXTENSIONS = {".png", ".svg", ".md", ".sh", ".py", ".csv", ".txt"}


def corpus_files():
    """List of (root name, root directory, file) for every candidate file."""
    found = []
    for name, root in corpus_roots().items():
        for path in sorted(root.rglob("*")):
            relative = path.relative_to(root)
            if not path.is_file() or any(p.startswith(".") for p in relative.parts):
                continue
            stem = path.name.upper()
            if path.suffix.lower() in SKIP_EXTENSIONS:
                continue
            if stem == "LICENSE" or stem.startswith("README"):
                continue
            found.append((name, root, path))
    return found


# --------------------------------------------------------------------------
# Launching the application
# --------------------------------------------------------------------------
class ConfigError(Exception):
    """The configured executable cannot be used."""


class AppStartError(Exception):
    """The application did not come up."""


def resolve_executable(path):
    """Accept the binary itself or a macOS .app bundle."""
    path = Path(path).expanduser()
    if path.is_dir() and path.suffix == ".app":
        macos = path / "Contents" / "MacOS"
        preferred = macos / "Avogadro2"
        if preferred.is_file():
            return preferred
        candidates = sorted(p for p in macos.glob("*") if p.is_file())
        if candidates:
            return candidates[0]
        raise ConfigError("No executable found inside %s" % path)
    if not path.is_file():
        raise ConfigError("Avogadro executable not found: %s" % path)
    if not os.access(path, os.X_OK):
        raise ConfigError("Avogadro executable is not executable: %s" % path)
    return path


def signal_name(returncode):
    if returncode is not None and returncode < 0:
        try:
            return signal.Signals(-returncode).name
        except ValueError:
            return "signal %d" % -returncode
    return None


def tail(path, lines=80):
    try:
        with open(path, "r", errors="replace") as handle:
            return handle.read().splitlines()[-lines:]
    except OSError:
        return []


class AvogadroApp:
    """One launched Avogadro process.

    The wait loop never trusts a connection alone: if the process exits
    while we are waiting, we fail immediately with the log tail.
    """

    _counter = itertools.count(1)

    def __init__(self, executable, log_dir):
        self.executable = resolve_executable(executable)
        self.log_dir = Path(log_dir)
        self.process = None
        self.name = None
        self.log_path = None
        self.version = None
        self.args = []
        self.extra_args = []  # appended to the launch line (see start())
        self.tests = []  # node ids run since the last launch
        # Set when a call never returned although the process lives (a modal
        # dialog, a hang): the app cannot be trusted for the next test.
        self.tainted = False
        # Called with this app after every (re)launch, once RPC answers.
        self.on_ready = None

    def start(self, timeout=STARTUP_TIMEOUT, extra_args=None):
        """Launch the application. extra_args (for example a file to open)
        are appended to the standard launch line and kept for restart()."""
        if extra_args is not None:
            self.extra_args = list(extra_args)
        self.name = "avotest-%d-%d" % (os.getpid(), next(self._counter))
        socket_path = Path(tempfile.gettempdir()) / self.name
        if socket_path.exists():
            socket_path.unlink()
        self.log_path = self.log_dir / (self.name + ".log")
        # --testing must never be passed (the parser rejects it).
        self.args = [
            "--rpc-name",
            self.name,
            "--skip-autosave",
            "--disable-settings",
        ] + self.extra_args
        self.tests = []
        self.tainted = False
        with open(self.log_path, "wb") as log:
            self.process = subprocess.Popen(
                [str(self.executable)] + self.args,
                stdout=log,
                stderr=subprocess.STDOUT,
                stdin=subprocess.DEVNULL,
            )

        deadline = time.monotonic() + timeout
        while True:
            if self.process.poll() is not None:
                raise AppStartError(
                    "Avogadro exited during startup (return code %s).\n%s"
                    % (self.process.returncode, "\n".join(tail(self.log_path, 40)))
                )
            try:
                with connect(self.name, timeout=5) as client:
                    self.version = client.version()
                    client.molecule_info()  # fails until the window exists
                break
            except (ConnectionError, OSError, RPCError):
                if time.monotonic() > deadline:
                    self.stop()
                    raise AppStartError(
                        "Avogadro did not accept RPC connections within %d s.\n%s"
                        % (timeout, "\n".join(tail(self.log_path, 40)))
                    )
                time.sleep(0.25)
        if self.on_ready is not None:
            self.on_ready(self)

    def alive(self):
        return self.process is not None and self.process.poll() is None

    def healthy(self):
        return self.alive() and not self.tainted and ping(self.name)

    def stop(self):
        process = self.process
        if process is not None and process.poll() is None:
            process.terminate()
            try:
                process.wait(5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        if self.name:
            try:
                (Path(tempfile.gettempdir()) / self.name).unlink()
            except OSError:
                pass

    def restart(self):
        self.stop()
        self.start()


# The smallest CIF that Open Babel reads: one atom in a P 1 cell.
_WARMUP_CIF = """data_warmup
_cell_length_a 5
_cell_length_b 5
_cell_length_c 5
_cell_angle_alpha 90
_cell_angle_beta 90
_cell_angle_gamma 90
_symmetry_space_group_name_H-M 'P 1'
loop_
_atom_site_label
_atom_site_fract_x
_atom_site_fract_y
_atom_site_fract_z
C1 0 0 0
"""


def wait_for_cif_reader(app, timeout=30.0):
    """Wait until Avogadro can read CIF (an Open Babel format).

    Open Babel's formats are registered in the background after the window
    answers RPC, so for a second or two every openFile of a .cif fails with
    "No file format available". Loading a CIF replaces the active molecule,
    so this is only for apps whose state does not matter (the corpus sweep).
    """
    deadline = time.monotonic() + timeout
    while True:
        try:
            with connect(app.name, timeout=10) as client:
                client.load_molecule(_WARMUP_CIF, "cif")
            return
        except (ConnectionError, OSError, RPCError):
            if time.monotonic() > deadline:
                raise AppStartError(
                    "Avogadro could not read CIF within %d s of starting.\n%s"
                    % (timeout, "\n".join(tail(app.log_path, 40)))
                )
            time.sleep(0.25)


def ping(name, timeout=PING_TIMEOUT):
    """True if a fresh connection gets an answer to internalPing in time."""
    try:
        with connect(name, timeout=timeout) as client:
            return client.ping()
    except (ConnectionError, OSError):
        return False


def probe(name, timeout=PING_TIMEOUT):
    """The per-step liveness check: (answers ping, open modal dialog or None).

    Both questions share one fresh connection, so the dialog check costs one
    extra call per step. The dialog is activeDialog's {title, className}. A
    modal dialog's nested event loop keeps both answerable, which is why a
    request that opened a dialog can be told apart from a hang.
    """
    try:
        with connect(name, timeout=timeout) as client:
            if not client.ping():
                return False, None
            try:
                reply = client.send("activeDialog")["result"]
            except RPCError:
                return True, None  # an app without activeDialog
            if reply.get("open"):
                return True, {
                    "title": reply.get("title", ""),
                    "className": reply.get("className", ""),
                }
            return True, None
    except (ConnectionError, OSError):
        return False, None


# --------------------------------------------------------------------------
# Talking to it
# --------------------------------------------------------------------------
def summarize(value, limit=160):
    """Shorten long strings (cjson, base64 PNGs) so a step list stays readable."""
    if isinstance(value, str):
        if len(value) > limit:
            return value[:limit] + "...(%d chars)" % len(value)
        return value
    if isinstance(value, dict):
        return {key: summarize(item, limit) for key, item in value.items()}
    if isinstance(value, list):
        if len(value) > 20:
            return [summarize(i, limit) for i in value[:20]] + ["...(%d items)" % len(value)]
        return [summarize(item, limit) for item in value]
    return value


def ignorable_dialog(dialog):
    """A modal that is not a stuck prompt. The progress dialog shown while a
    file reads in the background (the command line, File > Open) is modal for
    as long as the read lasts and goes away by itself."""
    return dialog.get("className") == "QProgressDialog"


class Session:
    """A client connection that records every step and polices liveness."""

    def __init__(self, app, nodeid, reproducer_dir, call_timeout=CALL_TIMEOUT):
        self.app = app
        self.nodeid = nodeid
        self.reproducer_dir = Path(reproducer_dir)
        self.call_timeout = call_timeout
        self.steps = []
        self.client = connect(app.name, timeout=call_timeout)
        app.tests = (app.tests + [nodeid])[-50:]

    def close(self):
        self.client.close()

    # -- requests --------------------------------------------------------
    def call(self, method, params=None, wait=False, timeout=None):
        """Send one request; return its result or raise RPCError."""
        result, error = self._run(method, params, wait, timeout)
        if error is not None:
            raise error
        return result

    def expect_error(self, method, params=None, code=None, wait=False, timeout=None):
        """Send a request that must be answered with an error; return it."""
        result, error = self._run(method, params, wait, timeout)
        if error is None:
            raise AssertionError(
                "%s %s: expected an error, got %s"
                % (method, params or {}, summarize(result))
            )
        if code is not None and error.code != code:
            raise AssertionError(
                "%s: expected error code %s, got %s (%s)"
                % (method, code, error.code, error.message)
            )
        return error

    # -- conveniences ----------------------------------------------------
    def info(self):
        return self.call("moleculeInfo")

    def load(self, content, format="xyz"):
        return self.call("loadMolecule", {"content": content, "format": format})

    def molecules(self):
        """The open molecules, as listMolecules reports them."""
        return self.call("listMolecules")

    def wait_for(self, predicate, what, method="listMolecules", timeout=30.0, interval=0.25):
        """Poll a read-back until predicate(result) is true; fail after
        timeout seconds. For things that happen on the app's own time, such
        as a file named on the command line."""
        deadline = time.monotonic() + timeout
        while True:
            result = self.call(method)
            if predicate(result):
                return result
            if time.monotonic() > deadline:
                raise AssertionError(
                    "%s did not happen within %d s; last %s: %s"
                    % (what, timeout, method, summarize(result))
                )
            time.sleep(interval)

    def data(self, method, params=None, timeout=None):
        """Run a waited plugin command and return its data dict."""
        return self.call(method, params, wait=True, timeout=timeout).get("data", {})

    # -- internals -------------------------------------------------------
    def _run(self, method, params, wait, timeout):
        step = {"method": method, "params": params or {}, "wait": wait}
        if timeout is not None:
            step["timeout"] = timeout
        result = error = transport = None
        started = time.monotonic()
        try:
            if timeout is not None and not wait:
                self.client.sock.settimeout(timeout)
            result = self.client.send(method, params, wait=wait, timeout=timeout)["result"]
            step["outcome"] = {"result": summarize(result)}
        except RPCError as exc:
            error = exc
            step["outcome"] = {"error": {"code": exc.code, "message": exc.message}}
        except (OSError, ValueError) as exc:  # timeout, closed socket, bad JSON
            transport = exc
            step["outcome"] = {"transport_error": repr(exc)}
        finally:
            if timeout is not None and not wait and self.client.sock is not None:
                self.client.sock.settimeout(self.call_timeout)
        step["elapsed"] = round(time.monotonic() - started, 3)
        self.steps.append(step)
        self._police(transport)
        return result, error

    def _police(self, transport):
        """The liveness oracle, run after every step."""
        app = self.app
        kind = None
        dialog = None
        if isinstance(transport, ConnectionError):
            # The process may be a moment away from being reaped.
            try:
                app.process.wait(5)
            except subprocess.TimeoutExpired:
                pass
        if app.process.poll() is not None:
            kind = "exit"
        else:
            answers, dialog = probe(app.name)
            if not answers:
                kind = "hang"
            elif dialog is not None and not ignorable_dialog(dialog):
                # Nothing in a test may leave a modal dialog open, whether or
                # not the request that opened it ever got a reply.
                kind = "dialog"
            elif isinstance(transport, socket.timeout):
                # Alive and answering pings, but this request never got a
                # reply: a stuck command.
                kind = "blocked"
            elif transport is not None:
                kind = "transport"
        if kind is None:
            return

        app.tainted = True
        last = self.steps[-1]
        path = self._write_reproducer(kind, dialog if kind == "dialog" else None)
        detail = {
            "exit": "Avogadro died (%s)"
            % (
                signal_name(app.process.returncode)
                or "return code %s" % app.process.returncode
            ),
            "hang": "Avogadro stopped answering internalPing",
            "blocked": "the request got no reply although Avogadro still answers "
            "pings (a stuck command?)",
            "transport": "the connection failed: %r" % (transport,),
            "dialog": "a modal dialog is open: %r (%s)"
            % tuple((dialog or {}).get(k, "") for k in ("title", "className")),
        }[kind]
        pytest.fail(
            "%s after step %d: %s %s\nreproducer: %s\nlog: %s"
            % (
                detail,
                len(self.steps),
                last["method"],
                json.dumps(summarize(last["params"])),
                path,
                app.log_path,
            ),
            pytrace=False,
        )

    def _write_reproducer(self, kind, dialog=None):
        app = self.app
        self.reproducer_dir.mkdir(parents=True, exist_ok=True)
        safe = re.sub(r"[^A-Za-z0-9_.-]+", "_", self.nodeid)[-120:]
        path = self.reproducer_dir / ("%s-%s.json" % (safe, time.strftime("%Y%m%dT%H%M%S")))
        returncode = app.process.poll()
        document = {
            "test": self.nodeid,
            "failure": kind,
            "returncode": returncode,
            "signal": signal_name(returncode),
            "app_version": app.version,
            "executable": str(app.executable),
            "launch_args": app.args,
            "tests_since_launch": app.tests,
            "steps": self.steps,
            "last_step": self.steps[-1],
            "log_tail": tail(app.log_path, 80),
        }
        if dialog is not None:
            document["dialog"] = dialog
        with open(path, "w") as handle:
            json.dump(document, handle, indent=2)
        return path


# --------------------------------------------------------------------------
# Small helpers shared by the tests
# --------------------------------------------------------------------------
PNG_MAGIC = b"\x89PNG\r\n\x1a\n"


def png_size(data):
    """(width, height) from a PNG's IHDR chunk; asserts the magic number."""
    assert data[:8] == PNG_MAGIC, "not a PNG"
    return int.from_bytes(data[16:20], "big"), int.from_bytes(data[20:24], "big")
