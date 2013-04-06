set(CPACK_PACKAGE_NAME "Avogadro2")
set(CPACK_PACKAGE_VERSION_MAJOR ${AvogadroApp_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${AvogadroApp_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${AvogadroApp_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION ${AvogadroApp_VERSION})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Avogadro2")

if(APPLE)
  configure_file("${AvogadroApp_SOURCE_DIR}/COPYING"
    "${AvogadroApp_BINARY_DIR}/COPYING.txt" @ONLY)
  set(CPACK_RESOURCE_FILE_LICENSE "${AvogadroApp_BINARY_DIR}/COPYING.txt")
else()
  set(CPACK_RESOURCE_FILE_LICENSE "${AvogadroApp_SOURCE_DIR}/COPYING")
endif()

set(CPACK_PACKAGE_EXECUTABLES "avogadro" "Avogadro2")
set(CPACK_CREATE_DESKTOP_LINKS "avogadro")

configure_file("${CMAKE_CURRENT_LIST_DIR}/AvogadroCPackOptions.cmake.in"
  "${AvogadroApp_BINARY_DIR}/AvogadroCPackOptions.cmake" @ONLY)
set(CPACK_PROJECT_CONFIG_FILE
  "${AvogadroApp_BINARY_DIR}/AvogadroCPackOptions.cmake")

include(CPack)
