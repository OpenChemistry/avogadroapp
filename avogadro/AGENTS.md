# avogadro

The `avogadro2` executable (`Avogadro2.app` on macOS):

- `avogadro.cpp`: `main()`. OpenGL surface format, translations, command-line
  options, then `MainWindow` and the `RpcListener`.
- `application.cpp`: `QApplication` subclass that turns macOS file-open
  events into `MainWindow::openFile()`.
- `mainwindow.cpp`: the window, docks, open molecules, plugin loading, file
  open/save/export, autosave, settings, and the built-in RPC commands that
  change the document (`handleCommand()`).
- `menubuilder.cpp`: builds menus from plugin actions by path and priority.
- `viewfactory.cpp`: creates the views (`GLWidget`) a molecule is shown in.
- `backgroundfileformat.cpp`: runs an `Io::FileFormat` read or write on a
  worker thread.
- `rpclistener.cpp`: answers RPC requests; transport is in [rpc/](rpc/AGENTS.md).
- `crashreporter.cpp`, `crashreportdialog.cpp`: Sentry crash reporting,
  compiled only with `USE_SENTRY`.
- `tdxcontroller.cpp`: 3Dconnexion SpaceMouse navigation (Windows, macOS).
- `icons/`, `metainfo/`, `avogadro.qrc`, `avogadro.manifest`: icons, Linux
  `.desktop` and AppStream files, Qt resources, the Windows manifest.
- `lastinstall/`: the `fixup_bundle` step; it must stay the last
  `add_subdirectory()` so it runs after every other install rule.

See the top-level [AGENTS.md](../AGENTS.md) for what belongs in the app at
all, and the rules that apply everywhere.

## MainWindow

`MainWindow` is the largest class in the project. Keep additions to it small:

- Plugin actions reach the menus through `MenuBuilder`, using the path and
  priority each `ExtensionPlugin` reports. To move or add a menu entry for a
  plugin, change the plugin in avogadrolibs, not the menu code here.
  Priorities group into sections by hundreds; a separator appears when an
  entry crosses a multiple of 100.
- The window owns the open molecules. Plugins see the active one through
  `setMolecule()`; anything that replaces or closes a molecule must go
  through `setMolecule()`/`closeMolecule()` so that plugins, layers and the
  undo stack follow.
- Files are opened on a worker thread (`BackgroundFileFormat`) into a fresh
  `Molecule` while a progress dialog is shown, and saves are asynchronous by
  default. Do not read or write molecule files synchronously on the GUI
  thread, and do not touch the molecule being read until the reader reports
  that it has finished.
- File formats that Open Babel provides are registered a moment after the
  window appears (`pluginFormatsSettled()`, `pluginFormatsReady()`). Code that
  opens files at startup, from the command line or from RPC must wait for
  them, as the existing queues do.

## Command-line options

Options are parsed by hand in `main()`. A new option must be added to
`isKnownOption()` as well as to the parsing loop; an unknown option makes the
app exit with an error, and an unlisted option after a value-taking option is
swallowed as its value. Current options: `--rpc-name`, `--disable-settings`,
`--skip-autosave`, `--skip-dialogs`, and the testing-only `--test-file`,
`--test-no-exit` and `--crash-test`. Anything else not starting with `-` is a
file to open.

`--disable-settings` **clears the user's real settings store** before
starting; it does not use a separate one. Do not use it, or recommend it, on a
machine whose settings matter.

## Settings

Settings are read in `readSettings()` and written in `writeSettings()`, both
in `mainwindow.cpp`. Window geometry (`size`, `pos`) and `perspective` are in
the `MainWindow` group, while dock visibility (`moleculeDock`, `viewDock`,
...) and `recentFiles` are top level for historical reasons. Leave existing
keys where they are, and put new keys in the group they belong to. The top-level rules on keeping keys stable apply. Plugins share the
same store under their own keys, so grep avogadrolibs before reusing a name.

## Platform code

- Keep platform branches small and behind `Q_OS_MAC`, `Q_OS_WIN`,
  `Q_OS_LINUX` (or `APPLE`/`WIN32` in CMake), with a fallback for the others,
  including BSD.
- `mac.mm` is not currently compiled (its line in `CMakeLists.txt` is
  commented out).
- File associations are declared in several places that must agree:
  `MimeType=` in `metainfo/org.openchemistry.Avogadro2.desktop` (Linux),
  `cmake/MacOSXBundleInfo.plist.in`, and the NSIS and MSIX files in
  `cmake/`. See [cmake/AGENTS.md](../cmake/AGENTS.md).
- The 3Dconnexion SDK is loaded at run time (`navlib_load.cpp`), so the app
  must still start, without SpaceMouse support, when the driver is missing.

## Crash reporting

Crash reporting uses sentry-native and exists only in the Windows diagnostic
build (`USE_SENTRY=ON`, which CMake refuses on other platforms). Release builds
compile the no-op inline functions in `crashreporter.h` instead. Rules:

- **Consent first.** Nothing is uploaded until the user has agreed in the
  consent dialog; the choice lives in `crashReporting/consent` and
  `crashReporting/consentAsked`, and sentry is initialized with
  `require_user_consent` so reports are held locally until then. Under
  `--skip-dialogs` the question is not asked: a user who never answered is
  treated as not consenting, and the stored choice is left alone. Keep it
  that way. The one exception is `--crash-test`: it gives consent for that
  run only, without the dialog and without changing the stored choice, then
  crashes, so it can upload a test crash. It exists for CI and support; no
  other path may grant consent.
- **Add no user data.** Tags and context describe the build, OpenGL driver and
  loaded plugins. Do not add molecule names, formulas, structures, file names
  or paths, window titles, or the user's name. Breadcrumbs follow the same
  rule: the file-open breadcrumb records the format
  (`open format=Avogadro: CJSON`), not the file name. Anything a new field might carry has to go through the
  home-directory scrubbing in `scrubEvent()`.
- Scrubbing on the client only covers non-crash events and breadcrumbs. For a
  real crash the event is rebuilt on the server from the minidump, so the
  Sentry project's server-side scrubbing rules matter too; mention that in
  the PR if a change affects what a minidump might contain.
- The `before_send` callback can run inside an exception filter: do not call
  Qt or allocate there. Prepare anything it needs in `initialize()`.

## Tests

Changes here are tested end to end through RPC; see
[tests/AGENTS.md](../tests/AGENTS.md). If a change adds or changes a startup
or error dialog, check that the RPC suite's dialog oracle still passes.
