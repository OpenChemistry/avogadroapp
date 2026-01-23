/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").

  Adapted from MoleQueue. Original copyright:
  Copyright 2012 Kitware, Inc.
******************************************************************************/

#include "localsocketconnection.h"

#include <QtCore/QDataStream>
#include <QtCore/QDateTime>
#include <QtCore/QTimer>
#include <QtNetwork/QLocalSocket>

namespace Avogadro::RPC {

LocalSocketConnection::LocalSocketConnection(QObject* parentObject,
                                             QLocalSocket* socket)
  : Connection(parentObject)
  , m_connectionString(socket->serverName())
  , m_socket(nullptr)
  , m_dataStream(new QDataStream)
  , m_holdRequests(true)
{
  setSocket(socket);
}

LocalSocketConnection::LocalSocketConnection(QObject* parentObject,
                                             const QString& serverName)
  : Connection(parentObject)
  , m_connectionString(serverName)
  , m_socket(nullptr)
  , m_dataStream(new QDataStream)
  , m_holdRequests(true)
{
  setSocket(new QLocalSocket);
}

LocalSocketConnection::~LocalSocketConnection()
{
  // Make sure we are closed
  close();

  delete m_socket;
  m_socket = nullptr;

  delete m_dataStream;
  m_dataStream = nullptr;
}

void LocalSocketConnection::setSocket(QLocalSocket* socket)
{
  if (m_socket != nullptr) {
    m_socket->abort();
    m_socket->disconnect(this);
    disconnect(m_socket);
    m_socket->deleteLater();
  }
  if (socket != nullptr) {
    connect(socket, SIGNAL(readyRead()), this, SLOT(readSocket()));
    connect(socket, SIGNAL(disconnected()), this, SIGNAL(disconnected()));
    connect(socket, SIGNAL(destroyed()), this, SLOT(socketDestroyed()));
  }
  m_dataStream->setDevice(socket);
  m_dataStream->setVersion(QDataStream::Qt_4_8);
  m_socket = socket;
}

void LocalSocketConnection::readSocket()
{
  if (!m_socket->isValid())
    return;

  if (m_holdRequests)
    return;

  if (m_socket->bytesAvailable() == 0)
    return;

  PacketType packet;
  (*m_dataStream) >> packet;

  emit packetReceived(packet, EndpointIdType());

  // Check again in 50 ms if no more data is available, or immediately if there
  // is. This helps ensure that burst traffic is handled robustly.
  QTimer::singleShot(m_socket->bytesAvailable() > 0 ? 0 : 50, this,
                     SLOT(readSocket()));
}

void LocalSocketConnection::open()
{
  if (m_socket) {
    if (isOpen()) {
      qWarning() << "Socket already connected to" << m_connectionString;
      return;
    }

    m_socket->connectToServer(m_connectionString);
  } else {
    qWarning() << "No socket set, connection not opened.";
  }
}

void LocalSocketConnection::start()
{
  if (m_socket) {
    m_holdRequests = false;
    while (m_socket->bytesAvailable() != 0)
      readSocket();
  }
}

void LocalSocketConnection::close()
{
  if (m_socket) {
    if (m_socket->isOpen()) {
      m_socket->disconnectFromServer();
      m_socket->close();
    }
  }
}

bool LocalSocketConnection::isOpen()
{
  return m_socket != nullptr && m_socket->isOpen();
}

QString LocalSocketConnection::connectionString() const
{
  return m_connectionString;
}

bool LocalSocketConnection::send(const PacketType& packet,
                                 const EndpointIdType& endpoint)
{
  Q_UNUSED(endpoint);

  // Because of a possible bug with Qt 5.8 and 5.9 on Windows,
  // (*m_dataStream) << packet sends two packets instead of one.
  // To fix this, we write the message to a byte array and send it
  // as a single raw data packet.
#ifdef _WIN32
  PacketType byteArray;
  QDataStream tmpStream(&byteArray, QIODevice::WriteOnly);
  tmpStream << packet;
  m_dataStream->writeRawData(byteArray.constData(), byteArray.size());
#else
  (*m_dataStream) << packet;
#endif

  return true;
}

void LocalSocketConnection::flush()
{
  if (m_socket)
    m_socket->flush();
}

void LocalSocketConnection::socketDestroyed()
{
  // Set to nullptr so we know we don't need to clean up
  m_socket = nullptr;
  // Tell anyone listening we have been disconnected.
  emit disconnected();
}

} // namespace Avogadro::RPC
