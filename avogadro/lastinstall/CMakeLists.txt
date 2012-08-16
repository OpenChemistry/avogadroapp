if((APPLE OR WIN32) AND NOT ${CMAKE_VERSION} VERSION_LESS 2.8.8)
  set(pfx "")
  if(NOT APPLE)
    set(pfx "bin/")
  endif()

  set(sfx "")
  if(APPLE)
    set(sfx ".app")
  elseif(WIN32)
    set(sfx ".exe")
  endif()

  set(exe "${pfx}avogadro${sfx}")

  set(dirs "")
  if(CMAKE_PREFIX_PATH)
    set(dirs "${CMAKE_PREFIX_PATH}/bin" "${CMAKE_PREFIX_PATH}/lib")
  endif()

  include(DeployQt4)
  install_qt4_executable(${exe} "" "" "${dirs}")
endif()
