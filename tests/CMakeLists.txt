find_package(AvogadroLibs REQUIRED NO_MODULE)
# The data repository should be at the side of ours.
find_path(AVOGADRO_DATA_ROOT .avogadro.data
  ${AvogadroApp_SOURCE_DIR}/../avogadrodata
  $ENV{AVOGADRO_DATA_ROOT}
  DOC "The repository for data used for testing."
  )
mark_as_advanced(AVOGADRO_DATA_ROOT)

if(AVOGADRO_DATA_ROOT)
  set(AVOGADRO_DATA ${AVOGADRO_DATA_ROOT})
else()
  message("No data root found, please set to run the tests.")
  return()
endif()

file(GLOB test_xml
  RELATIVE "${AVOGADRO_DATA}/tests/avogadro/xml"
  "${AVOGADRO_DATA}/tests/avogadro/xml/*.xml")

foreach(xml ${test_xml})
  get_filename_component(name "${xml}" NAME_WE)
  message("Adding test: ${name} from XML")
  add_test(NAME avogadro-${name}
    COMMAND avogadro
      --disable-settings
      --test-file "${AVOGADRO_DATA}/tests/avogadro/xml/${xml}")
  # Ensure we pass in an absolute path to the prefix so that we find the plugins
  set_tests_properties(avogadro-${name} PROPERTIES
    ENVIRONMENT "AVOGADRO_PLUGIN_DIR=${AvogadroLibs_INSTALL_PREFIX}")
endforeach()

# End-to-end tests: drive the built application over JSON-RPC (see rpc/README.md).
find_package(Python3 COMPONENTS Interpreter)
set(_avogadro_rpc_ready FALSE)
if(Python3_Interpreter_FOUND)
  execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "import pytest"
    RESULT_VARIABLE _pytest_missing
    OUTPUT_QUIET ERROR_QUIET)
  if(_pytest_missing EQUAL 0)
    set(_avogadro_rpc_ready TRUE)
  else()
    message(WARNING "pytest is not importable by ${Python3_EXECUTABLE}; "
      "not adding the avogadro-rpc tests.")
  endif()
else()
  message(WARNING "Python 3 was not found; not adding the avogadro-rpc tests.")
endif()

if(_avogadro_rpc_ready)
  # One test per pytest marker expression.
  function(add_avogadro_rpc_test name marker labels timeout)
    add_test(NAME ${name}
      COMMAND ${Python3_EXECUTABLE} -m pytest ${CMAKE_CURRENT_SOURCE_DIR}/rpc -q
        --avogadro $<TARGET_FILE:avogadro>
        --reproducer-dir ${CMAKE_CURRENT_BINARY_DIR}/rpc-reproducers
        -m "${marker}")
    # Ensure we pass in an absolute path to the prefix so that we find the
    # plugins
    set_tests_properties(${name} PROPERTIES
      ENVIRONMENT "AVOGADRO_DATA_ROOT=${AVOGADRO_DATA};AVOGADRO_PLUGIN_DIR=${AvogadroLibs_INSTALL_PREFIX}"
      LABELS "${labels}"
      TIMEOUT ${timeout})
    # The plugins look for obabel and obmm next to the executable, then on
    # PATH. Neither is next to the build-tree avogadro2, so put the prefix's
    # bin first; otherwise CI has no Open Babel and a developer machine uses
    # whatever obabel happens to be installed.
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.22)
      set_property(TEST ${name} PROPERTY ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${AvogadroLibs_INSTALL_PREFIX}/bin")
    endif()
  endfunction()

  add_avogadro_rpc_test(avogadro-rpc "not corpus" "rpc" 900)
  # Opens every sample file; slow, and a file that blocks the application
  # costs its timeout, so it is given a generous limit.
  add_avogadro_rpc_test(avogadro-rpc-corpus "corpus" "rpc;corpus" 7200)
endif()
