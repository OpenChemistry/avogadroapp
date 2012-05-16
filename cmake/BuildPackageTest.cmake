get_filename_component(_BuildPackageTest_self_dir
  "${CMAKE_CURRENT_LIST_FILE}" PATH)


if("$ENV{DASHBOARD_TEST_FROM_CTEST}" STREQUAL "")
  # Not a dashboard, do not add BuildPackage* tests by default:
  set(BUILD_PACKAGE_TEST_DEFAULT OFF)
else()
  # Dashboard, do add BuildPackage* tests by default:
  set(BUILD_PACKAGE_TEST_DEFAULT ON)
endif()


option(BUILD_PACKAGE_TEST "Add BuildPackage* tests..."
  ${BUILD_PACKAGE_TEST_DEFAULT})


function(BuildPackageTest_Add projname binary_dir)
  if (NOT BUILD_PACKAGE_TEST)
    return()
  endif()

  # Use the NAME/COMMAND form of add_test and pass $<CONFIGURATION>.
  # However, using this form requires passing -C when running ctest
  # from the command line, or setting CTEST_CONFIGURATION_TYPE
  # in a -S script.

  configure_file(
    ${_BuildPackageTest_self_dir}/BuildPackage.cmake.in
    ${binary_dir}/BuildPackage${projname}.cmake
    @ONLY
    )

  add_test(
    NAME BuildPackage${projname}
    COMMAND ${CMAKE_COMMAND}
      -D config=$<CONFIGURATION>
      -P ${binary_dir}/BuildPackage${projname}.cmake
    )
endfunction()
