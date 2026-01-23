/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").

  Adapted from MoleQueue. Original copyright:
  Copyright 2012 Kitware, Inc.
******************************************************************************/

#ifndef AVOGADRO_RPC_LOCALSOCKETCONNECTION_H
#define AVOGADRO_RPC_LOCALSOCKETCONNECTION_H

#include "connection.h"

class QLocalSocket;
class QDataStream;

namespace Avogadro::RPC {

/**
 * @class LocalSocketConnection localsocketconnection.h
 * <avogadro/rpc/localsocketconnection.h>
 * @brief Provides an implementation of Connection using QLocalSockets.
 * Each instance of the class wraps a QLocalSocket.
 */
class LocalSocketConnection : public Connection
{
  Q_OBJECT
public:
  /**
   * Constructor used by LocalSocketConnectionListener to create a new
   * connection based on an existing QLocalSocket.
   */
  explicit LocalSocketConnection(QObject* parentObject, QLocalSocket* socket);

  /**
   * Constructor used by a client to connect to a server.
   */
  explicit LocalSocketConnection(QObject* parentObject,
                                 const QString& connectionString);

  ~LocalSocketConnection();

  /// Opens the connection to the server
  void open() override;

  /// Start receiving messages on this connection
  void start() override;

  /// Close the underlying socket
  void close() override;

  /// @return true if connection is open, false otherwise
  bool isOpen() override;

  /// @return The serverName from the underlying socket
  QString connectionString() const override;

  bool send(const PacketType& packet, const EndpointIdType& endpoint) override;

  void flush() override;

private slots:
  void readSocket();
  void socketDestroyed();

private:
  void setSocket(QLocalSocket* socket);

  QString m_connectionString;
  QLocalSocket* m_socket;
  QDataStream* m_dataStream;
  bool m_holdRequests;
};

} // namespace Avogadro::RPC

#endif // AVOGADRO_RPC_LOCALSOCKETCONNECTION_H
