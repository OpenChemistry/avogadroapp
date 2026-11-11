# cmake

CMake helpers for the build and everything that turns it into an installer:

| File | Purpose |
|------|---------|
| `BuildType.cmake`, `CompilerFlags.cmake`, `BuildLocation.cmake`, `InstallLocation.cmake` | Build types (including the sanitizer types), warnings, output and install directories |
| `DetermineVersion.cmake` | Version string from git; `AVOGADRO_RELEASE_BUILD=ON` drops the date and hash |
| `AvogadroCPack.cmake`, `AvogadroCPackOptions.cmake.in` | CPack setup: DMG, NSIS, plus bundling of avogadrolibs plugins, Qt and Open Babel |
| `deploy-osx.cmake.in`, `MacOSXBundleInfo.plist.in`, `Entitlements.plist` | macOS bundle fix-up, code signing and `Info.plist` |
| `avogadro2.nsi.in` | Windows NSIS installer script |
| `AppxManifest.xml.in` | Windows MSIX package manifest (Microsoft Store) |
| `BuildPackage*.cmake` | Optional `BUILD_PACKAGE_TEST` ctest that builds a package (on for dashboards) |
| `COPYING.openbabel`, `COPYING.sentry` | Licenses of bundled components, installed with the app |

The Flatpak manifest is [../flatpak/org.openchemistry.Avogadro2.yaml](../flatpak/org.openchemistry.Avogadro2.yaml),
and the per-platform CI and release builds are in `../.github/workflows/`.
See the top-level [AGENTS.md](../AGENTS.md) for the rules that apply
everywhere.

Packaging changes can only really be tested by building and installing the
package on that platform. Say in the PR which installers you built and
installed, and which you could not.

## Version numbers

The version is set once, as `AvogadroApp_VERSION_MAJOR/MINOR/PATCH` in the
top-level `CMakeLists.txt`, and flows into every package. Several consumers
need it purely numeric: `CFBundleVersion` on macOS and the four-part
`Identity Version` in the MSIX manifest. Keep dates and git hashes in the
display strings only (`MACOSX_BUNDLE_INFO_STRING`, `SNAPSHOT_DATE` for nightly
DMG names).

## Things that must stay in step

- **File associations** are listed separately for each platform: the
  `FileTypeAssociation` in `AppxManifest.xml.in`, the `Push ".ext"` blocks in
  `avogadro2.nsi.in`, `CFBundleDocumentTypes` in `MacOSXBundleInfo.plist.in`,
  and `MimeType=` in `../avogadro/metainfo/org.openchemistry.Avogadro2.desktop`.
  Change them together.
- **Installed icons**: the Windows and MSIX icons installed from
  `../avogadro/CMakeLists.txt` must match the file names the NSIS and MSIX
  manifests refer to.
- **The Windows diagnostic build** (`USE_SENTRY`) installs beside a release
  install. Everything derived from `NSIS_APP_NAME` differs, and it registers
  no file associations, so that uninstalling it does not break the user's
  normal Avogadro. Keep that separation when changing the NSIS script.

## Identity, signing and secrets

- Do not change the MSIX `Identity` (name and publisher) or the macOS bundle
  identifier (`cc.avogadro`). They tie installs, updates and store listings
  to the project; changing them makes a different application.
- Signing identities and certificates come from CI secrets and environment
  variables (`CODESIGN_IDENTITY`). Never commit them or write them into a
  file that gets installed or logged.
- The Sentry DSN (`SENTRY_DSN`) is a public client key: it only allows
  sending events, so it is compiled into the diagnostic build
  (`avogadroappconfig.h`). It still comes from a CI secret so that it is not
  in the repository or in builds that should not report. Never put a Sentry
  auth token (used to upload symbols) anywhere that gets installed.
- Keep entitlements and sandbox permissions to what the app needs. The macOS
  entitlements allow JIT and loading libraries that are not signed by the
  project; the Flatpak has display, GPU, and network for plugin downloads,
  plus `--share=ipc`. Do not add camera, microphone, location, broad
  file-system or network-server permissions; a new one needs a stated reason
  in the PR.

## Bundled dependencies

On macOS and Windows the installer must be self-contained (`INSTALL_BUNDLE_FILES`).
A new runtime dependency, in the app or in avogadrolibs, needs:

- an install rule, or a check that `fixup_bundle` in
  `../avogadro/lastinstall/` finds it. That directory must stay the last
  `add_subdirectory()` in `../avogadro/CMakeLists.txt`;
- on macOS, signing: `deploy-osx.cmake.in` signs the Open Babel plugins
  explicitly, and other loose libraries need the same;
- its license installed alongside the existing `COPYING.*` files, and a
  maintainer's agreement that the license allows bundling;
- a matching module in the Flatpak manifest, with a pinned version or commit.

Windows Qt is deliberately pinned (`QT_VERSION: 6.8.3` in
`build_windows.yml`); newer versions fail to deploy on some Windows 10
systems. Do not bump it as a side effect of another change.
