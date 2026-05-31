include(ExternalProject)

find_package(AvogadroLibs REQUIRED NO_MODULE)
include_directories(${AvogadroLibs_INCLUDE_DIRS})
list(APPEND CMAKE_MODULE_PATH ${AvogadroLibs_CMAKE_DIR})
find_package(Eigen3 REQUIRED)
include_directories(SYSTEM ${EIGEN3_INCLUDE_DIR})

option(USE_PLOTTER "Enable JKQTPlotter for charts and spectra" ON)
if(USE_PLOTTER)
  # should be passed along by avogadrolibs as well
  find_package(JKQTPlotter${QT_VERSION} REQUIRED)
endif()

if(MSVC)
  add_definitions("-D_CRT_SECURE_NO_WARNINGS")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")
endif()

# Find the Qt components we need.
if(QT_VERSION EQUAL 6)
  find_package(Qt6
    COMPONENTS
      Concurrent
      Widgets
      Network
      OpenGL
      OpenGLWidgets
    REQUIRED)
endif()

configure_file(avogadroappconfig.h.in avogadroappconfig.h)

set(avogadro_srcs
  aboutdialog.cpp
  application.cpp
  avogadro.cpp
  backgroundfileformat.cpp
  mainwindow.cpp
  menubuilder.cpp
  renderingdialog.cpp
  tooltipfilter.cpp
  viewfactory.cpp
)

qt_wrap_ui(ui_srcs
  aboutdialog.ui
  mainwindow.ui
  renderingdialog.ui
)

qt_add_resources(rcc_srcs avogadro.qrc)

# download and install QM translation files
# Look in parallel directory for the i18n repository
set(_i18n "${AvogadroApp_SOURCE_DIR}/../avogadro-i18n")
if(NOT EXISTS "${_i18n}")
# download molecules...
ExternalProject_Add(i18n
    GIT_REPOSITORY https://github.com/openchemistry/avogadro-i18n
    SOURCE_DIR "${AvogadroApp_SOURCE_DIR}/../avogadro-i18n"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
)
endif()

install(DIRECTORY "${AvogadroApp_SOURCE_DIR}/../avogadro-i18n/avogadroapp/"
  "${AvogadroApp_SOURCE_DIR}/../avogadro-i18n/avogadrolibs/"
  DESTINATION "${INSTALL_DATA_DIR}/avogadro2/i18n"
  FILES_MATCHING PATTERN "*.qm"
)

if(APPLE OR WIN32 OR INSTALL_BUNDLE_FILES)
  # copy the qt translation files
  get_filename_component(_qttranslationdir "${Qt${QT_VERSION}_DIR}/../../../translations" ABSOLUTE)
  message(STATUS "Install Qt translations ${_qttranslationdir}")
  install(DIRECTORY "${_qttranslationdir}/"
    DESTINATION "${INSTALL_DATA_DIR}/avogadro2/i18n"
    FILES_MATCHING PATTERN "qt_*.qm" PATTERN "qtbase*.qm"
    # technically, this also copies qt_help.* but that's not a big deal
  )
endif()

if(Avogadro_ENABLE_RPC)
  add_subdirectory(rpc)
  list(APPEND avogadro_srcs rpclistener.cpp rpclistener.h)
  add_definitions("-DAvogadro_ENABLE_RPC")
endif()

if(ENABLE_TESTING)
  if(QT_VERSION EQUAL 6)
    find_package(Qt6 COMPONENTS Test REQUIRED)
  endif()
  find_package(QtTesting REQUIRED NO_MODULE)
  include_directories(${QtTesting_INCLUDE_DIRS})
  link_directories(${QtTesting_LIBRARY_DIR})
  add_definitions(-DQTTESTING)
endif()

# Need to add current binary dir to include path for UI file headers
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# if we are building statically then we need HDF5 targets
if(NOT BUILD_SHARED_LIBS AND USE_HDF5)
  find_package(HDF5 REQUIRED COMPONENTS C)
endif()

# Handle Mac OS X specific icons etc.
if(APPLE)
  # add some Objective-C++ code
  # list(APPEND avogadro_srcs mac.mm)

  # for all the translations, create a .lproj directory
  # Migrated from Avo 1.x - contributed by Geoff Hutchison
  # We're assuming that there will never be an libavogadro translation
  #  without a corresponding avogadro one.
  # (Fairly safe assumption)

  # This regex gives us translations with just two characters
  #  (e.g., avogadro_fr.ts)
  FILE(GLOB avogadro_QM "${INSTALL_DATA_DIR}/avogadro2/i18n/*.qm")
  string(REGEX MATCHALL "-[a-z]+\\.qm" langresult "${avogadro_QM}")
  string(REGEX REPLACE "-([^.]+)\\.qm" "\\1" languages "${langresult}")
  # This one gives us languages for country codes (e.g., avogadro_zh_CN.ts)
  string(REGEX MATCHALL "-[a-z]+_[A-Z]+\\.qm" langresult "${avogadro_QM}")
  string(REGEX REPLACE "-([^_]+_[^.]+)\\.qm" "\\1" fulllangs "${langresult}")
  set (LANGUAGES "${languages};${fulllangs};en;en_US")

  foreach(lang ${LANGUAGES})
    set( MAC_LANG_DIR "${CMAKE_INSTALL_PREFIX}/Resources/${lang}.lproj" )
    install(CODE "EXECUTE_PROCESS(COMMAND
      mkdir ${MAC_LANG_DIR}
      ERROR_QUIET)")
  endforeach()

  list(APPEND avogadro_srcs icons/Assets.car)
  list(APPEND avogadro_srcs icons/AppIcon.icns)
  list(APPEND avogadro_srcs icons/doc.icns)
  list(APPEND avogadro_srcs icons/cml.icns)
  list(APPEND avogadro_srcs icons/cjson.icns)
  set(MACOSX_BUNDLE_ICON_FILE AppIcon)
  set(MACOSX_BUNDLE_BUNDLE_VERSION "${AvogadroApp_VERSION}")
  set(MACOSX_BUNDLE_INFO_STRING "Avogadro - version ${AvogadroApp_VERSION}")

  # Change following line to point to actual icns file in bundle.
  set(MACOSX_BUNDLE_GUI_IDENTIFIER "cc.avogadro")
  set(MACOSX_BUNDLE_BUNDLE_NAME "Avogadro")

  set_source_files_properties(icons/Assets.car PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)
  set_source_files_properties(icons/AppIcon.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)
  set_source_files_properties(icons/doc.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)
  set_source_files_properties(icons/cjson.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)
  set_source_files_properties(icons/cml.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)

elseif(UNIX)
  install(FILES "metainfo/org.openchemistry.Avogadro2.desktop" DESTINATION "${INSTALL_XDG_APPS_DIR}")
  install(FILES "metainfo/org.openchemistry.Avogadro2.metainfo.xml" DESTINATION "${CMAKE_INSTALL_PREFIX}/share/metainfo")
  install(FILES "icons/avogadro.png" DESTINATION "${INSTALL_XDG_ICON_DIR}/hicolor/32x32/apps"
    RENAME "org.openchemistry.Avogadro2.png")
  install(FILES "icons/avogadro.png" DESTINATION "${INSTALL_XDG_ICON_DIR}/hicolor/16x16@2/apps"
    RENAME "org.openchemistry.Avogadro2.png")
  install(FILES "icons/avogadro2_64.png" DESTINATION "${INSTALL_XDG_ICON_DIR}/hicolor/64x64/apps"
    RENAME "org.openchemistry.Avogadro2.png")
  install(FILES "icons/avogadro2_64.png" DESTINATION "${INSTALL_XDG_ICON_DIR}/hicolor/32x32@2/apps"
    RENAME "org.openchemistry.Avogadro2.png")
  install(FILES "icons/avogadro2_128.png" DESTINATION "${INSTALL_XDG_ICON_DIR}/hicolor/128x128/apps"
    RENAME "org.openchemistry.Avogadro2.png")
  install(FILES "icons/avogadro2_256.png" DESTINATION "${INSTALL_XDG_ICON_DIR}/hicolor/256x256/apps"
    RENAME "org.openchemistry.Avogadro2.png")
  install(FILES "icons/avogadro2.svg" DESTINATION "${INSTALL_XDG_ICON_DIR}/hicolor/scalable/apps"
    RENAME "org.openchemistry.Avogadro2.svg")
elseif(WIN32)
  list(APPEND avogadro_srcs icons/avogadro.rc)
  # copy icons
  install(FILES "icons/avogadro.png" DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons")
  install(FILES "icons/avogadro.ico" DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons")
  install(FILES "icons/doc.ico" DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons")
  install(FILES "icons/cml.ico" DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons")
  install(FILES "icons/cjson.ico" DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons")

  # for the MSIX installer
  install(FILES "icons/Avogadro2-310x150.png" DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons")
  install(FILES "icons/Avogadro2_44.png" DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons")
  install(FILES "icons/Avogadro2_71.png" DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons")
  install(FILES "icons/Avogadro2_150.png" DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons")
  install(FILES "icons/Avogadro2_310.png" DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons")

  # configure the NSI and MSIX installer scripts
  configure_file("${AvogadroApp_SOURCE_DIR}/cmake/avogadro2.nsi.in" "${CMAKE_INSTALL_PREFIX}/avogadro2.nsi" @ONLY)
  configure_file("${AvogadroApp_SOURCE_DIR}/cmake/AppxManifest.xml.in" "${CMAKE_INSTALL_PREFIX}/AppxManifest.xml" @ONLY)
endif()

if(USE_3DCONNEXION AND (WIN32 OR APPLE))
  list(APPEND avogadro_srcs icons/3dx_pivot.png)
  list(APPEND avogadro_srcs tdxcontroller.cpp)
  list(APPEND avogadro_srcs "${AvogadroApp_SOURCE_DIR}/thirdparty/3DConnexion/src/navlib_load.cpp")
  list(APPEND avogadro_srcs "${AvogadroApp_SOURCE_DIR}/thirdparty/3DConnexion/src/navlib_stub.c")
  set_source_files_properties(icons/3dx_pivot.png PROPERTIES
  MACOSX_PACKAGE_LOCATION Resources)
endif()

add_executable(avogadro WIN32 MACOSX_BUNDLE
  ${avogadro_srcs} ${ui_srcs} ${rcc_srcs})
if(EMSCRIPTEN)
  find_program(AVOGADRO_GZIP_EXECUTABLE gzip)
  target_compile_options(avogadro PRIVATE -pthread)
  target_link_options(avogadro PRIVATE
    -pthread
    --bind
    "SHELL:-sMIN_WEBGL_VERSION=2"
    "SHELL:-sMAX_WEBGL_VERSION=2"
    "SHELL:-sEXPORTED_RUNTIME_METHODS=['specialHTMLTargets','JSEvents']")
  if(AVOGADRO_GZIP_EXECUTABLE)
    add_custom_command(TARGET avogadro POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy
        "$<TARGET_FILE_DIR:avogadro>/$<TARGET_FILE_BASE_NAME:avogadro>.wasm"
        "$<TARGET_FILE_DIR:avogadro>/$<TARGET_FILE_BASE_NAME:avogadro>.wasm.gz.tmp"
      COMMAND "${AVOGADRO_GZIP_EXECUTABLE}" -9 -f
        "$<TARGET_FILE_DIR:avogadro>/$<TARGET_FILE_BASE_NAME:avogadro>.wasm.gz.tmp"
      COMMAND "${CMAKE_COMMAND}" -E rename
        "$<TARGET_FILE_DIR:avogadro>/$<TARGET_FILE_BASE_NAME:avogadro>.wasm.gz.tmp.gz"
        "$<TARGET_FILE_DIR:avogadro>/$<TARGET_FILE_BASE_NAME:avogadro>.wasm.gz"
      COMMENT "Compressing $<TARGET_FILE_BASE_NAME:avogadro>.wasm"
      VERBATIM)
  endif()
endif()
target_link_libraries(avogadro Qt::Widgets Qt::Network Qt::Concurrent)
if(WIN32)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:MSVCRTD")
endif()
set_target_properties(avogadro PROPERTIES AUTOMOC TRUE)
target_link_libraries(avogadro Avogadro::QtOpenGL Avogadro::QtGui Avogadro::QtPlugins)
if(Avogadro_ENABLE_RPC)
  target_link_libraries(avogadro AvogadroRPC)
endif()
if(APPLE)
  set_target_properties(avogadro PROPERTIES OUTPUT_NAME ${MACOSX_BUNDLE_NAME})
else()
  set_target_properties(avogadro PROPERTIES OUTPUT_NAME "avogadro2")
endif()
if(ENABLE_TESTING)
  target_link_libraries(avogadro qttesting)
endif()
if(QT_VERSION EQUAL 6)
  target_link_libraries(avogadro Qt6::OpenGL Qt6::OpenGLWidgets)
endif()

if(USE_3DCONNEXION AND (WIN32 OR APPLE))
  target_compile_definitions(avogadro PUBLIC _3DCONNEXION)
  target_include_directories(avogadro PUBLIC "${AvogadroApp_SOURCE_DIR}/thirdparty/3DConnexion/inc")
	if(APPLE)
		target_compile_definitions(avogadro PUBLIC __APPLE__)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdeclspec")
	elseif(WIN32)
		install(FILES "icons/3dx_pivot.png" DESTINATION "${CMAKE_INSTALL_BINDIR}/img")
	endif()
endif()

if(EMSCRIPTEN)
  file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/index.html"
    CONTENT [[<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Avogadro wasm</title>
    <style>
      html, body, #screen {
        margin: 0;
        width: 100%;
        height: 100%;
        overflow: hidden;
      }
    </style>
  </head>
  <body>
    <div id="screen"></div>
    <script>
      var screen = document.getElementById("screen");
      var Module = {
        qtContainerElements: [screen],
        print: console.log,
        printErr: console.error
      };
    </script>
    <script src="$<TARGET_FILE_NAME:avogadro>"></script>
  </body>
</html>
]])
  file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/serve-wasm.py"
    CONTENT [[#!/usr/bin/env python3
from pathlib import Path
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit


class Handler(SimpleHTTPRequestHandler):
    extensions_map = {
        **SimpleHTTPRequestHandler.extensions_map,
        ".wasm": "application/wasm",
    }

    def do_GET(self):
        parsed = urlsplit(self.path)
        if parsed.path.endswith(".wasm"):
            accept_encoding = self.headers.get("Accept-Encoding", "")
            compressed_path = Path(self.translate_path(parsed.path + ".gz"))
            if "gzip" in accept_encoding and compressed_path.is_file():
                self.send_response(200)
                self.send_header("Content-Type", "application/wasm")
                self.send_header("Content-Encoding", "gzip")
                self.send_header("Vary", "Accept-Encoding")
                self.send_header("Content-Length", str(compressed_path.stat().st_size))
                self.end_headers()
                with compressed_path.open("rb") as source:
                    self.copyfile(source, self.wfile)
                return

        super().do_GET()

    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cross-Origin-Resource-Policy", "same-origin")
        super().end_headers()


if __name__ == "__main__":
    ThreadingHTTPServer(("127.0.0.1", 8000), Handler).serve_forever()
]])
endif()

install(TARGETS avogadro
  RUNTIME DESTINATION ${INSTALL_RUNTIME_DIR}
  BUNDLE DESTINATION .
  )
if(EMSCRIPTEN)
  install(FILES "$<TARGET_FILE_DIR:avogadro>/$<TARGET_FILE_BASE_NAME:avogadro>.wasm"
    DESTINATION ${INSTALL_RUNTIME_DIR})
  if(AVOGADRO_GZIP_EXECUTABLE)
    install(FILES "$<TARGET_FILE_DIR:avogadro>/$<TARGET_FILE_BASE_NAME:avogadro>.wasm.gz"
      DESTINATION ${INSTALL_RUNTIME_DIR})
  endif()
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/index.html"
    DESTINATION ${INSTALL_RUNTIME_DIR})
  install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/serve-wasm.py"
    DESTINATION ${INSTALL_RUNTIME_DIR})
endif()

# Keep "add_subdirectory(lastinstall)" last: fixup_bundle needs to be
# *after* all other install(TARGETS and install(FILES calls
add_subdirectory(lastinstall)
