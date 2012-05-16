find_package(AvogadroLibs REQUIRED)
include_directories(${AvogadroLibs_INCLUDE_DIRS})
list(APPEND CMAKE_MODULE_PATH ${AvogadroLibs_LIBRARY_DIRS}/cmake/avogadrolibs)
find_package(Eigen3 REQUIRED)
include_directories(SYSTEM ${EIGEN3_INCLUDE_DIRS})

find_package(Boost
  COMPONENTS thread filesystem program_options system iostreams)
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
target_link_libraries(avogadro ${Boost_LIBRARIES} AvogadroCore AvogadroIO
  ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTNETWORK_LIBRARY}
  ${QT_QTOPENGL_LIBRARY}
  )
install(TARGETS avogadro DESTINATION "${INSTALL_RUNTIME_DIR}")
