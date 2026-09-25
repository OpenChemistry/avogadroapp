# Implementation validation — 25 September 2026

Validation ran locally from `/home/timvdm/openchemistry`. Existing archives,
`avogadro-wasm/`, `build-wasm/`, unrelated submodule changes and commit history
were preserved. Nothing was published.

## Results

| Check | Result |
| --- | --- |
| Native default superbuild, Qt 6.11.1 | Built libraries and desktop executable |
| Native `ENABLE_SUBPROCESS=OFF` | Built libraries and desktop executable |
| WASM web profile, Qt 6.11.1 multithreaded / Emscripten 4.0.7 | Built and installed |
| Native default Core/Io/QtGui/Rendering | 47/47 CTest entries passed |
| Native subprocess-disabled Core/Io/QtGui/Rendering | 46/46 passed |
| Native web-profile Core/Io/QtGui/Rendering | 46/46 passed |
| Web controller regression executable | All seven GoogleTest cases passed |
| Address, undefined-behavior and leak sanitizers | All seven controller/Draw cases passed with instrumented Avogadro libraries |
| Chromium | Startup, Draw/hydrogens, undo/redo, discard/cancel, navigation guard, settings, selection/deletion, HTML focus, browser file input, malformed import, four downloads, camera and resize passed |
| Firefox 154 | Startup, Draw/hydrogens, bond dragging, selection/deletion, undo/redo, HTML focus, rotate/pan/zoom/fit/resize, multi-record notice, malformed import and four export payloads passed |
| Startup failure states | Missing isolation headers, unavailable WebGL2 and missing WASM asset displayed errors with controls disabled |
| Native viewport | Started with a valid OpenGL context on private Xvfb display |
| Native desktop | Started with temporary settings; remained alive after activating Draw |
| Generated dependencies and installed exports | Only Navigator, Editor, Selection and BallStick plugins; disabled subprocess sources/headers/exports, libarchive, plotting and RPC absent |
| Incompatible WASM configuration requests | Eight explicit incompatible requests rejected with option-specific errors |
| Static checks | cppcheck and clang-tidy completed; details below |

The controller tests cover lazy panel creation and disposal, two-way option
synchronization, defaults, invalid settings/tool lookup, unchanged hydrogen
adjustment, single/double/triple bonds, grouped undo/redo, file round trips,
first-record notices, failed-import preservation, save state and repeated
release of document layer state.

## Build and test commands

The following abbreviations identify the exact local tool locations used:

```sh
CMAKE=/home/timvdm/Qt/Tools/CMake/bin/cmake
CTEST=/home/timvdm/Qt/Tools/CMake/bin/ctest
NINJA=/home/timvdm/Qt/Tools/Ninja/ninja
QT_NATIVE=/home/timvdm/Qt/6.11.1/gcc_64
```

Native default configuration and build:

```sh
$CMAKE -S . -B build-native -G Ninja -DCMAKE_MAKE_PROGRAM="$NINJA" \
  -DQt6_DIR="$QT_NATIVE/lib/cmake/Qt6" -DCMAKE_BUILD_TYPE=Release
CMAKE_BUILD_PARALLEL_LEVEL=4 $CMAKE --build build-native -j2
$CMAKE -S avogadrolibs -B build-native/avogadrolibs -DENABLE_TESTING=ON
$CMAKE --build build-native/avogadrolibs -j4
QT_QPA_PLATFORM=offscreen XDG_CONFIG_HOME="$PWD/build-native/test-config" \
  LD_LIBRARY_PATH="$QT_NATIVE/lib:$PWD/build-native/prefix/lib" \
  $CTEST --test-dir build-native/avogadrolibs --output-on-failure \
  -R '^(Core|Io|QtGui|Rendering)-'
```

Native subprocess-disabled libraries and desktop, using the newly built native
dependencies:

```sh
$CMAKE -S avogadrolibs -B build-native-nosubprocess/libs -G Ninja \
  -DCMAKE_MAKE_PROGRAM="$NINJA" \
  -DCMAKE_PREFIX_PATH="$PWD/build-native/prefix;$QT_NATIVE" \
  -DCMAKE_INSTALL_PREFIX="$PWD/build-native-nosubprocess/prefix" \
  -DCMAKE_BUILD_TYPE=Release -DENABLE_SUBPROCESS=OFF \
  -DENABLE_TESTING=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
$CMAKE --build build-native-nosubprocess/libs -j4
$CMAKE --install build-native-nosubprocess/libs
$CMAKE -S avogadroapp -B build-native-nosubprocess/app -G Ninja \
  -DCMAKE_MAKE_PROGRAM="$NINJA" -DQT_VERSION=6 -DBUILD_SHARED_LIBS=ON \
  -DCMAKE_PREFIX_PATH="$PWD/build-native-nosubprocess/prefix;$PWD/build-native/prefix;$QT_NATIVE" \
  -DCMAKE_BUILD_TYPE=Release
$CMAKE --build build-native-nosubprocess/app -j4
QT_QPA_PLATFORM=offscreen $CTEST --test-dir build-native-nosubprocess/libs \
  --output-on-failure -R '^(Core|Io|QtGui|Rendering)-'
```

The native web profile uses the same direct-project commands, replacing
`build-native-nosubprocess` with `build-native-web`, using `RelWithDebInfo`, and
adding `-DAVOGADRO_WEB_EDITOR=ON` to both projects and `-DENABLE_TESTING=ON` to the
app. Its additional test command is:

```sh
QT_QPA_PLATFORM=offscreen $CTEST --test-dir build-native-web/app --output-on-failure
```

WASM configuration/build used a workspace-local copy of the installed SDK and
cache because the installed cache was read-only. The SDK's missing `acorn`
package was supplied in that copy; no installed SDK files were modified.
Previously downloaded third-party archives were reused in the fresh builds.

```sh
export EMSDK="$PWD/build-web/sdk"
export EM_CACHE="$PWD/build-web/em-cache"
/home/timvdm/Qt/6.11.1/wasm_multithread/bin/qt-cmake -S . -B build-web \
  -G Ninja -DCMAKE_MAKE_PROGRAM="$NINJA" -DCMAKE_BUILD_TYPE=Release \
  -DQT_HOST_PATH="$QT_NATIVE" \
  -DQT_CHAINLOAD_TOOLCHAIN_FILE="$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake"
CMAKE_BUILD_PARALLEL_LEVEL=4 $CMAKE --build build-web -j2
$CMAKE --install build-web/avogadroapp --prefix "$PWD/build-web/stage"
python3 avogadroapp/web/tests/check-build-profile.py build-web
```

Fresh configurations under `build-web/configuration-checks/` were run with the
same Qt/WASM configure arguments and one override at a time:
`ENABLE_SUBPROCESS=ON`, `USE_LIBARCHIVE=ON`, `USE_PYTHON=ON`, `USE_PLOTTER=ON`,
`Avogadro_ENABLE_RPC=ON`, `BUILD_STATIC_PLUGINS=OFF`, `BUILD_SHARED_LIBS=ON`, and
`AVOGADRO_WEB_EDITOR=OFF`. Every command returned nonzero and named the offending
option. Logs are kept in that directory.

Browser checks, with a loopback `serve-wasm.py` process serving the application:

```sh
node avogadroapp/web/tests/browser-smoke.mjs http://127.0.0.1:8000 build-web/browser-evidence
export PUPPETEER_MODULE=/tmp/avogadro-node/node_modules/puppeteer-core/lib/puppeteer/puppeteer-core.js
PATH=/tmp/avogadro-qa-tools/root/usr/bin:$PATH LIBGL_ALWAYS_SOFTWARE=1 \
  xvfb-run -a -s '-screen 0 1280x900x24 -nolisten tcp' \
  node avogadroapp/web/tests/firefox-smoke.mjs http://127.0.0.1:8000 build-web/firefox-evidence
node avogadroapp/web/tests/startup-failures.mjs build-web/stage/share/avogadro-web
```

Screenshots and JSON results are in `build-web/browser-evidence/` and
`build-web/firefox-evidence/`. Tests own fresh browser profiles. Xvfb and
clang-tidy were extracted from Ubuntu packages into `/tmp`; no system packages
were installed. Puppeteer Core 25.12.0 was installed in `/tmp` with package
scripts disabled. Native GUI checks explicitly selected `QT_QPA_PLATFORM=xcb`,
`QT_XCB_GL_INTEGRATION=xcb_glx` and `LIBGL_ALWAYS_SOFTWARE=1` on private Xvfb
displays. The viewport used `build-native-web/app/bin/avogadro-web --smoke-test`;
the desktop check activated Ctrl+2 with `xdotool` and checked process liveness.
Native Draw panel synchronization is additionally checked by the regression
executable.

## Sanitizers and static analysis

The `build-native-sanitize/libs` and `app` directories were configured like the
native web profile, using Debug and
`-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer` (and the
same C flags for libraries). All Avogadro libraries were instrumented; external
Qt and dependency binaries were not rebuilt with sanitizers.

```sh
ASAN_OPTIONS=detect_leaks=0 $CMAKE --build build-native-sanitize/libs -j4
$CMAKE --install build-native-sanitize/libs
ASAN_OPTIONS=detect_leaks=0 $CMAKE --build build-native-sanitize/app -j4
QT_QPA_PLATFORM=offscreen ASAN_OPTIONS=detect_leaks=1 \
  UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  $CTEST --test-dir build-native-sanitize/app --output-on-failure
```

Leak detection was disabled only while executing build-time host tools inside
the ptrace-restricted sandbox. The regression test ran outside that restriction
with leak detection enabled and reported no sanitizer errors or leaks.

```sh
cppcheck --enable=warning,performance,portability --std=c++17 \
  --suppress=missingIncludeSystem --inline-suppr -Dslots= -Dsignals=public \
  -DQ_OBJECT= -DQ_EMIT= -Demit= avogadroapp/web/main.cpp \
  avogadroapp/web/webcontroller.cpp avogadrolibs/avogadro/qtplugins/editor/editor.cpp \
  avogadrolibs/avogadro/qtplugins/editor/editortoolwidget.cpp \
  avogadrolibs/avogadro/rendering/shaderprogram.cpp
/tmp/avogadro-qa-tools/root/usr/bin/clang-tidy-18 -p build-native-web/app \
  avogadroapp/web/webcontroller.cpp avogadroapp/web/main.cpp
/tmp/avogadro-qa-tools/root/usr/bin/clang-tidy-18 -p build-native-web/libs \
  avogadrolibs/avogadro/qtplugins/editor/editor.cpp \
  avogadrolibs/avogadro/qtplugins/editor/editortoolwidget.cpp \
  avogadrolibs/avogadro/rendering/shaderprogram.cpp
```

Static analysis found and prompted fixes for missing-factory lookup and null
pointer arithmetic in GL buffer offsets. No application-source warnings remain
in the controller/main clang-tidy run. The existing library style configuration
still reports inherited initialization/style warnings, the const cast required
by the existing const `toolWidget()` interface, and the GL buffer-offset cast.
cppcheck reports the existing virtual `setIcon()` call in the Editor constructor.
These are not reported as a clean, warning-free analysis run.

## Artifacts and remaining limits

The release WASM is approximately 16.8 MB (6.15 MB gzip), with about 251 KB of
Emscripten JavaScript and 12 KB of Qt loader JavaScript. Exact final sizes and
SHA-256 hashes are recorded in `build-web/release-info.json` beside the archive.
The fresh stage is `build-web/stage/share/avogadro-web/`.

Browser checks used software rendering, not a hardware GPU. Safari, tablets,
mobile devices, production HTTPS hosting, and browser-native navigation prompt
UI were not exercised. `beforeunload` event cancellation passed its regression check, but browser
policies control whether its prompt appears. The full native Qt package tests
and unrelated quantum/force-field suites were not run. The native desktop needs
the selected Qt 6.11.1 runtime on its library path when the host also has an
incompatible system Qt. Publishing remains outside this work.
