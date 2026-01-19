/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").

  Adapted from MoleQueue. Original copyright:
  Copyright 2012 Kitware, Inc.
******************************************************************************/

#include "jsonrpc.h"
#include "connectionlistener.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QMetaType>

namespace Avogadro::RPC {

JsonRpc::JsonRpc(QObject* parent_)
  : QObject(parent_)
{
  qRegisterMetaType<Message>("Avogadro::RPC::Message");
  qRegisterMetaType<PacketType>("Avogadro::RPC::PacketType");
  qRegisterMetaType<EndpointIdType>("Avogadro::RPC::EndpointIdType");
}

JsonRpc::~JsonRpc() {}

void JsonRpc::addConnectionListener(ConnectionListener* connlist)
{
  if (m_connections.keys().contains(connlist))
    return;

  m_connections.insert(connlist, QList<Connection*>());
  connect(connlist, SIGNAL(newConnection(Avogadro::RPC::Connection*)),
          SLOT(addConnection(Avogadro::RPC::Connection*)));
  connect(connlist, SIGNAL(destroyed()),
          SLOT(removeConnectionListenerInternal()));
}

void JsonRpc::removeConnectionListener(ConnectionListener* connlist)
{
  disconnect(nullptr, connlist);
  foreach (Connection* conn, m_connections.value(connlist))
    this->removeConnection(connlist, conn);

  m_connections.remove(connlist);
}

void JsonRpc::addConnection(Connection* conn)
{
  ConnectionListener* connlist = qobject_cast<ConnectionListener*>(sender());

  if (!connlist || !m_connections.keys().contains(connlist))
    return;

  QList<Connection*>& conns = m_connections[connlist];
  if (conns.contains(conn))
    return;

  conns << conn;

  connect(conn, SIGNAL(destroyed()), SLOT(removeConnection()));
  connect(
    conn,
    SIGNAL(
      packetReceived(Avogadro::RPC::PacketType, Avogadro::RPC::EndpointIdType)),
    SLOT(newPacket(Avogadro::RPC::PacketType, Avogadro::RPC::EndpointIdType)));

  conn->start();
}

void JsonRpc::removeConnection(ConnectionListener* connlist, Connection* conn)
{
  disconnect(nullptr, conn);

  if (!m_connections.contains(connlist))
    return;

  QList<Connection*>& conns = m_connections[connlist];
  conns.removeOne(conn);
}

void JsonRpc::removeConnection(Connection* conn)
{
  // Find the connection listener:
  foreach (ConnectionListener* connlist, m_connections.keys()) {
    if (m_connections[connlist].contains(conn)) {
      removeConnection(connlist, conn);
      return;
    }
  }
}

void JsonRpc::removeConnection()
{
  if (Connection* conn = reinterpret_cast<Connection*>(sender()))
    removeConnection(conn);
}

void JsonRpc::removeConnectionListenerInternal()
{
  if (ConnectionListener* cl = reinterpret_cast<ConnectionListener*>(sender()))
    removeConnectionListener(cl);
}

void JsonRpc::newPacket(const PacketType& packet,
                        const EndpointIdType& endpoint)
{
  Connection* conn = qobject_cast<Connection*>(sender());
  if (!conn)
    return;

  // Parse the packet as JSON
  QJsonParseError error;
  QJsonDocument doc = QJsonDocument::fromJson(QByteArray(packet), &error);

  // Send a server error and return if there was an issue parsing the packet.
  if (error.error != QJsonParseError::NoError || doc.isNull()) {
    Message errorMessage(Message::Error, conn, endpoint);
    errorMessage.setErrorCode(-32700);
    errorMessage.setErrorMessage("Parse error");

    QJsonObject errorDataObject;
    errorDataObject.insert("QJsonParseError::error", error.error);
    errorDataObject.insert("QJsonParseError::errorString", error.errorString());
    errorDataObject.insert("QJsonParseError::offset", error.offset);
    errorDataObject.insert("bytes received", QLatin1String(packet.constData()));
    errorMessage.send();
    return;
  }

  // Pass the JSON off for further processing. Must be an array or object.
  handleJsonValue(conn, endpoint,
                  doc.isArray() ? QJsonValue(doc.array())
                                : QJsonValue(doc.object()));
}

void JsonRpc::handleJsonValue(Connection* conn, const EndpointIdType& endpoint,
                              const QJsonValue& json)
{
  // Handle batch requests recursively
  if (json.isArray()) {
    foreach (const QJsonValue& val, json.toArray())
      handleJsonValue(conn, endpoint, val);
    return;
  }

  // Objects are RPC calls
  if (!json.isObject()) {
    Message errorMessage(Message::Error, conn, endpoint);
    errorMessage.setErrorCode(-32600);
    errorMessage.setErrorMessage("Invalid Request");

    QJsonObject errorDataObject;
    errorDataObject.insert("description",
                           QLatin1String("Request is not a JSON object."));
    errorDataObject.insert("request", json);
    errorMessage.send();
    return;
  }

  Message message(json.toObject(), conn, endpoint);
  Message errorMessage;
  if (!message.parse(errorMessage)) {
    errorMessage.send();
    return;
  }

  // Handle ping requests internally
  if (message.type() == Message::Request &&
      message.method() == "internalPing") {
    Message response = message.generateResponse();
    response.setResult(QLatin1String("pong"));
    response.send();
    return;
  }

  emit messageReceived(message);
}

} // namespace Avogadro::RPC
