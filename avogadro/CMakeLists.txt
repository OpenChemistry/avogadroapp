find_package(AvogadroLibs REQUIRED NO_MODULE)
include_directories(${AvogadroLibs_INCLUDE_DIRS})
list(APPEND CMAKE_MODULE_PATH ${AvogadroLibs_CMAKE_DIR})
find_package(Eigen3 REQUIRED)
include_directories(SYSTEM ${EIGEN3_INCLUDE_DIR})

option(USE_VTK "Build with VTK support" OFF)
if(USE_VTK)
  find_package(VTK COMPONENTS vtkGUISupportQt vtkRenderingOpenGL REQUIRED)
  include(${VTK_USE_FILE})
  add_definitions(-DAVO_USE_VTK)
endif()

if(MSVC)
  add_definitions("-D_CRT_SECURE_NO_WARNINGS")
endif()

# Find the Qt components we need.
find_package(Qt5Widgets REQUIRED)
find_package(Qt5OpenGL REQUIRED)
find_package(Qt5Network REQUIRED)

configure_file(avogadroappconfig.h.in avogadroappconfig.h)

set(avogadro_srcs
  aboutdialog.cpp
  avogadro.cpp
  backgroundfileformat.cpp
  mainwindow.cpp
  menubuilder.cpp
  viewfactory.cpp
)

qt5_wrap_ui(ui_srcs
  aboutdialog.ui
)

qt5_add_resources(rcc_srcs avogadro.qrc)

if(Avogadro_ENABLE_RPC)
  list(APPEND avogadro_srcs rpclistener.cpp)
  # MoleQueue is required for its RPC functions.
  find_package(MoleQueue REQUIRED)
  include_directories(${MoleQueue_INCLUDE_DIRS})
  add_definitions("-DAvogadro_ENABLE_RPC")
endif()

if(ENABLE_TESTING)
  find_package(Qt5Test REQUIRED)
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
  set(MACOSX_BUNDLE_INFO_STRING "Avogadro - version ${AvogadroApp_VERSION}")

  # Change following line to point to actual icns file in bundle.
  set(MACOSX_BUNDLE_GUI_IDENTIFIER "cc.avogadro")
  set(MACOSX_BUNDLE_BUNDLE_NAME "Avogadro")

  set_source_files_properties(icons/avogadro.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)

elseif(UNIX)
  install(FILES "icons/avogadro2.desktop" DESTINATION "${INSTALL_XDG_APPS_DIR}")
  install(FILES "icons/avogadro.png" DESTINATION "${INSTALL_XDG_ICON_DIR}"
    RENAME "avogadro2.png")
elseif(WIN32)
  list(APPEND avogadro_srcs icons/avogadro.rc)
endif()

add_executable(avogadro WIN32 MACOSX_BUNDLE
  ${avogadro_srcs} ${ui_srcs} ${rcc_srcs})
qt5_use_modules(avogadro Widgets OpenGL Network Concurrent)
if(WIN32)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:MSVCRTD")
  target_link_libraries(avogadro Qt5::WinMain)
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
  find_package(Qt5Test REQUIRED)
  target_link_libraries(avogadro QtTesting)
endif()
install(TARGETS avogadro
  RUNTIME DESTINATION ${INSTALL_RUNTIME_DIR}
  BUNDLE DESTINATION .
  )

# Keep "add_subdirectory(lastinstall)" last: fixup_bundle needs to be
# *after* all other install(TARGETS and install(FILES calls
add_subdirectory(lastinstall)
