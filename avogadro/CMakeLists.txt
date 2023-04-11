include(ExternalProject)

find_package(AvogadroLibs REQUIRED NO_MODULE)
include_directories(${AvogadroLibs_INCLUDE_DIRS})
list(APPEND CMAKE_MODULE_PATH ${AvogadroLibs_CMAKE_DIR})
find_package(Eigen3 REQUIRED)
include_directories(SYSTEM ${EIGEN3_INCLUDE_DIR})

option(USE_VTK "Build with VTK support" OFF)
if(USE_VTK)
  find_package(VTK COMPONENTS GUISupportQt RenderingOpenGL2 REQUIRED)
  add_definitions(-DAVO_USE_VTK)
endif()

if(MSVC)
  add_definitions("-D_CRT_SECURE_NO_WARNINGS")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")
endif()

# Find the Qt components we need.
if(QT_VERSION EQUAL 6)
  find_package(Qt6 COMPONENTS Concurrent Widgets Network Core5Compat REQUIRED)
else()
  find_package(Qt5 COMPONENTS Concurrent Widgets Network REQUIRED)
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
get_filename_component(_qttranslationdir "${Qt5_DIR}/../../../translations" ABSOLUTE)
message(STATUS "Trying to install Qt translations ${_qttranslationdir}")
install(DIRECTORY "${_qttranslationdir}/"
DESTINATION "${INSTALL_DATA_DIR}/avogadro2/i18n"
FILES_MATCHING PATTERN "qt_*.qm" PATTERN "qtbase*.qm"
# technically, this also copies qt_help.* but that's not a big deal
)
endif()

if(Avogadro_ENABLE_RPC)
  list(APPEND avogadro_srcs rpclistener.cpp)
  # MoleQueue is required for its RPC functions.
  find_package(MoleQueue REQUIRED)
  include_directories(${MoleQueue_INCLUDE_DIRS})
  add_definitions("-DAvogadro_ENABLE_RPC")
endif()

if(ENABLE_TESTING)
  find_package(Qt5 COMPONENTS Test REQUIRED)
  find_package(QtTesting REQUIRED NO_MODULE)
  include_directories(${QtTesting_INCLUDE_DIRS})
  link_directories(${QtTesting_LIBRARY_DIR})
  add_definitions(-DQTTESTING)
endif()

# Need to add current binary dir to include path for UI file headers
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# if we are building statically then we need HDF5 targets
if(NOT BUILD_SHARED_LIBS)
  find_package(HDF5 REQUIRED COMPONENTS C)
endif()

# Handle Mac OS X specific icons etc.
if(APPLE)
  # add some Objective-C++ code
  list(APPEND avogadro_srcs mac.mm ../thirdparty/qcocoacontrols/qcocoamessagebox.mm)
  include_directories(SYSTEM "${AvogadroApp_SOURCE_DIR}/thirdparty/qcocoacontrols")
  set(CMAKE_EXE_LINKER_FLAGS "-framework Foundation -framework Cocoa")

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

  list(APPEND avogadro_srcs icons/avogadro.icns)
  list(APPEND avogadro_srcs icons/doc.icns)
  list(APPEND avogadro_srcs icons/cml.icns)
  list(APPEND avogadro_srcs icons/cjson.icns)
  set(MACOSX_BUNDLE_ICON_FILE avogadro.icns)
  set(MACOSX_BUNDLE_BUNDLE_VERSION "${AvogadroApp_VERSION}")
  set(MACOSX_BUNDLE_INFO_STRING "Avogadro - version ${AvogadroApp_VERSION}")

  # Change following line to point to actual icns file in bundle.
  set(MACOSX_BUNDLE_GUI_IDENTIFIER "cc.avogadro")
  set(MACOSX_BUNDLE_BUNDLE_NAME "Avogadro")

  set_source_files_properties(icons/avogadro.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)
  set_source_files_properties(icons/doc.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)
  set_source_files_properties(icons/cjson.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)
  set_source_files_properties(icons/cml.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)

elseif(UNIX)
  install(FILES "icons/avogadro2.desktop" DESTINATION "${INSTALL_XDG_APPS_DIR}")
  install(FILES "icons/avogadro2.appdata.xml" DESTINATION "${CMAKE_INSTALL_PREFIX}/share/metainfo")
  install(FILES "icons/avogadro.png" DESTINATION "${INSTALL_XDG_ICON_DIR}"
    RENAME "avogadro2.png")
elseif(WIN32)
  list(APPEND avogadro_srcs icons/avogadro.rc)
endif()

add_executable(avogadro WIN32 MACOSX_BUNDLE
  ${avogadro_srcs} ${ui_srcs} ${rcc_srcs})
target_link_libraries(avogadro Qt::Widgets Qt::Network Qt::Concurrent)
if(WIN32)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:MSVCRTD")
  target_link_libraries(avogadro Qt::WinMain)
endif()
set_target_properties(avogadro PROPERTIES AUTOMOC TRUE)
target_link_libraries(avogadro AvogadroQtOpenGL AvogadroQtGui AvogadroQtPlugins)
if(Avogadro_ENABLE_RPC)
  target_link_libraries(avogadro MoleQueueServerCore MoleQueueClient)
endif()
if(USE_VTK)
  target_link_libraries(avogadro ${VTK_LIBRARIES} AvogadroVtk)
endif()
if(APPLE)
  set_target_properties(avogadro PROPERTIES OUTPUT_NAME ${MACOSX_BUNDLE_NAME})
else()
  set_target_properties(avogadro PROPERTIES OUTPUT_NAME "avogadro2")
endif()
if(ENABLE_TESTING)
  target_link_libraries(avogadro QtTesting)
endif()
if(QT_VERSION EQUAL 6)
  target_link_libraries(avogadro Qt6::Core5Compat)
endif()
install(TARGETS avogadro
  RUNTIME DESTINATION ${INSTALL_RUNTIME_DIR}
  BUNDLE DESTINATION .
  )

# Keep "add_subdirectory(lastinstall)" last: fixup_bundle needs to be
# *after* all other install(TARGETS and install(FILES calls
add_subdirectory(lastinstall)
