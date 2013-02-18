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
endforeach()
