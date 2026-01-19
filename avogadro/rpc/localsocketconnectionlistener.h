/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").

  Adapted from MoleQueue. Original copyright:
  Copyright 2012 Kitware, Inc.
******************************************************************************/

#ifndef AVOGADRO_RPC_LOCALSOCKETCONNECTIONLISTENER_H
#define AVOGADRO_RPC_LOCALSOCKETCONNECTIONLISTENER_H

#include "connectionlistener.h"

#include <QtNetwork/QAbstractSocket>

class QLocalServer;

namespace Avogadro::RPC {

/**
 * @class LocalSocketConnectionListener localsocketconnectionlistener.h
 * <avogadro/rpc/localsocketconnectionlistener.h>
 * @brief Provides an implementation of ConnectionListener using QLocalServer.
 * Each connection made is emitted as a LocalSocketConnection.
 */
class LocalSocketConnectionListener : public ConnectionListener
{
  Q_OBJECT
public:
  /**
   * Constructor.
   * @param parentObject parent
   * @param connectionString The address that the QLocalServer should listen on.
   */
  explicit LocalSocketConnectionListener(QObject* parentObject,
                                         const QString& connectionString);

  ~LocalSocketConnectionListener();

  /// Start listening for incoming connections.
  void start() override;

  /**
   * Stops the connection listener.
   * @param force If true use QLocalServer::removeServer(...) to remove server.
   */
  void stop(bool force) override;

  /// Calls stop(false)
  void stop() override;

  /// @return the address the QLocalServer is listening on.
  QString connectionString() const override;

  /// @return the full address the QLocalServer is listening on.
  QString fullConnectionString() const;

private slots:
  void newConnectionAvailable();

private:
  ConnectionListener::Error toConnectionListenerError(
    QAbstractSocket::SocketError error);

  QString m_connectionString;
  QLocalServer* m_server;
};

} // namespace Avogadro::RPC

#endif // AVOGADRO_RPC_LOCALSOCKETCONNECTIONLISTENER_H
