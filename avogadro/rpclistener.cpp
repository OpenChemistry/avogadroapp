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

#include <avogadro/io/fileformatmanager.h>
#include <avogadro/qtgui/molecule.h>

#include "rpc/jsonrpc.h"
#include "rpc/jsonrpcclient.h"
#include "rpc/localsocketconnectionlistener.h"

namespace Avogadro {

using Io::FileFormatManager;
using QtGui::Molecule;
using std::string;

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
  }
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
      errorMessage.setErrorCode(-1);
      errorMessage.setErrorMessage(
        "Ignoring kill command. Start with '--testing' to enable.");
      errorMessage.send();
    }
  }

  // check if there's an active window
  if (m_window == nullptr) {
    // send error response
    RPC::Message errorMessage = message.generateErrorResponse();
    errorMessage.setErrorCode(-1);
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
      errorMessage.setErrorCode(-1);
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
      errorMessage.setErrorCode(-1);
      errorMessage.setErrorMessage(
        QString("Failed to read Chemical JSON: %1")
          .arg(QString::fromStdString(FileFormatManager::instance().error())));
      errorMessage.send();
    }
  } else { // ask the main window to handle the message
    QVariantMap options = params.toVariantMap();
    bool success = m_window->handleCommand(method, options);

    if (success) {
      // send response
      RPC::Message response = message.generateResponse();
      response.setResult(true);
      response.send();
    } else {
      // send error response
      RPC::Message errorMessage = message.generateErrorResponse();
      errorMessage.setErrorCode(-32601);
      errorMessage.setErrorMessage("Method not found");
      QJsonObject errorDataObject;
      errorDataObject.insert("request", message.toJsonObject());
      errorMessage.setErrorData(errorDataObject);
      errorMessage.send();
    }
  }
}

} // End of Avogadro namespace
