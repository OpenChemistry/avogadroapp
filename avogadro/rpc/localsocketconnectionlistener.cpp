/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").

  Adapted from MoleQueue. Original copyright:
  Copyright 2012 Kitware, Inc.
******************************************************************************/

#include "localsocketconnectionlistener.h"
#include "localsocketconnection.h"

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
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
#if defined(Q_OS_UNIX)
  // On Unix, QLocalServer creates a socket file in /tmp. If that path already
  // exists but is not a socket (e.g. someone created /tmp/avogadro as a
  // directory), listen() will fail with AddressInUseError. That would trigger
  // the ping-and-retry logic, which tries to connect to a directory and
  // crashes. Detect this case early and emit UnknownError so we skip the ping
  // path.
  QString socketPath = QDir::tempPath() + QLatin1Char('/') + m_connectionString;
  QFileInfo fi(socketPath);
  if (fi.isDir()) {
    emit connectionError(
      UnknownError,
      QString("Cannot start RPC server: '%1' is a directory, not a socket")
        .arg(socketPath));
    return;
  }
#endif

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
