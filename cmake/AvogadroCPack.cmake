set(CPACK_PACKAGE_NAME "Avogadro2")
set(CPACK_PACKAGE_VERSION_MAJOR ${AvogadroApp_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${AvogadroApp_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${AvogadroApp_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION ${AvogadroApp_VERSION})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Avogadro2")
set(CPACK_PACKAGE_VENDOR "http://openchemistry.org/")
set(CPACK_PACKAGE_DESCRIPTION
  "An advanced molecule editor and visualization application.")

if(APPLE)
  configure_file("${AvogadroApp_SOURCE_DIR}/COPYING"
    "${AvogadroApp_BINARY_DIR}/COPYING.txt" @ONLY)
  set(CPACK_RESOURCE_FILE_LICENSE "${AvogadroApp_BINARY_DIR}/COPYING.txt")
  set(CPACK_PACKAGE_ICON
    "${AvogadroApp_SOURCE_DIR}/avogadro/icons/avogadro.icns")
  set(CPACK_BUNDLE_ICON "${CPACK_PACKAGE_ICON}")
else()
  set(CPACK_RESOURCE_FILE_LICENSE "${AvogadroApp_SOURCE_DIR}/COPYING")
endif()

set(CPACK_PACKAGE_EXECUTABLES "avogadro" "Avogadro2")
set(CPACK_CREATE_DESKTOP_LINKS "avogadro")

configure_file("${CMAKE_CURRENT_LIST_DIR}/AvogadroCPackOptions.cmake.in"
  "${AvogadroApp_BINARY_DIR}/AvogadroCPackOptions.cmake" @ONLY)
set(CPACK_PROJECT_CONFIG_FILE
  "${AvogadroApp_BINARY_DIR}/AvogadroCPackOptions.cmake")

# Should we add extra install rules to make a self-contained bundle, this is
# usually only required when attempting to create self-contained installers.
option(INSTALL_BUNDLE_FILES "Add install rules to bundle files" OFF)
if(INSTALL_BUNDLE_FILES)
  # First the AvogadroLibs files that are not detected.
  find_package(AvogadroLibs REQUIRED NO_MODULE)
  install(DIRECTORY "${AvogadroLibs_LIBRARY_DIR}/avogadro2"
    DESTINATION ${INSTALL_LIBRARY_DIR})
endif()

include(CPack)
