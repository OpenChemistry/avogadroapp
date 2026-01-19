# This source file is part of the Avogadro project.
# This source code is released under the 3-Clause BSD License, (see "LICENSE").

set(rpc_srcs
  connectionlistener.cpp
  jsonrpc.cpp
  jsonrpcclient.cpp
  localsocketconnection.cpp
  localsocketconnectionlistener.cpp
  message.cpp
  messageidmanager_p.cpp
)

set(rpc_headers
  connection.h
  connectionlistener.h
  jsonrpc.h
  jsonrpcclient.h
  localsocketconnection.h
  localsocketconnectionlistener.h
  message.h
  messageidmanager_p.h
  rpcglobal.h
)

add_library(AvogadroRPC STATIC ${rpc_srcs} ${rpc_headers})
target_link_libraries(AvogadroRPC Qt::Core Qt::Network)
set_target_properties(AvogadroRPC PROPERTIES AUTOMOC TRUE)
target_include_directories(AvogadroRPC PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
