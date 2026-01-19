/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").

  Adapted from MoleQueue. Original copyright:
  Copyright 2012 Kitware, Inc.
******************************************************************************/

#ifndef AVOGADRO_RPC_JSONRPC_H
#define AVOGADRO_RPC_JSONRPC_H

#include "message.h"
#include "rpcglobal.h"

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QObject>

namespace Avogadro::RPC {

class Connection;
class ConnectionListener;

/**
 * @class JsonRpc jsonrpc.h <avogadro/rpc/jsonrpc.h>
 * @brief The JsonRpc class manages ConnectionListener and Connection instances,
 * and emits incoming JSON-RPC Messages.
 *
 * To use the JsonRpc class, create one or more ConnectionListener instances
 * and call addConnectionListener(). Connect a slot to messageReceived and
 * handle any incoming messages.
 *
 * Incoming requests with method="internalPing" will be automatically replied
 * to with result="pong". This can be used to test if a server is alive.
 */
class JsonRpc : public QObject
{
  Q_OBJECT
public:
  explicit JsonRpc(QObject* parent_ = nullptr);
  ~JsonRpc();

  /**
   * @brief Register a connection listener with this JsonRpc instance.
   * Any incoming connections on the listener will be monitored by this class.
   */
  void addConnectionListener(ConnectionListener* connlist);

  /**
   * @brief Unregister a connection listener from this JsonRpc instance.
   */
  void removeConnectionListener(ConnectionListener* connlist);

signals:
  /// Emitted when a valid message is received.
  void messageReceived(const Avogadro::RPC::Message& message);

private slots:
  void addConnection(Avogadro::RPC::Connection* conn);
  void removeConnection(Avogadro::RPC::ConnectionListener* connlist,
                        Avogadro::RPC::Connection* conn);
  void removeConnection(Avogadro::RPC::Connection* conn);
  void removeConnection();
  void removeConnectionListenerInternal();
  void newPacket(const Avogadro::RPC::PacketType& packet,
                 const Avogadro::RPC::EndpointIdType& endpoint);

private:
  void handleJsonValue(Connection* conn, const EndpointIdType& endpoint,
                       const QJsonValue& json);

  QMap<ConnectionListener*, QList<Connection*>> m_connections;
};

} // namespace Avogadro::RPC

#endif // AVOGADRO_RPC_JSONRPC_H
