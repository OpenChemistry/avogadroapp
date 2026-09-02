/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#include "rpclistener.h"
#include "mainwindow.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QInputDialog>

#include <QtCore/QJsonValue>
#include <QtCore/QTimer>
#include <QtCore/QVariantMap>

#include <avogadro/io/fileformatmanager.h>
#include <avogadro/qtgui/molecule.h>

#include "rpc/connection.h"
#include "rpc/jsonrpc.h"
#include "rpc/jsonrpcclient.h"
#include "rpc/localsocketconnectionlistener.h"

namespace Avogadro {

using Io::FileFormatManager;
using QtGui::Molecule;
using std::string;

namespace {
/// How long to wait for a command before giving up on it, in seconds. Most
/// commands are fast; a script that expects a slow one passes "timeout".
constexpr int defaultCommandTimeout = 60;

/// The request could not be carried out.
constexpr int errorRequestFailed = -1;
/// A command that was started failed, or timed out.
constexpr int errorCommandFailed = -2;
/// The plugin is already running a command.
constexpr int errorPluginBusy = -3;
/// No tool or extension claims this method.
constexpr int errorMethodNotFound = -32601;
} // namespace

RpcListener::RpcListener(QObject* parent_)
  : QObject(parent_)
  , m_pingClient(nullptr)
{
  m_rpc = new RPC::JsonRpc(this);

  m_connectionListener =
    new RPC::LocalSocketConnectionListener(this, "avogadro");

  connect(m_connectionListener, &RPC::ConnectionListener::connectionError, this,
          &RpcListener::connectionError);

  m_rpc->addConnectionListener(m_connectionListener);

  connect(m_rpc, &RPC::JsonRpc::messageReceived, this,
          &RpcListener::messageReceived);

  // Find the main window.
  m_window = nullptr;
  foreach (QWidget* widget, QApplication::topLevelWidgets())
    if ((m_window = qobject_cast<MainWindow*>(widget)))
      break;

  if (m_window) {
    connect(this, &RpcListener::callSetMolecule, m_window,
            &MainWindow::setMolecule);
    connect(m_window, &MainWindow::commandCompleted, this,
            &RpcListener::resolvePending);
  }

  // Do not leave a script waiting on a reply that will never come.
  connect(qApp, &QCoreApplication::aboutToQuit, this,
          [this]() { failAllPending(tr("Avogadro is shutting down.")); });
}

RpcListener::~RpcListener()
{
  m_rpc->removeConnectionListener(m_connectionListener);
  m_connectionListener->stop();
}

void RpcListener::start()
{
  m_connectionListener->start();
}

void RpcListener::connectionError(RPC::ConnectionListener::Error error,
                                  const QString& message)
{
  qDebug() << "Error starting RPC server:" << message;
  if (error == RPC::ConnectionListener::AddressInUseError) {
    // Try to ping the existing server to see if it is alive:
    if (!m_pingClient)
      m_pingClient = new RPC::JsonRpcClient(this);
    bool result(
      m_pingClient->connectToServer(m_connectionListener->connectionString()));

    if (result) {
      QJsonObject request(m_pingClient->emptyRequest());
      request["method"] = QLatin1String("internalPing");
      connect(m_pingClient, &RPC::JsonRpcClient::resultReceived, this,
              &RpcListener::receivePingResponse);
      result = m_pingClient->sendRequest(request);
    }

    // If any of the above failed, trigger a failure now:
    if (!result)
      receivePingResponse();
    else // Otherwise wait 200 ms
      QTimer::singleShot(200, this, [this]() { receivePingResponse(); });
  }
}

void RpcListener::receivePingResponse(const QJsonObject& response)
{
  // Disconnect and remove the ping client the first time this is called:
  if (m_pingClient) {
    m_pingClient->deleteLater();
    m_pingClient = nullptr;
  } else {
    // In case the single shot timeout is triggered after the slot is called
    // directly or in response to m_pingClient's signal.
    return;
  }

  bool pingSuccessful = response.value("result").toString() == QString("pong");
  if (pingSuccessful) {
    qDebug() << "Other server is alive. Not starting new instance.";
  } else {
    qDebug() << "Starting new server.";
    m_connectionListener->stop(true);
    m_connectionListener->start();
  }
}

void RpcListener::messageReceived(const RPC::Message& message)
{
  QString method = message.method();
  QJsonObject params = message.params().toObject();

  // "wait" and "timeout" are reserved: they configure how this request is
  // answered, and are removed so that no plugin ever sees them as an option.
  const bool wait = params.take("wait").toBool(false);
  const QJsonValue timeoutValue = params.take("timeout");
  int timeoutSeconds = defaultCommandTimeout;
  if (timeoutValue.isDouble()) {
    timeoutSeconds = static_cast<int>(timeoutValue.toDouble());
    if (timeoutSeconds < 1)
      timeoutSeconds = 1;
  }

  // check for quit message first, since it doesn't require a window
  if (method == "kill") {
    // Only allow avogadro to be killed through RPC if it was started with the
    // '--testing' flag.
    if (qApp->arguments().contains("--testing")) {
      RPC::Message response = message.generateResponse();
      response.setResult(true);
      response.send();

      qApp->quit();
    } else {
      // send error response
      RPC::Message errorMessage = message.generateErrorResponse();
      errorMessage.setErrorCode(errorRequestFailed);
      errorMessage.setErrorMessage(
        "Ignoring kill command. Start with '--testing' to enable.");
      errorMessage.send();
    }
    // Either way the request has been answered. Without this the message
    // falls through to the handler chain below and a second reply is sent.
    return;
  }

  // check if there's an active window
  if (m_window == nullptr) {
    // send error response
    RPC::Message errorMessage = message.generateErrorResponse();
    errorMessage.setErrorCode(errorRequestFailed);
    errorMessage.setErrorMessage("No Active Avogadro Window");
    errorMessage.send();
    return;
  }

  // okay, window is open
  if (method == "openFile") {
    // Read the supplied file.
    string fileName = params["fileName"].toString().toStdString();
    auto* molecule = new Molecule(this);
    bool success = FileFormatManager::instance().readFile(*molecule, fileName);
    if (success) {
      emit callSetMolecule(molecule);

      // set response
      RPC::Message response = message.generateResponse();
      response.setResult(true);
      response.send();
    } else {
      delete molecule;

      // send error response
      RPC::Message errorMessage = message.generateErrorResponse();
      errorMessage.setErrorCode(errorRequestFailed);
      errorMessage.setErrorMessage(
        QString("Failed to read file: %1")
          .arg(QString::fromStdString(FileFormatManager::instance().error())));
      errorMessage.send();
    }
  } else if (method == "saveGraphic") {
    // Read the supplied file.
    QString fileName = params["fileName"].toString();

    // save the image
    m_window->exportGraphics(fileName);

    // set response
    RPC::Message response = message.generateResponse();
    response.setResult(true);
    response.send();
  } else if (method == "exportFile") {
    // Save to the supplied file name
    QString filename = params["fileName"].toString();

    bool result = m_window->exportFile(filename);

    // set response
    RPC::Message response = message.generateResponse();
    response.setResult(result);
    response.send();
  } else if (method == "loadMolecule") {
    // get molecule data and format
    string content = params["content"].toString().toStdString();
    string format = params["format"].toString().toStdString();

    // read molecule data
    auto* molecule = new Molecule(this);
    bool success =
      FileFormatManager::instance().readString(*molecule, content, format);
    if (success) {
      emit callSetMolecule(molecule);

      // send response
      RPC::Message response = message.generateResponse();
      response.setResult(true);
      response.send();
    } else {
      delete molecule;

      // send error response
      RPC::Message errorMessage = message.generateErrorResponse();
      errorMessage.setErrorCode(errorRequestFailed);
      errorMessage.setErrorMessage(
        QString("Failed to read Chemical JSON: %1")
          .arg(QString::fromStdString(FileFormatManager::instance().error())));
      errorMessage.send();
    }
  } else { // ask the main window to handle the message
    QVariantMap options = params.toVariantMap();
    // Only a request that asked to wait needs a token to report back with.
    const quint64 token = wait ? ++m_nextToken : 0;
    QString resultMessage;
    QVariantMap resultData;

    MainWindow::CommandStatus status = m_window->handleCommand(
      method, options, token, &resultMessage, &resultData);

    switch (status) {
      case MainWindow::CommandStatus::Started:
        if (wait) {
          // Hold the reply until the plugin reports back.
          holdReply(message, token, timeoutSeconds);
        } else {
          // The caller did not ask to wait, so answer as we always have.
          RPC::Message response = message.generateResponse();
          response.setResult(true);
          response.send();
        }
        break;

      case MainWindow::CommandStatus::Finished:
        if (wait) {
          sendSuccess(message, resultMessage, resultData);
        } else {
          RPC::Message response = message.generateResponse();
          response.setResult(true);
          response.send();
        }
        break;

      case MainWindow::CommandStatus::Failed:
        sendError(message, errorCommandFailed,
                  resultMessage.isEmpty() ? tr("The command failed.")
                                          : resultMessage);
        break;

      case MainWindow::CommandStatus::Busy:
        sendError(message, errorPluginBusy,
                  tr("That plugin is already running a command."));
        break;

      case MainWindow::CommandStatus::NotHandled: {
        // send error response
        RPC::Message errorMessage = message.generateErrorResponse();
        errorMessage.setErrorCode(errorMethodNotFound);
        errorMessage.setErrorMessage("Method not found");
        QJsonObject errorDataObject;
        errorDataObject.insert("request", message.toJsonObject());
        errorMessage.setErrorData(errorDataObject);
        errorMessage.send();
        break;
      }
    }
  }
}

void RpcListener::holdReply(const RPC::Message& message, quint64 token,
                            int timeoutSeconds)
{
  PendingCommand pending;
  pending.request = message;
  pending.connection = message.connection();
  pending.timer = new QTimer(this);
  pending.timer->setSingleShot(true);
  pending.timer->setInterval(timeoutSeconds * 1000);
  connect(pending.timer, &QTimer::timeout, this, [this, token]() {
    // The caller has given up. Release the plugin as well, so that a command
    // which never reports back does not leave it busy for the whole session.
    if (m_window != nullptr)
      m_window->abandonCommand(token);
    resolvePending(token, false, tr("The command timed out."), QVariantMap());
  });

  m_pending.insert(token, pending);
  pending.timer->start();
}

void RpcListener::resolvePending(quint64 token, bool success,
                                 const QString& message,
                                 const QVariantMap& result)
{
  auto match = m_pending.find(token);
  if (match == m_pending.end())
    return; // already answered, or the caller never asked to wait

  const PendingCommand pending = match.value();
  m_pending.erase(match);

  if (pending.timer != nullptr)
    pending.timer->deleteLater();

  // The client may have gone away while the command was running. The request
  // holds a bare pointer to the connection, so check before answering.
  if (pending.connection.isNull())
    return;

  if (success)
    sendSuccess(pending.request, message, result);
  else
    sendError(pending.request, errorCommandFailed,
              message.isEmpty() ? tr("The command failed.") : message);
}

void RpcListener::failAllPending(const QString& reason)
{
  const QList<quint64> tokens = m_pending.keys();
  foreach (quint64 token, tokens)
    resolvePending(token, false, reason, QVariantMap());
}

void RpcListener::sendSuccess(const RPC::Message& request,
                              const QString& message, const QVariantMap& result)
{
  QJsonObject payload;
  payload.insert("status", QStringLiteral("finished"));
  if (!message.isEmpty())
    payload.insert("message", message);
  if (!result.isEmpty())
    payload.insert("data", QJsonObject::fromVariantMap(result));

  RPC::Message response = request.generateResponse();
  response.setResult(payload);
  response.send();
}

void RpcListener::sendError(const RPC::Message& request, int code,
                            const QString& message)
{
  RPC::Message errorMessage = request.generateErrorResponse();
  errorMessage.setErrorCode(code);
  errorMessage.setErrorMessage(message);
  errorMessage.send();
}

} // End of Avogadro namespace
