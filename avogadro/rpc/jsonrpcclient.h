/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").

  Adapted from MoleQueue. Original copyright:
  Copyright 2012-2013 Kitware, Inc.
******************************************************************************/

#ifndef AVOGADRO_RPC_JSONRPCCLIENT_H
#define AVOGADRO_RPC_JSONRPCCLIENT_H

#include <QtCore/QJsonObject>
#include <QtCore/QObject>

class QLocalSocket;

namespace Avogadro::RPC {

/**
 * @class JsonRpcClient jsonrpcclient.h <avogadro/rpc/jsonrpcclient.h>
 * @brief The JsonRpcClient class is used by clients to submit calls to an RPC
 * server using JSON-RPC 2.0.
 *
 * Provides a simple Qt C++ API to make JSON-RPC 2.0 calls to an RPC server.
 */
class JsonRpcClient : public QObject
{
  Q_OBJECT

public:
  explicit JsonRpcClient(QObject* parent_ = nullptr);
  ~JsonRpcClient();

  /// Query if the client is connected to a server.
  bool isConnected() const;

  /// @return The server name that the client is connected to.
  QString serverName() const;

public slots:
  /// Connect to the server.
  bool connectToServer(const QString& serverName);

  /// Flush all pending messages to the server.
  void flush();

  /// Construct an empty JSON-RPC 2.0 request with a valid request id.
  QJsonObject emptyRequest();

  /// Send the Json request to the RPC server.
  bool sendRequest(const QJsonObject& request);

protected slots:
  void readPacket(const QByteArray message);
  void readSocket();

signals:
  void connectionStateChanged();
  void resultReceived(QJsonObject message);
  void notificationReceived(QJsonObject message);
  void errorReceived(QJsonObject message);
  void badPacketReceived(QString error);
  void newPacket(const QByteArray& packet);

protected:
  unsigned int m_packetCounter;
  QLocalSocket* m_socket;
};

} // namespace Avogadro::RPC

#endif // AVOGADRO_RPC_JSONRPCCLIENT_H
