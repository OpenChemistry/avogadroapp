find_package(AvogadroLibs REQUIRED NO_MODULE)
find_package(Eigen3 REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets)
if(NOT AvogadroLibs_WEB_EDITOR OR AvogadroLibs_ENABLE_SUBPROCESS)
  message(FATAL_ERROR "Build avogadrolibs with AVOGADRO_WEB_EDITOR=ON and ENABLE_SUBPROCESS=OFF")
endif()

add_library(AvogadroWebController STATIC webcontroller.cpp webcontroller.h)
set_target_properties(AvogadroWebController PROPERTIES AUTOMOC ON)
target_link_libraries(AvogadroWebController PUBLIC Avogadro::QtGui Avogadro::QtOpenGL Avogadro::QtPlugins Eigen3::Eigen)
target_include_directories(AvogadroWebController PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
qt_add_executable(avogadro-web main.cpp)
target_link_libraries(avogadro-web PRIVATE AvogadroWebController)

if(EMSCRIPTEN)
  target_link_options(avogadro-web PRIVATE --bind -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2)
  set_target_properties(avogadro-web PROPERTIES QT_WASM_EXPORT_NAME createAvogadroWeb)
  # qt_add_executable deploys the loader and Emscripten auxiliary files.
  foreach(asset index.html editor.js editor.css serve-wasm.py)
    configure_file(${asset} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${asset} COPYONLY)
  endforeach()
  install(FILES README.md VALIDATION.md DESTINATION share/avogadro-web)
  find_program(AVOGADRO_GZIP_EXECUTABLE gzip)
  if(AVOGADRO_GZIP_EXECUTABLE)
    add_custom_command(TARGET avogadro-web POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE_DIR:avogadro-web>/avogadro-web.wasm" "$<TARGET_FILE_DIR:avogadro-web>/avogadro-web.wasm.compress"
      COMMAND ${AVOGADRO_GZIP_EXECUTABLE} -9 -f "$<TARGET_FILE_DIR:avogadro-web>/avogadro-web.wasm.compress"
      COMMAND ${CMAKE_COMMAND} -E rename "$<TARGET_FILE_DIR:avogadro-web>/avogadro-web.wasm.compress.gz" "$<TARGET_FILE_DIR:avogadro-web>/avogadro-web.wasm.gz"
      VERBATIM)
  endif()
  install(DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ DESTINATION share/avogadro-web
    FILES_MATCHING PATTERN "*.js" PATTERN "*.wasm" PATTERN "*.wasm.gz"
    PATTERN "*.data" PATTERN "index.html" PATTERN "*.css" PATTERN "serve-wasm.py")
else()
  install(TARGETS avogadro-web RUNTIME DESTINATION ${INSTALL_RUNTIME_DIR})
endif()
if(ENABLE_TESTING AND NOT EMSCRIPTEN)
  find_package(GTest REQUIRED)
  find_package(Qt6 REQUIRED COMPONENTS Test)
  add_executable(AvogadroWebTests tests.cpp)
  target_link_libraries(AvogadroWebTests PRIVATE AvogadroWebController GTest::gtest Qt6::Test)
  add_test(NAME WebEditor COMMAND AvogadroWebTests)
  set_tests_properties(WebEditor PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
