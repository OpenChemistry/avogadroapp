find_package(AvogadroLibs REQUIRED NO_MODULE)
include_directories(${AvogadroLibs_INCLUDE_DIRS})
list(APPEND CMAKE_MODULE_PATH ${AvogadroLibs_CMAKE_DIR})
find_package(Eigen3 REQUIRED)
include_directories(SYSTEM ${EIGEN3_INCLUDE_DIR})

if(MSVC)
  add_definitions("-D_CRT_SECURE_NO_WARNINGS")
endif()

# Find the Qt components we need.
if(WIN32)
  set(qtmain QtMain)
else()
  unset(qtmain)
endif()
find_package(Qt4 4.7
  COMPONENTS QtCore QtGui QtOpenGL QtNetwork ${qtmain} REQUIRED)
include_directories(SYSTEM ${QT_INCLUDES})
add_definitions(${QT_DEFINITIONS})

configure_file(avogadroappconfig.h.in avogadroappconfig.h)

set(avogadro_srcs
  aboutdialog.cpp
  avogadro.cpp
  backgroundfileformat.cpp
  mainwindow.cpp
  menubuilder.cpp
)

qt4_wrap_ui(ui_srcs
  mainwindow.ui
  aboutdialog.ui
)

qt4_add_resources(rcc_srcs avogadro.qrc)

if(Avogadro_ENABLE_RPC)
  list(APPEND avogadro_srcs rpclistener.cpp)
  # MoleQueue is required for its RPC functions.
  find_package(MoleQueue REQUIRED)
  include_directories(${MoleQueue_INCLUDE_DIRS})
  add_definitions("-DAvogadro_ENABLE_RPC")
endif()

if(ENABLE_TESTING)
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
  list(APPEND avogadro_srcs icons/avogadro.icns)
  set(MACOSX_BUNDLE_ICON_FILE avogadro.icns)
  set(MACOSX_BUNDLE_BUNDLE_VERSION "${AvogadroApp_VERSION}")
  set_source_files_properties(icons/avogadro.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)
elseif(WIN32)
  list(APPEND avogadro_srcs icons/avogadro.rc)
endif()

add_executable(avogadro WIN32 MACOSX_BUNDLE
  ${avogadro_srcs} ${ui_srcs} ${rcc_srcs})
set_target_properties(avogadro PROPERTIES AUTOMOC TRUE)
target_link_libraries(avogadro
  AvogadroCore AvogadroIO AvogadroQtOpenGL AvogadroQtGui AvogadroQtPlugins
  ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTNETWORK_LIBRARY}
  ${QT_QTOPENGL_LIBRARY}
  )
if(Avogadro_ENABLE_RPC)
  target_link_libraries(avogadro
    MoleQueueServerCore
    MoleQueueClient
  )
endif()
if(APPLE)
  set_target_properties(avogadro PROPERTIES OUTPUT_NAME ${MACOSX_BUNDLE_NAME})
else()
  set_target_properties(avogadro PROPERTIES OUTPUT_NAME "avogadro2")
endif()
if(ENABLE_TESTING)
  target_link_libraries(avogadro QtTesting)
endif()
install(TARGETS avogadro
  RUNTIME DESTINATION ${INSTALL_RUNTIME_DIR}
  BUNDLE DESTINATION .
  )

# Keep "add_subdirectory(lastinstall)" last: fixup_bundle needs to be
# *after* all other install(TARGETS and install(FILES calls
add_subdirectory(lastinstall)
