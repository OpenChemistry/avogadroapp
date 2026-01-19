/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").

  Adapted from MoleQueue. Original copyright:
  Copyright 2012 Kitware, Inc.
******************************************************************************/

#ifndef AVOGADRO_RPC_MESSAGE_H
#define AVOGADRO_RPC_MESSAGE_H

#include "rpcglobal.h"

#include <QtCore/QByteArray>
#include <QtCore/QFlags>
#include <QtCore/QJsonObject>
#include <QtCore/QMetaType>
#include <QtCore/QString>

namespace Avogadro::RPC {

class Connection;
class JsonRpc;

/**
 * @class Message message.h <avogadro/rpc/message.h>
 * @brief The Message class encapsulates a single JSON-RPC transmission.
 *
 * The Message class provides an interface to construct, interpret, and
 * manipulate JSON-RPC messages.
 *
 * There are four types of valid JSON-RPC messages: Requests, notifications,
 * responses, and errors. The type() method can be used to determine a given
 * Message's MessageType.
 */
class Message
{
public:
  /// Flags representing different types of JSON-RPC messages
  enum MessageType
  {
    /// A JSON-RPC request, with id, method, and params attributes.
    Request = 0x1,
    /// A JSON-RPC notification, with method and params attributes.
    Notification = 0x2,
    /// A JSON-RPC response, with id, method, and result attributes.
    Response = 0x4,
    /// A JSON-RPC error, with id, method, and errorCode, errorMessage, and
    /// errorData attributes.
    Error = 0x8,
    /// This MessageType indicates that this Message holds a raw QJsonObject
    /// that has not been interpreted. Call parse() to convert this Message
    /// into an appropriate type.
    Raw = 0x10,
    /// This Message is invalid.
    Invalid = 0x20
  };
  Q_DECLARE_FLAGS(MessageTypes, MessageType)

  /// Construct an Invalid Message using the @a conn and @a endpoint_.
  Message(Connection* conn = nullptr,
          EndpointIdType endpoint_ = EndpointIdType());

  /// Construct an empty Message with the specified @a type that uses the
  /// @a conn and @a endpoint_.
  Message(MessageType type_, Connection* conn = nullptr,
          EndpointIdType endpoint_ = EndpointIdType());

  /// Construct a Raw Message with the specified @a type that uses the
  /// @a conn and @a endpoint_. The @a rawJson QJsonObject will be cached to be
  /// parsed by parse() later.
  Message(const QJsonObject& rawJson, Connection* conn = nullptr,
          EndpointIdType endpoint_ = EndpointIdType());

  /// Copy constructor
  Message(const Message& other);

  /// Assignment operator
  Message& operator=(const Message& other);

  /// @return The MessageType of this Message.
  MessageType type() const;

  /**
   * The name of the method used in the remote procedure call.
   * @note This function is only valid for Request, Notification, Response, and
   * Error messages.
   */
  QString method() const;
  void setMethod(const QString& m);

  /**
   * The parameters used in the remote procedure call.
   * @note This function is only valid for Request and Notification messages.
   */
  QJsonValue params() const;
  QJsonValue& paramsRef();
  void setParams(const QJsonArray& p);
  void setParams(const QJsonObject& p);

  /**
   * The result object used in a remote procedure call response.
   * @note This function is only valid for Response messages.
   */
  QJsonValue result() const;
  QJsonValue& resultRef();
  void setResult(const QJsonValue& r);

  /**
   * The integral error code used in a remote procedure call error response.
   * @note This function is only valid for Error messages.
   */
  int errorCode() const;
  void setErrorCode(int e);

  /**
   * The error message string used in a remote procedure call error response.
   * @note This function is only valid for Error messages.
   */
  QString errorMessage() const;
  void setErrorMessage(const QString& e);

  /**
   * The data object used in a remote procedure call error response.
   * @note This function is only valid for Error messages.
   */
  QJsonValue errorData() const;
  QJsonValue& errorDataRef();
  void setErrorData(const QJsonValue& e);

  /**
   * The message id used in a remote procedure call.
   * @note This function is only valid for Request, Response, and Error
   * messages.
   */
  MessageIdType id() const;

protected: // Users should have no reason to set this:
  void setId(const MessageIdType& i);

public:
  /// The connection associated with the remote procedure call.
  Connection* connection() const;
  void setConnection(Connection* c);

  /// The connection endpoint associated with the remote procedure call.
  EndpointIdType endpoint() const;
  void setEndpoint(const EndpointIdType& e);

  /// @return A QJsonObject representation of the remote procedure call.
  QJsonObject toJsonObject() const;

  /// @return A string representation of the remote procedure call.
  PacketType toJson() const;

  /**
   * @brief Send the message to the associated connection and endpoint.
   * @return True on success, false on failure.
   * @note If this message is a Request, a unique id will be assigned prior to
   * sending. Use the id() method to retrieve the assigned id.
   */
  bool send();

  /**
   * @brief Create a new Response message in reply to a Request.
   * The connection, endpoint, id, and method will be copied from this Message.
   * @note This function is only valid for Request messages.
   */
  Message generateResponse() const;

  /**
   * @brief Create a new Error message in reply to a Request.
   * The connection, endpoint, id, and method will be copied from this Message.
   * @note This function is only valid for Request, Raw, and Invalid messages.
   */
  Message generateErrorResponse() const;

  /**
   * @brief Interpret the raw QJsonObject passed to the constructor.
   * @return True on success, false on failure.
   * @note This function is only valid for Raw messages.
   */
  bool parse();
  bool parse(Message& errorMessage_);

private:
  bool checkType(const char* method_, MessageTypes validTypes) const;
  bool interpretRequest(const QJsonObject& json, Message& errorMessage);
  void interpretNotification(const QJsonObject& json);
  void interpretResponse(const QJsonObject& json, const QString& method_);
  void interpretError(const QJsonObject& json, const QString& method_);

  MessageType m_type;
  QString m_method;
  MessageIdType m_id;
  QJsonValue m_params;
  QJsonValue m_result;
  int m_errorCode;
  QString m_errorMessage;
  QJsonValue m_errorData;
  QJsonObject m_rawJson;
  Connection* m_connection;
  EndpointIdType m_endpoint;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Message::MessageTypes)

} // namespace Avogadro::RPC

Q_DECLARE_METATYPE(Avogadro::RPC::Message)

#endif // AVOGADRO_RPC_MESSAGE_H
