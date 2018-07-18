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
  configure_file("${AvogadroApp_SOURCE_DIR}/LICENSE"
    "${AvogadroApp_BINARY_DIR}/COPYING.txt" @ONLY)
  set(CPACK_RESOURCE_FILE_LICENSE "${AvogadroApp_BINARY_DIR}/COPYING.txt")
  set(CPACK_PACKAGE_ICON
    "${AvogadroApp_SOURCE_DIR}/avogadro/icons/avogadro.icns")
  set(CPACK_BUNDLE_ICON "${CPACK_PACKAGE_ICON}")
else()
  set(CPACK_RESOURCE_FILE_LICENSE "${AvogadroApp_SOURCE_DIR}/LICENSE")
endif()

set(CPACK_PACKAGE_EXECUTABLES "avogadro2" "Avogadro2")
set(CPACK_CREATE_DESKTOP_LINKS "avogadro2")

configure_file("${CMAKE_CURRENT_LIST_DIR}/AvogadroCPackOptions.cmake.in"
  "${AvogadroApp_BINARY_DIR}/AvogadroCPackOptions.cmake" @ONLY)
set(CPACK_PROJECT_CONFIG_FILE
  "${AvogadroApp_BINARY_DIR}/AvogadroCPackOptions.cmake")

# Should we add extra install rules to make a self-contained bundle, this is
# usually only required when attempting to create self-contained installers.
if(APPLE)
  set(INSTALL_BUNDLE_FILES ON)
else()
  option(INSTALL_BUNDLE_FILES "Add install rules to bundle files" OFF)
endif()
if(INSTALL_BUNDLE_FILES)
  # First the AvogadroLibs files that are not detected.
  find_package(AvogadroLibs REQUIRED NO_MODULE)
  install(DIRECTORY "${AvogadroLibs_LIBRARY_DIR}/avogadro2"
    DESTINATION ${INSTALL_LIBRARY_DIR})

  find_program(OBABEL_EXE obabel)
  if(OBABEL_EXE)
    install(FILES ${OBABEL_EXE} DESTINATION ${INSTALL_RUNTIME_DIR}
      PERMISSIONS
        OWNER_READ OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE)
    get_filename_component(BABEL_DIR "${OBABEL_EXE}" PATH)
    if(WIN32)
      file(GLOB BABEL_PLUGINS ${BABEL_DIR}/*.obf)
      install(
        FILES
          ${BABEL_PLUGINS}
          ${BABEL_DIR}/inchi.dll
          ${BABEL_DIR}/libxml2.dll
        DESTINATION ${INSTALL_RUNTIME_DIR})
      install(DIRECTORY "${BABEL_DIR}/data"
        DESTINATION ${INSTALL_RUNTIME_DIR})
    elseif(APPLE)
      install(DIRECTORY "${BABEL_DIR}/../lib/openbabel"
        DESTINATION ${INSTALL_LIBRARY_DIR})
      install(DIRECTORY "${BABEL_DIR}/../share/openbabel"
        DESTINATION ${INSTALL_DATA_DIR})
    endif()
    install(FILES ${AvogadroApp_SOURCE_DIR}/cmake/COPYING.openbabel
      DESTINATION ${INSTALL_DOC_DIR}/openbabel)
    file(READ "${AvogadroApp_SOURCE_DIR}/cmake/COPYING.openbabel" ob_license)
    file(READ "${AvogadroApp_SOURCE_DIR}/LICENSE" avo_license)
    file(WRITE "${AvogadroApp_BINARY_DIR}/COPYING.txt"
      "${avo_license}\n\nOpen Babel components licensed under GPLv2\n\n"
      "${ob_license}")
    set(CPACK_RESOURCE_FILE_LICENSE "${AvogadroApp_BINARY_DIR}/COPYING.txt")
  endif()
endif()

include(CPack)
