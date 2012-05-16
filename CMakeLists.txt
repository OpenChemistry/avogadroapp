cmake_minimum_required(VERSION 2.8)

project(AvogadroApp)

set(CMAKE_MODULE_PATH ${AvogadroApp_SOURCE_DIR}/cmake)

include(BuildType)
include(BuildLocation)
include(CompilerFlags)
include(InstallLocation)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

option(ENABLE_TESTING "Enable testing and building the tests." OFF)

if(ENABLE_TESTING)
  include(CTest)
  enable_testing()
endif()

add_subdirectory(avogadro)

option(BUILD_DOCUMENTATION "Build project documentation" OFF)

if(BUILD_DOCUMENTATION)
  add_subdirectory(docs)
endif()
