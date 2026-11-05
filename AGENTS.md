# Avogadro Application: Guide for Contributors and Coding Agents

Avogadro is an open source, cross-platform C++ desktop application for
building, editing and visualizing molecules and materials, running simple
simulations and geometry optimizations, and analyzing results. This repository,
avogadroapp, holds the application shell: the main window, menus, settings,
the JSON-RPC server for scripting, crash reporting and the installers. The
libraries and nearly all features live in
[avogadrolibs](https://github.com/OpenChemistry/avogadrolibs).

This file explains how the code is organized and **why**, so that new code
lands in the right place. Each directory below has its own `AGENTS.md` with
the rules that apply there; read the one for the directory you are working in.
The avogadrolibs `AGENTS.md` files cover the libraries.

## AI use policy

Code needs to be **correct**, particularly for science: force fields, molecular
dynamics, surfaces, orbitals, unit conversions and file parsing all need
careful human review.

Our policy (see
[AI Usage in avogadrolibs/CONTRIBUTING.md](https://github.com/OpenChemistry/avogadrolibs/blob/master/CONTRIBUTING.md#ai-usage))
is that only **levels 0 and 1** of AI assistance are accepted: autocomplete,
and small snippets or boilerplate that a human reviews in full. Agents are
welcome to explain code, suggest approaches, help debug and help review. A
human must write, or fully understand and own, every line that is contributed.
Contributions that appear to be mostly machine-generated may be rejected.

## Where does this code go?

**The application is a thin shell.** It hosts windows and docks, builds menus
from what the plugins provide, stores settings, dispatches RPC requests and
packages everything for installation. Features belong in avogadrolibs, where
they can be tested in isolation and reused by the Python package and other
front ends.

Ask these questions in order:

1. **Is it a user-facing feature: a menu action, an interactive tool, a
   display type, a dialog for a calculation?** It is a plugin in
   `avogadrolibs/avogadro/qtplugins/`, or a Python plugin in its own
   repository. Not here.
2. **Is it shared Qt code that the app and plugins both need?** It goes in
   `avogadrolibs/avogadro/qtgui/`.
3. **Is it a scriptable action on a molecule?** It is a plugin command
   declared through `QtGui::CommandRegistry` in avogadrolibs (see
   [avogadro/rpc/AGENTS.md](avogadro/rpc/AGENTS.md)), not a new built-in RPC
   method here.
4. **Does it concern the application as a whole?** Startup, command-line
   options, the window layout and docks, the list of open molecules,
   file-open and save dispatch, autosave, settings, platform integration
   (file associations, macOS open events, 3Dconnexion), crash reporting, or
   packaging. Then it belongs here.

`mainwindow.cpp` is already very large. Prefer not to grow it: if a change
needs more than a small hook in `MainWindow`, look for a plugin or a `qtgui`
class that could own the behavior instead.

## Layout

| Directory | Purpose |
|-----------|---------|
| [avogadro/](avogadro/AGENTS.md) | The `avogadro2` executable: `main()`, `MainWindow`, `MenuBuilder`, `ViewFactory`, `RpcListener`, crash reporter, 3Dconnexion, icons, Linux desktop and metainfo files |
| [avogadro/rpc/](avogadro/rpc/AGENTS.md) | `AvogadroRPC`, a static library with the JSON-RPC 2.0 transport over local sockets |
| [tests/](tests/AGENTS.md) | QtTesting XML playback tests and the `tests/rpc` end-to-end suite |
| [cmake/](cmake/AGENTS.md) | Build helpers and the installers: CPack, NSIS, MSIX, macOS bundle and signing |
| `flatpak/` | Flatpak manifest (`org.openchemistry.Avogadro2.yaml`) |
| `i18n/` | Gettext `.po` files for the app's strings, updated by a workflow; see [Translations](#translations) |
| `scripts/` | Message extraction for translations and small RPC client examples |
| `thirdparty/3DConnexion/` | Vendored 3Dconnexion navigation SDK headers and loader |

The app links `Avogadro::QtOpenGL`, `Avogadro::QtGui` and `Avogadro::QtPlugins`
from an installed avogadrolibs (`find_package(AvogadroLibs)`). It may use any
installed avogadrolibs header, but avogadrolibs must never depend on the app.
If the app needs something the libraries do not expose, add it to the
libraries first.

## Rules that apply everywhere

- **Never crash.** No exceptions and no asserts in production code. Validate
  input, especially from files, the command line and RPC clients, check
  indices and sizes, and return an error instead.
- C++17, portable across MSVC, Clang and GCC on Windows, macOS, Linux and BSD.
  Platform code goes behind `Q_OS_*` (or CMake platform checks) with a
  working fallback, and must still build on the platforms that do not use it.
- Qt 6 only. Prefer Qt facilities over new dependencies, and ask before
  adding a third-party library; every one has to be bundled into four
  installers.
- Use `override`, `nullptr`, `constexpr`, `noexcept` and `enum class`. Use C++
  casts (`static_cast`, and `qobject_cast` for QObjects), never C-style casts.
  Prefer new-style `connect()` with member function pointers over
  `SIGNAL()`/`SLOT()` strings in new code.
- Prefer smart pointers or Qt parent ownership over manual `new`/`delete`.
- Headers: avogadrolibs headers use angle brackets
  (`#include <avogadro/qtgui/molecule.h>`), local ones use quotes. Order: local,
  Avogadro libraries, Qt (specific classes, not whole modules), STL.
  Forward-declare in headers.
- Every user-visible string goes through `tr()` (or
  `QCoreApplication::translate` outside QObjects).

### Privacy and security

Avogadro is used on unpublished research, often on shared machines. Treat
everything a user opens or draws as confidential.

- **Collect nothing without consent.** No telemetry, analytics or usage
  reporting. Any new network request needs a clear user benefit, a way to turn
  it off, and agreement from the maintainers before it lands. Today the only
  request this repository makes on its own is the startup update check, a
  plain `GET` of the GitHub releases API that carries no user data.
- **Never send or log user data**: molecule names, titles, formulas,
  structures, file names and paths, user names and home directories. This
  applies to crash reports, `qDebug`/`qWarning` output (which crash reports
  collect as breadcrumbs), and any error message that might be pasted into a
  public issue. Log the kind of failure, not the content.
- **Crash reporting stays opt-in.** It exists only in the Windows diagnostic
  build (`USE_SENTRY`), uploads only after the user agrees, and scrubs the home
  directory. See [avogadro/AGENTS.md](avogadro/AGENTS.md#crash-reporting)
  before touching it.
- **RPC is local and untrusted.** The server listens only on a local socket.
  Do not add a network listener, and treat every request as hostile input.
  See [avogadro/rpc/AGENTS.md](avogadro/rpc/AGENTS.md).
- Report vulnerabilities privately, as described in [SECURITY.md](SECURITY.md).

### Dialogs and automation

- Prefer non-modal dialogs and dock widgets. A modal dialog runs a nested event
  loop: the RPC request that opened it never gets a reply, and a script driving
  Avogadro stalls.
- Every startup or error dialog must respect `--skip-dialogs`
  (`dialogsSkipped()` in `MainWindow`, `QtGui::Utilities::dialogsSkipped()`
  for plugins): log a `qInfo("--skip-dialogs: skipped ...")` line and take the
  safe default instead of showing it. `--rpc-name` implies `--skip-dialogs`.

### Settings

- **Settings keys are a compatibility contract.** Users carry their
  `QSettings` (organization `OpenChemistry`, application `Avogadro`) across
  versions, and plugins in avogadrolibs read and write the same store. Do not
  rename or remove a key, or change the type or meaning of its value, without
  code that migrates the old value. Check avogadrolibs for other readers of
  the key before changing it.
- Group new keys under a section (`MainWindow/...`, `autosave/...`) rather
  than adding more top-level keys, and give `value()` a sensible default so a
  fresh or cleared store works.

## Build, test and format

Most developers build through the `openchemistry` superbuild, which builds
avogadrolibs first; see the
[build instructions](https://two.avogadro.cc/develop/build.html). The app needs
an installed avogadrolibs of the same vintage: the two repositories are
developed together, and an app change often depends on a libraries change that
must merge first.

```sh
# in the superbuild build directory
cmake --build .
ctest --test-dir avogadroapp            # app tests (needs ENABLE_TESTING=ON)
ctest --test-dir avogadroapp -L rpc     # only the RPC end-to-end tests
```

See [tests/AGENTS.md](tests/AGENTS.md) for what the tests cover and how to add
one. Useful CMake options: `Avogadro_ENABLE_RPC` (on), `ENABLE_TESTING`
(off), `USE_3DCONNEXION` (off; superbuild option, Windows and macOS only) and
`USE_SENTRY` (off; Windows diagnostic builds only).

Before submitting:

- Format the changed lines with `clang-format` (Mozilla-based style in
  [.clang-format](.clang-format)), for example `git clang-format origin/master`.
  CI rejects formatting differences on changed lines and also runs
  `clang-tidy` with the checks in [.clang-tidy](.clang-tidy).
- Add an RPC test for a new RPC behavior and a regression test for a bug fix
  where the harness can reach it.
- Build with warnings visible. CI builds with ASan, TSan and UBSan, and runs
  the RPC suite in the UBSan build.
- Commit messages: a short imperative sentence, optionally followed by a blank
  line and details.

### Translations

Wrap strings in `tr()`; nothing else is needed. A workflow runs
[scripts/extract-messages.sh](scripts/extract-messages.sh) to refresh
`i18n/avogadroapp.pot`, translators work in Weblate, and the compiled `.qm`
files come from the separate
[avogadro-i18n](https://github.com/OpenChemistry/avogadro-i18n) repository. Do
not edit the `.po` or `.pot` files by hand.

## References

- API documentation: https://two.avogadro.cc/develop/classlist.html
- Build guide: https://two.avogadro.cc/develop/build.html
- Forum: https://discuss.avogadro.cc/
- [CONTRIBUTING.md](CONTRIBUTING.md), [SECURITY.md](SECURITY.md)
