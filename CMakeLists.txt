cmake_minimum_required(VERSION 2.8.6 FATAL_ERROR)

project(AvogadroApp)

set(CMAKE_MODULE_PATH ${AvogadroApp_SOURCE_DIR}/cmake)

include(BuildType)
include(BuildLocation)
include(CompilerFlags)
include(InstallLocation)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

option(ENABLE_RPATH "Enable rpath support on Linux and Mac" ON)
if(UNIX AND ENABLE_RPATH)
  if(APPLE)
    set(CMAKE_INSTALL_NAME_DIR "@executable_path/../${INSTALL_LIBRARY_DIR}")
  endif()
  set(CMAKE_SKIP_BUILD_RPATH FALSE)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

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

include(CPack)

if(ENABLE_TESTING)
  include(BuildPackageTest)
  BuildPackageTest_Add("AvogadroApp" "${CMAKE_CURRENT_BINARY_DIR}")
endif()
