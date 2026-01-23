/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").

  Adapted from MoleQueue. Original copyright:
  Copyright 2012 Kitware, Inc.
******************************************************************************/

#ifndef AVOGADRO_RPC_CONNECTION_H
#define AVOGADRO_RPC_CONNECTION_H

#include "rpcglobal.h"

#include <QtCore/QObject>

namespace Avogadro::RPC {

/**
 * @class Connection connection.h <avogadro/rpc/connection.h>
 * @brief The Connection class is an interface defining the connection used to
 * communicate between processes. Subclasses provide concrete implementations,
 * for example based on local sockets @see LocalSocketConnection
 */
class Connection : public QObject
{
  Q_OBJECT
public:
  explicit Connection(QObject* parentObject = nullptr)
    : QObject(parentObject)
  {
  }

  /// Open the connection
  virtual void open() = 0;

  /// Start receiving messages on this connection
  virtual void start() = 0;

  /// Close the connection. Once a connection is closed it can't be reused.
  virtual void close() = 0;

  /// @return true if the connection is open, false otherwise
  virtual bool isOpen() = 0;

  /// @return the connection string describing the endpoint
  virtual QString connectionString() const = 0;

  /// Send the @a packet on the connection to @a endpoint.
  virtual bool send(const PacketType& packet,
                    const EndpointIdType& endpoint) = 0;

  /// Flush all pending messages to the other endpoint.
  virtual void flush() = 0;

signals:
  /// Emitted when a new message has been received on this connection.
  void packetReceived(const Avogadro::RPC::PacketType& packet,
                      const Avogadro::RPC::EndpointIdType& endpoint);

  /// Emitted when the connection is disconnected.
  void disconnected();
};

} // namespace Avogadro::RPC

#endif // AVOGADRO_RPC_CONNECTION_H
