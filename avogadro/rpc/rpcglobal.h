/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").

  Adapted from MoleQueue. Original copyright:
  Copyright 2013 Kitware, Inc.
******************************************************************************/

#ifndef AVOGADRO_RPC_GLOBAL_H
#define AVOGADRO_RPC_GLOBAL_H

#include <QtCore/QByteArray>
#include <QtCore/QJsonValue>

namespace Avogadro::RPC {

/// Type for Endpoint identifiers
typedef QByteArray EndpointIdType;

/// Type for Message identifiers (JSON-RPC ids)
typedef QJsonValue MessageIdType;

/// Type for RPC packets
typedef QByteArray PacketType;

} // namespace Avogadro::RPC

#endif // AVOGADRO_RPC_GLOBAL_H
