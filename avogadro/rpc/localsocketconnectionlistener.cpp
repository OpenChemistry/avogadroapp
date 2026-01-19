/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").

  Adapted from MoleQueue. Original copyright:
  Copyright 2012 Kitware, Inc.
******************************************************************************/

#include "localsocketconnectionlistener.h"
#include "localsocketconnection.h"

#include <QtCore/QDebug>
#include <QtNetwork/QLocalServer>
#include <QtNetwork/QLocalSocket>

namespace Avogadro::RPC {

LocalSocketConnectionListener::LocalSocketConnectionListener(
  QObject* parentObject, const QString& connString)
  : ConnectionListener(parentObject)
  , m_connectionString(connString)
  , m_server(new QLocalServer())
{
  connect(m_server, SIGNAL(newConnection()), this,
          SLOT(newConnectionAvailable()));
}

LocalSocketConnectionListener::~LocalSocketConnectionListener()
{
  // Make sure we are stopped
  stop();

  delete m_server;
  m_server = nullptr;
}

void LocalSocketConnectionListener::start()
{
  if (!m_server->listen(m_connectionString)) {
    emit connectionError(toConnectionListenerError(m_server->serverError()),
                         m_server->errorString());
    return;
  }
  qDebug() << "RPC server listening on:" << m_server->fullServerName();
}

void LocalSocketConnectionListener::stop(bool force)
{
  if (force)
    QLocalServer::removeServer(m_connectionString);

  if (m_server)
    m_server->close();
}

void LocalSocketConnectionListener::stop()
{
  stop(false);
}

QString LocalSocketConnectionListener::connectionString() const
{
  return m_connectionString;
}

QString LocalSocketConnectionListener::fullConnectionString() const
{
  return m_server->fullServerName();
}

void LocalSocketConnectionListener::newConnectionAvailable()
{
  if (!m_server->hasPendingConnections())
    return;

  QLocalSocket* socket = m_server->nextPendingConnection();

  LocalSocketConnection* conn = new LocalSocketConnection(this, socket);

  emit newConnection(conn);
}

ConnectionListener::Error
LocalSocketConnectionListener::toConnectionListenerError(
  QAbstractSocket::SocketError socketError)
{
  ConnectionListener::Error listenerError = UnknownError;

  switch (socketError) {
    case QAbstractSocket::AddressInUseError:
      listenerError = ConnectionListener::AddressInUseError;
      break;
    default:
      // UnknownError
      break;
  }

  return listenerError;
}

} // namespace Avogadro::RPC
