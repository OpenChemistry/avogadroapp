find_package(AvogadroLibs REQUIRED)
include_directories(${AvogadroLibs_INCLUDE_DIRS})
message("CMAKE:DIR: ${AvogadroLibs_CMAKE_DIR}")
list(APPEND CMAKE_MODULE_PATH ${AvogadroLibs_CMAKE_DIR})
find_package(Eigen3 REQUIRED)
include_directories(SYSTEM ${EIGEN3_INCLUDE_DIR})

if(WIN32)
  if(MSVC)
    add_definitions("-D_CRT_SECURE_NO_WARNINGS")
  endif()
  set(Boost_USE_STATIC_LIBS ON)
endif()
find_package(Boost
  COMPONENTS thread chrono filesystem program_options system iostreams)
if(WIN32)
  add_definitions("-DBOOST_ALL_NO_LIB")
endif()
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})

# Find the Qt components we need.
find_package(Qt4 4.7 COMPONENTS QtCore QtGui QtOpenGL QtNetwork REQUIRED)
include_directories(SYSTEM ${QT_INCLUDES})
add_definitions(${QT_DEFINITIONS})

set(avogadro_srcs
  avogadro.cpp
  mainwindow.cpp
)

qt4_wrap_cpp(moc_srcs
  mainwindow.h
)

#qt4_wrap_ui(ui_srcs
#  mainwindow.ui
#)

# Need to add current binary dir to include path for UI file headers
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(avogadro MACOSX_BUNDLE ${avogadro_srcs} ${moc_srcs} ${ui_srcs})
target_link_libraries(avogadro ${Boost_LIBRARIES}
  AvogadroCore AvogadroIO AvogadroQtOpenGL AvogadroQtGui
  ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTNETWORK_LIBRARY}
  ${QT_QTOPENGL_LIBRARY}
  )
install(TARGETS avogadro
  RUNTIME DESTINATION ${INSTALL_RUNTIME_DIR}
  BUNDLE DESTINATION .
  )


# Keep "add_subdirectory(lastinstall)" last: fixup_bundle needs to be
# *after* all other install(TARGETS and install(FILES calls
add_subdirectory(lastinstall)
