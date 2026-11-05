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
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtNetwork/QLocalServer>
#include <QtNetwork/QLocalSocket>

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#endif

namespace Avogadro::RPC {

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
namespace {

// True if path is a real directory (not a symlink to one), owned by the
// current user, with no group or other permissions.
bool isPrivateDirectory(const QString& path)
{
  struct stat st;
  if (::lstat(QFile::encodeName(path).constData(), &st) != 0)
    return false;
  return S_ISDIR(st.st_mode) && st.st_uid == ::getuid() &&
         (st.st_mode & 077) == 0;
}

} // namespace
#endif

QString LocalSocketConnectionListener::resolveServerName(const QString& name,
                                                         QString* error)
{
  // An explicit path is the caller's choice.
  if (QDir::isAbsolutePath(name))
    return name;

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
  // Prefer the session's private runtime directory.
  const QString runtimeDir = qEnvironmentVariable("XDG_RUNTIME_DIR");
  if (!runtimeDir.isEmpty()) {
    if (QDir::isAbsolutePath(runtimeDir) && isPrivateDirectory(runtimeDir))
      return QDir::cleanPath(runtimeDir) + QLatin1Char('/') + name;
    qWarning() << "Ignoring XDG_RUNTIME_DIR" << runtimeDir
               << ": not an absolute path to a directory owned by this user "
                  "with no group or other access";
  }

  // Otherwise a per-user directory in the temporary directory, like tmux and
  // ssh-agent. Create it if needed, then verify it: another user could have
  // created (or symlinked) the same name first.
  const QString dir = QDir::tempPath() + QStringLiteral("/avogadro-") +
                      QString::number(static_cast<qulonglong>(::getuid()));
  if (::mkdir(QFile::encodeName(dir).constData(), 0700) != 0 &&
      errno != EEXIST) {
    if (error) {
      *error = QStringLiteral("Cannot create RPC socket directory %1: %2")
                 .arg(dir, QString::fromLocal8Bit(::strerror(errno)));
    }
    return QString();
  }
  if (!isPrivateDirectory(dir)) {
    if (error) {
      *error = QStringLiteral("Refusing to use RPC socket directory %1: not "
                              "owned by this user or accessible by others")
                 .arg(dir);
    }
    return QString();
  }
  return dir + QLatin1Char('/') + name;
#else
  // Windows pipe names, and macOS where Qt uses the private per-user
  // temporary directory.
  Q_UNUSED(error)
  return name;
#endif
}

LocalSocketConnectionListener::LocalSocketConnectionListener(
  QObject* parentObject, const QString& connString)
  : ConnectionListener(parentObject)
  , m_server(new QLocalServer())
{
  m_connectionString = resolveServerName(connString, &m_resolveError);
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
  if (m_connectionString.isEmpty()) {
    emit connectionError(UnknownError, m_resolveError);
    return;
  }

#if defined(Q_OS_UNIX)
  // On Unix, QLocalServer creates a socket file. If that path already exists
  // but is not a socket (e.g. someone created it as a directory), listen()
  // will fail with AddressInUseError. That would trigger the ping-and-retry
  // logic, which tries to connect to a directory and crashes. Detect this case
  // early and emit UnknownError so we skip the ping path.
#if defined(Q_OS_MACOS)
  // Qt puts a bare name in the per-user temporary directory.
  QString socketPath = QDir::isAbsolutePath(m_connectionString)
                         ? m_connectionString
                         : QDir::tempPath() + QLatin1Char('/') +
                             m_connectionString;
#else
  QString socketPath = m_connectionString;
#endif
  QFileInfo fi(socketPath);
  if (fi.isDir()) {
    emit connectionError(
      UnknownError,
      QString("Cannot start RPC server: '%1' is a directory, not a socket")
        .arg(socketPath));
    return;
  }
#endif

#if defined(Q_OS_WIN)
  // Restrict the named pipe to the current user. This must not be used on
  // Unix: Qt then binds a temporary name and rename()s it over the target,
  // so a second listen() silently replaces a live server's socket instead of
  // failing with AddressInUseError.
  m_server->setSocketOptions(QLocalServer::UserAccessOption);
#endif

  if (!m_server->listen(m_connectionString)) {
    emit connectionError(toConnectionListenerError(m_server->serverError()),
                         m_server->errorString());
    return;
  }

#if defined(Q_OS_UNIX)
  // Defence in depth: only the owner needs to read or write the socket.
  if (!QFile::setPermissions(m_server->fullServerName(),
                             QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
    qWarning() << "Could not restrict permissions on RPC socket"
               << m_server->fullServerName();
  }
#endif
  qDebug() << "RPC server listening on:" << m_server->fullServerName();
}

void LocalSocketConnectionListener::stop(bool force)
{
  if (force && !m_connectionString.isEmpty())
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
