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
  set(_rpc_common
    ${Python3_EXECUTABLE} -m pytest ${CMAKE_CURRENT_SOURCE_DIR}/rpc -q
      --avogadro $<TARGET_FILE:avogadro>
      --reproducer-dir ${CMAKE_CURRENT_BINARY_DIR}/rpc-reproducers)
  # Ensure we pass in an absolute path to the prefix so that we find the plugins
  set(_rpc_environment
    "AVOGADRO_DATA_ROOT=${AVOGADRO_DATA}"
    "AVOGADRO_PLUGIN_DIR=${AvogadroLibs_INSTALL_PREFIX}")

  add_test(NAME avogadro-rpc COMMAND ${_rpc_common} -m "not corpus")
  set_tests_properties(avogadro-rpc PROPERTIES
    ENVIRONMENT "${_rpc_environment}"
    LABELS "rpc"
    TIMEOUT 900)

  # Opens every sample file; slow, and a file that blocks the application
  # costs its timeout, so it is given a generous limit.
  add_test(NAME avogadro-rpc-corpus COMMAND ${_rpc_common} -m corpus)
  set_tests_properties(avogadro-rpc-corpus PROPERTIES
    ENVIRONMENT "${_rpc_environment}"
    LABELS "rpc;corpus"
    TIMEOUT 7200)
endif()
