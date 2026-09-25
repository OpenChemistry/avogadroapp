# Avogadro web editor

A desktop-browser molecular editor with HTML controls around one Qt-backed
`GLWidget`. It uses Avogadro's Navigator, Editor, SelectionTool and BallStick
plugins, existing molecular readers/writers, and `RWMolecule` undo transactions.
It does not create the desktop main window, menus, tool panels, or file dialogs.

The initial document is empty, with Draw active, carbon selected, automatic bond
order and hydrogen adjustment enabled. Draw by clicking or dragging; right-click
an atom or bond to remove it. Select by clicking or dragging a rectangle. Navigate
by dragging to rotate, right-dragging to pan, and scrolling to zoom. Ctrl/Cmd+Z,
Ctrl/Cmd+Shift+Z, Ctrl+Y, Delete and Backspace apply only in the viewport.

CJSON is the default download format. MOL, SDF and XYZ are also supported. Open
parses into a temporary document; errors preserve the molecule and undo history.
Open and New clear history on success. Multi-record SDF and XYZ imports use the
first record with a visible notice. Replacing unsaved work requires an HTML
confirmation; leaving the page also requests browser confirmation. Only a CJSON
download clears the unsaved marker because other formats may omit properties.
A download request cannot prove that the user retained the file.

Tablet gestures, notebooks, SMILES conversion, Python plugins, server-side
computation, and publishing are outside this milestone.

## Build

Use a **fresh build directory and install prefix** for each profile. Reusing an
installation with more features can leave obsolete plugins and headers behind.
Run these commands from the OpenChemistry superbuild root, with its submodules
already available. Native examples assume CMake, Ninja and the selected Qt kit
are on the appropriate paths.

```sh
# Normal native defaults remain enabled.
cmake -S . -B build-native-new -G Ninja \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.11.1/gcc_64
cmake --build build-native-new --parallel 4

# Native desktop without external processes or Python script plugins.
cmake -S . -B build-native-no-process-new -G Ninja \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.11.1/gcc_64 \
  -DENABLE_SUBPROCESS=OFF
cmake --build build-native-no-process-new --parallel 4

# Qt 6.11.1 multithreaded WASM and Emscripten 4.0.7.
# Use the SDK's normal environment activation before configuring.
/path/to/Qt/6.11.1/wasm_multithread/bin/qt-cmake \
  -S . -B build-web-new -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DQT_HOST_PATH=/path/to/Qt/6.11.1/gcc_64
cmake --build build-web-new --parallel 4
# Stage the application into a separate, empty destination.
cmake --install build-web-new/avogadroapp --prefix "$PWD/stage-web-new"
cd stage-web-new/share/avogadro-web
python3 serve-wasm.py
```

Open `http://127.0.0.1:8000/`. Stop the server with Ctrl+C. The server binds only to
loopback and serves its working directory. It sends the required isolation
headers and serves the optional precompressed WASM file when gzip is accepted.

`ENABLE_SUBPROCESS` defaults ON natively and OFF under Emscripten; it is separate
from `USE_PYTHON`, which builds Python API bindings. Its value is exported as
`AvogadroLibs_ENABLE_SUBPROCESS` and through the native target definition
`AVOGADRO_ENABLE_SUBPROCESS`. Disabled script/package APIs are not installed.

`AVOGADRO_WEB_EDITOR` defaults OFF natively and ON under Emscripten. This profile
requires subprocess, libarchive, plotting and desktop RPC to be OFF and static
plugins to be ON. WASM requires static libraries and no Python bindings.
Incompatible explicit requests fail configuration rather than being silently
forced off. Native code tests use `ENABLE_TESTING=ON`; browser tests execute the
WASM release. Existing CMake policy settings are retained.

For a native version of the same web controller and its tests, build
avogadrolibs and avogadroapp with `AVOGADRO_WEB_EDITOR=ON`, and the libraries with
`ENABLE_SUBPROCESS=OFF`. The native executable is useful for debugging; the HTML
interface is delivered only by the WASM package. Set `ENABLE_TESTING=ON` on both
projects and provide GoogleTest. Run `ctest --test-dir <app-build>` and the
Core/Io/QtGui/Rendering suites in the libraries build.

## Browser runtime and deployment

Keep `index.html`, `editor.css`, `editor.js`, `qtloader.js`, `avogadro-web.js`,
`avogadro-web.wasm`, and any generated `.data` or worker JavaScript files together.
CMake packages the loader installed with the selected Qt kit. Gzip is generated
when the host `gzip` utility is available. Do not mix loaders or runtimes from
another Qt build. The generated Qt demo HTML is not part of the release package.

The page calls the supported [`qtLoad` and container resize API](https://doc.qt.io/qt-6/wasm.html#using-qtloader).
It requires WebAssembly, WebGL2, SharedArrayBuffer, a secure context (HTTPS or
localhost), and cross-origin isolation. A static host must send:

```text
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
Cross-Origin-Resource-Policy: same-origin
```

Serve `.wasm` as `application/wasm`; add `Content-Encoding: gzip` only when
actually serving gzip bytes. A plain `python -m http.server` omits the isolation
headers and the editor will display an error. HTTPS hosting and publication are
not configured by this change.

## Interface

`WebController` owns the document and layer state and coordinates plugins,
transaction history, file operations and state notifications. All bindings run
on the Qt application thread, return values, and expose no C++ pointers.
`AvogadroEditor` in `editor.js` wraps the Embind instance:

- `setTool('navigate' | 'draw' | 'select')`
- `setDrawOptions({ atomicNumber: 1..118, bondOrder: 0..3, adjustHydrogens: bool })`
- `newMolecule()`, `loadMolecule(text, format)`, `exportMolecule(format = 'cjson')`
- `deleteSelection()`, `undo()`, `redo()`, `resetCamera()`
- `state` and `statechange` events containing tool, drawOptions, atomCount,
  bondCount, modified, canUndo and canRedo

The Editor plugin owns its Draw options and publishes them through its command
interface and change signal. Its native QWidget panel is created on demand and
synchronizes in both directions. Browser code never creates or reads that panel.

## Automated browser checks

Tests use fresh profiles and never attach to a user's browser. Chromium's test
uses Node's built-in APIs. Firefox and startup-failure tests use `puppeteer-core`
(optionally located by `PUPPETEER_MODULE`). Firefox can run inside `xvfb-run` for
software rendering on Linux. See [VALIDATION.md](VALIDATION.md) for the exact
commands and results from this implementation.

```sh
node avogadroapp/web/tests/browser-smoke.mjs http://127.0.0.1:8000 evidence/chromium
xvfb-run -a node avogadroapp/web/tests/firefox-smoke.mjs \
  http://127.0.0.1:8000 evidence/firefox
node avogadroapp/web/tests/startup-failures.mjs stage-web-new/share/avogadro-web
python3 avogadroapp/web/tests/check-build-profile.py build-web-new
```
