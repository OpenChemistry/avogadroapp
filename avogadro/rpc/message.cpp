/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").

  Adapted from MoleQueue. Original copyright:
  Copyright 2012 Kitware, Inc.
******************************************************************************/

#include "message.h"

#include "connection.h"
#include "messageidmanager_p.h"

#include <QtCore/QDebug>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QStringList>

namespace Avogadro::RPC {

// Dummy value returned by *Ref() functions called with invalid types.
QJsonValue dummyValue(QJsonValue::Null);

Message::Message(Connection* conn, EndpointIdType endpoint_)
  : m_type(Invalid)
  , m_errorCode(0)
  , m_connection(conn)
  , m_endpoint(endpoint_)
{
}

Message::Message(Message::MessageType type_, Connection* conn,
                 EndpointIdType endpoint_)
  : m_type(type_)
  , m_errorCode(0)
  , m_connection(conn)
  , m_endpoint(endpoint_)
{
}

Message::Message(const QJsonObject& rawJson, Connection* conn,
                 EndpointIdType endpoint_)
  : m_type(Raw)
  , m_errorCode(0)
  , m_rawJson(rawJson)
  , m_connection(conn)
  , m_endpoint(endpoint_)
{
}

Message::Message(const Message& other)
  : m_type(other.m_type)
  , m_method(other.m_method)
  , m_id(other.m_id)
  , m_params(other.m_params)
  , m_result(other.m_result)
  , m_errorCode(other.m_errorCode)
  , m_errorMessage(other.m_errorMessage)
  , m_errorData(other.m_errorData)
  , m_rawJson(other.m_rawJson)
  , m_connection(other.m_connection)
  , m_endpoint(other.m_endpoint)
{
}

Message& Message::operator=(const Message& other)
{
  m_type = other.m_type;
  m_method = other.m_method;
  m_id = other.m_id;
  m_params = other.m_params;
  m_result = other.m_result;
  m_errorCode = other.m_errorCode;
  m_errorMessage = other.m_errorMessage;
  m_errorData = other.m_errorData;
  m_rawJson = other.m_rawJson;
  m_connection = other.m_connection;
  m_endpoint = other.m_endpoint;
  return *this;
}

Message::MessageType Message::type() const
{
  return m_type;
}

QString Message::method() const
{
  if (!checkType(Q_FUNC_INFO, Request | Notification | Response | Error))
    return QString();
  return m_method;
}

void Message::setMethod(const QString& m)
{
  if (!checkType(Q_FUNC_INFO, Request | Notification | Response | Error))
    return;
  m_method = m;
}

QJsonValue Message::params() const
{
  if (!checkType(Q_FUNC_INFO, Request | Notification))
    return QJsonValue();
  return m_params;
}

QJsonValue& Message::paramsRef()
{
  if (!checkType(Q_FUNC_INFO, Request | Notification))
    return dummyValue;
  return m_params;
}

void Message::setParams(const QJsonArray& p)
{
  if (!checkType(Q_FUNC_INFO, Request | Notification))
    return;
  m_params = p;
}

void Message::setParams(const QJsonObject& p)
{
  if (!checkType(Q_FUNC_INFO, Request | Notification))
    return;
  m_params = p;
}

QJsonValue Message::result() const
{
  if (!checkType(Q_FUNC_INFO, Response))
    return QJsonValue();
  return m_result;
}

QJsonValue& Message::resultRef()
{
  if (!checkType(Q_FUNC_INFO, Response))
    return dummyValue;
  return m_result;
}

void Message::setResult(const QJsonValue& r)
{
  if (!checkType(Q_FUNC_INFO, Response))
    return;
  m_result = r;
}

int Message::errorCode() const
{
  if (!checkType(Q_FUNC_INFO, Error))
    return 0;
  return m_errorCode;
}

void Message::setErrorCode(int e)
{
  if (!checkType(Q_FUNC_INFO, Error))
    return;
  m_errorCode = e;
}

QString Message::errorMessage() const
{
  if (!checkType(Q_FUNC_INFO, Error))
    return QString();
  return m_errorMessage;
}

void Message::setErrorMessage(const QString& e)
{
  if (!checkType(Q_FUNC_INFO, Error))
    return;
  m_errorMessage = e;
}

QJsonValue Message::errorData() const
{
  if (!checkType(Q_FUNC_INFO, Error))
    return QJsonValue();
  return m_errorData;
}

QJsonValue& Message::errorDataRef()
{
  if (!checkType(Q_FUNC_INFO, Error))
    return dummyValue;
  return m_errorData;
}

void Message::setErrorData(const QJsonValue& e)
{
  if (!checkType(Q_FUNC_INFO, Error))
    return;
  m_errorData = e;
}

MessageIdType Message::id() const
{
  if (!checkType(Q_FUNC_INFO, Request | Response | Error))
    return MessageIdType();
  return m_id;
}

void Message::setId(const MessageIdType& i)
{
  if (!checkType(Q_FUNC_INFO, Request | Response | Error))
    return;
  m_id = i;
}

Connection* Message::connection() const
{
  return m_connection;
}

void Message::setConnection(Connection* c)
{
  m_connection = c;
}

EndpointIdType Message::endpoint() const
{
  return m_endpoint;
}

void Message::setEndpoint(const EndpointIdType& e)
{
  m_endpoint = e;
}

QJsonObject Message::toJsonObject() const
{
  QJsonObject obj;

  switch (m_type) {
    case Message::Request:
      obj.insert("jsonrpc", QLatin1String("2.0"));
      obj.insert("method", m_method);
      if ((m_params.isObject() && !m_params.toObject().isEmpty()) ||
          (m_params.isArray() && !m_params.toArray().isEmpty())) {
        obj.insert("params", m_params);
      }
      obj.insert("id", m_id);
      break;
    case Message::Notification:
      obj.insert("jsonrpc", QLatin1String("2.0"));
      obj.insert("method", m_method);
      if ((m_params.isObject() && !m_params.toObject().isEmpty()) ||
          (m_params.isArray() && !m_params.toArray().isEmpty())) {
        obj.insert("params", m_params);
      }
      break;
    case Message::Response:
      obj.insert("jsonrpc", QLatin1String("2.0"));
      obj.insert("result", m_result);
      obj.insert("id", m_id);
      break;
    case Message::Error: {
      QJsonObject errorObject;
      errorObject.insert("code", m_errorCode);
      errorObject.insert("message", m_errorMessage);
      if (!m_errorData.isNull())
        errorObject.insert("data", m_errorData);

      obj.insert("jsonrpc", QLatin1String("2.0"));
      obj.insert("error", errorObject);
      obj.insert("id", m_id);
    } break;
    case Message::Raw:
      obj = m_rawJson;
      break;
    case Message::Invalid:
      qWarning() << "Cannot convert invalid message to a JSON object.";
      break;
  }
  return obj;
}

PacketType Message::toJson() const
{
  QJsonDocument doc(toJsonObject());
  return PacketType(doc.toJson());
}

bool Message::send()
{
  if (m_type == Invalid || !m_connection || !m_connection->isOpen())
    return false;

  if (m_type == Request)
    m_id = MessageIdManager::registerMethod(m_method);

  return m_connection->send(toJson(), m_endpoint);
}

Message Message::generateResponse() const
{
  if (!checkType(Q_FUNC_INFO, Request))
    return Message();

  Message resp(Response, m_connection, m_endpoint);
  resp.m_method = m_method;
  resp.m_id = m_id;
  return resp;
}

Message Message::generateErrorResponse() const
{
  if (!checkType(Q_FUNC_INFO, Request | Raw | Invalid))
    return Message();

  Message resp(Error, m_connection, m_endpoint);
  resp.m_method = m_method;
  resp.m_id = m_id;
  return resp;
}

bool Message::parse()
{
  Message message;
  return parse(message);
}

bool Message::parse(Message& errorMessage_)
{
  // Can only parse Raw types -- return true if this message is already parsed
  // or invalid.
  if (m_type != Raw)
    return true;

  // Validate the message
  QStringList errors;

  // jsonrpc must equal "2.0"
  if (!m_rawJson.contains("jsonrpc"))
    errors << "jsonrpc key missing.";
  if (!m_rawJson.value("jsonrpc").isString())
    errors << "jsonrpc key must be a string.";
  if (m_rawJson.value("jsonrpc").toString() != "2.0") {
    errors << QString("Unrecognized jsonrpc string: %1")
                .arg(m_rawJson.value("jsonrpc").toString());
  }

  // Must have either id or method
  if (!m_rawJson.contains("id") && !m_rawJson.contains("method"))
    errors << "Missing both id and method.";

  // If method is present, it must be a string.
  QString method_;
  if (m_rawJson.contains("method")) {
    if (!m_rawJson.value("method").isString())
      errors << "method must be a string.";
    else
      method_ = m_rawJson.value("method").toString();
  } else {
    // Lookup method for response/error.
    method_ = MessageIdManager::lookupMethod(m_rawJson.value("id"));
  }

  // If any errors have occurred, prep the response:
  if (!errors.empty()) {
    errors.prepend("Invalid request:");
    QJsonObject errorDataObject;
    errorDataObject.insert("description", errors.join(" "));
    errorDataObject.insert("request", m_rawJson);
    errorMessage_ = generateErrorResponse();
    errorMessage_.setErrorCode(-32600);
    errorMessage_.setErrorMessage("Invalid request");
    errorMessage_.setErrorData(errorDataObject);
    return false;
  }

  // Results, errors, and notifications cannot return errors. Parse them
  // as best we can and return true.
  if (m_rawJson.contains("result")) {
    interpretResponse(m_rawJson, method_);
    return true;
  } else if (m_rawJson.contains("error")) {
    interpretError(m_rawJson, method_);
    return true;
  } else if (!m_rawJson.contains("id")) {
    interpretNotification(m_rawJson);
    return true;
  }

  // Assume anything else is a request.
  return interpretRequest(m_rawJson, errorMessage_);
}

bool Message::checkType(const char* method_, MessageTypes validTypes) const
{
  if (m_type & validTypes)
    return true;

  qWarning() << "Invalid message type in call.\n"
             << "  Method:" << method_ << "\n"
             << "  Valid types:" << validTypes << "\n"
             << "  Actual type:" << m_type;
  return false;
}

bool Message::interpretRequest(const QJsonObject& json, Message& errorMessage_)
{
  QStringList errors;

  // method must exist and be a string.
  if (!json.value("method").isString())
    errors << "method is not a string.";

  // id must be present.
  if (!json.contains("id"))
    errors << "id missing.";

  // params is optional, but must be structured if present.
  if (json.contains("params") && !json.value("params").isArray() &&
      !json.value("params").isObject()) {
    errors << "params must be structured if present.";
  }

  if (!errors.empty()) {
    errors.prepend("Invalid request:");
    QJsonObject errorDataObject;
    errorDataObject.insert("description", errors.join(" "));
    errorDataObject.insert("request", json);
    errorMessage_ = generateErrorResponse();
    errorMessage_.setErrorCode(-32600);
    errorMessage_.setErrorMessage("Invalid request");
    errorMessage_.setErrorData(errorDataObject);
    return false;
  }

  m_type = Request;
  m_method = json.value("method").toString();
  if (json.contains("params"))
    m_params = json.value("params");
  else
    m_params = QJsonValue();
  m_id = MessageIdType(json.value("id"));
  return true;
}

void Message::interpretNotification(const QJsonObject& json)
{
  m_type = Notification;
  m_method = json.value("method").toString();
  if (json.contains("params"))
    m_params = json.value("params");
  else
    m_params = QJsonValue();
  m_id = MessageIdType();
}

void Message::interpretResponse(const QJsonObject& json, const QString& method_)
{
  m_type = Response;
  m_method = method_;
  m_result = json.value("result");
  m_id = json.value("id");
}

void Message::interpretError(const QJsonObject& json, const QString& method_)
{
  m_type = Error;
  m_method = method_;
  m_id = json.value("id");

  QStringList errors;
  QJsonValue errorValue = json.value("error");
  if (!errorValue.isObject()) {
    errors << "error must be an object.";
  } else {
    QJsonObject errorObject = errorValue.toObject();

    // error.code validation
    if (!errorObject.contains("code")) {
      errors << "error.code missing.";
    } else {
      if (!errorObject.value("code").isDouble()) {
        errors << "error.code is not numeric.";
      } else {
        double code = errorObject.value("code").toDouble();
        if (qAbs(code - static_cast<double>(static_cast<int>(code))) > 1e-5)
          errors << "error.code is not integral.";
        else
          m_errorCode = static_cast<int>(code);
      }
    }

    // error.message validation
    if (!errorObject.contains("message")) {
      errors << "error.message missing.";
    } else {
      if (!errorObject.value("message").isString())
        errors << "error.message is not a string.";
      else
        m_errorMessage = errorObject.value("message").toString();
    }

    if (errorObject.contains("data"))
      m_errorData = errorObject.value("data");
  }

  // If any errors occurred, reset the error members to a server error.
  if (!errors.empty()) {
    m_errorCode = -32000;
    m_errorMessage = "Server error";

    QJsonObject errorDataObject;
    errors.prepend("Malformed error response:");
    errorDataObject.insert("description", errors.join(" "));
    errorDataObject.insert("origMessage", errorValue);
    m_errorData = errorDataObject;
  }
}

} // namespace Avogadro::RPC
