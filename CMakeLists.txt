cmake_minimum_required(VERSION 2.8.6 FATAL_ERROR)

project(AvogadroApp)

set(CMAKE_MODULE_PATH ${AvogadroApp_SOURCE_DIR}/cmake)

include(BuildType)
include(BuildLocation)
include(CompilerFlags)
include(InstallLocation)
include(DetermineVersion)

# Set up our version.
set(AvogadroApp_VERSION_MAJOR   "0")
set(AvogadroApp_VERSION_MINOR   "1")
set(AvogadroApp_VERSION_PATCH   "0")
set(AvogadroApp_VERSION
  "${AvogadroApp_VERSION_MAJOR}.${AvogadroApp_VERSION_MINOR}.${AvogadroApp_VERSION_PATCH}")
find_package(Git)
determine_version(${AvogadroApp_SOURCE_DIR} ${GIT_EXECUTABLE} "AvogadroApp")

if(APPLE)
  set(MACOSX_BUNDLE_NAME "Avogadro2")
  set(prefix "${MACOSX_BUNDLE_NAME}.app/Contents")
  set(INSTALL_INCLUDE_DIR "${prefix}/${INSTALL_INCLUDE_DIR}")
  set(INSTALL_LIBRARY_DIR "${prefix}/${INSTALL_LIBRARY_DIR}")
  set(INSTALL_ARCHIVE_DIR "${prefix}/${INSTALL_ARCHIVE_DIR}")
  set(INSTALL_CMAKE_DIR   "${prefix}/Resources")
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

option(ENABLE_TESTING "Enable testing and building the tests." OFF)

if(ENABLE_TESTING)
  include(CTest)
  enable_testing()
endif()

option(Avogadro_ENABLE_RPC "Enable RPC server" ON)

add_subdirectory(avogadro)

option(BUILD_DOCUMENTATION "Build project documentation" OFF)

if(BUILD_DOCUMENTATION)
  add_subdirectory(docs)
endif()

install(
  FILES
    README.md
    CONTRIBUTING.md
    COPYING
  DESTINATION "${INSTALL_DOC_DIR}/avogadro2")

include(AvogadroCPack)

if(ENABLE_TESTING)
  include(BuildPackageTest)
  BuildPackageTest_Add("AvogadroApp" "${CMAKE_CURRENT_BINARY_DIR}")
  add_subdirectory(tests)
endif()
