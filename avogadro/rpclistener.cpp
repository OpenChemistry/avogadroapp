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

#include <molequeue/client/jsonrpcclient.h>
#include <molequeue/servercore/jsonrpc.h>
#include <molequeue/servercore/localsocketconnectionlistener.h>

namespace Avogadro {

using std::string;
using Io::FileFormatManager;
using QtGui::Molecule;

RpcListener::RpcListener(QObject* parent_)
  : QObject(parent_), m_pingClient(nullptr)
{
  m_rpc = new MoleQueue::JsonRpc(this);

  m_connectionListener =
    new MoleQueue::LocalSocketConnectionListener(this, "avogadro");

  connect(
    m_connectionListener,
    &MoleQueue::ConnectionListener::connectionError,
    this, &RpcListener::connectionError);

  m_rpc->addConnectionListener(m_connectionListener);

  connect(m_rpc, &MoleQueue::JsonRpc::messageReceived, this,
          &RpcListener::messageReceived);

  // Find the main window.
  m_window = 0;
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

void RpcListener::connectionError(MoleQueue::ConnectionListener::Error error,
                                  const QString& message)
{
  qDebug() << "Error starting RPC server:" << message;
  if (error == MoleQueue::ConnectionListener::AddressInUseError) {
    // Try to ping the existing server to see if it is alive:
    if (!m_pingClient)
      m_pingClient = new MoleQueue::JsonRpcClient(this);
    bool result(
      m_pingClient->connectToServer(m_connectionListener->connectionString()));

    if (result) {
      QJsonObject request(m_pingClient->emptyRequest());
      request["method"] = QLatin1String("internalPing");
      connect(m_pingClient, &MoleQueue::JsonRpcClient::resultReceived,
              this, &RpcListener::receivePingResponse);
      result = m_pingClient->sendRequest(request);
    }

    // If any of the above failed, trigger a failure now:
    if (!result)
      receivePingResponse();
    else // Otherwise wait 200 ms
      QTimer::singleShot(200, this, SLOT(receivePingResponse()));
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

void RpcListener::messageReceived(const MoleQueue::Message& message)
{
  QString method = message.method();
  QJsonObject params = message.params().toObject();

  if (method == "openFile") {
    if (m_window) {
      // Read the supplied file.
      string fileName = params["fileName"].toString().toStdString();
      Molecule* molecule = new Molecule(this);
      bool success =
        FileFormatManager::instance().readFile(*molecule, fileName);
      if (success) {
        emit callSetMolecule(molecule);

        // set response
        MoleQueue::Message response = message.generateResponse();
        response.setResult(true);
        response.send();
      } else {
        delete molecule;

        // send error response
        MoleQueue::Message errorMessage = message.generateErrorResponse();
        errorMessage.setErrorCode(-1);
        errorMessage.setErrorMessage(
          QString("Failed to read file: %1")
            .arg(
              QString::fromStdString(FileFormatManager::instance().error())));
        errorMessage.send();
      }
    } else {
      // send error response
      MoleQueue::Message errorMessage = message.generateErrorResponse();
      errorMessage.setErrorCode(-1);
      errorMessage.setErrorMessage("No Active Avogadro Window");
      errorMessage.send();
    }
  } else if (method == "loadMolecule") {
    if (m_window) {
      // get molecule data and format
      string content = params["content"].toString().toStdString();
      string format = params["format"].toString().toStdString();

      // read molecule data
      Molecule* molecule = new Molecule(this);
      bool success =
        FileFormatManager::instance().readString(*molecule, content, format);
      if (success) {
        emit callSetMolecule(molecule);

        // send response
        MoleQueue::Message response = message.generateResponse();
        response.setResult(true);
        response.send();
      } else {
        delete molecule;

        // send error response
        MoleQueue::Message errorMessage = message.generateErrorResponse();
        errorMessage.setErrorCode(-1);
        errorMessage.setErrorMessage(
          QString("Failed to read Chemical JSON: %1")
            .arg(
              QString::fromStdString(FileFormatManager::instance().error())));
        errorMessage.send();
      }
    } else {
      // send error response
      MoleQueue::Message errorMessage = message.generateErrorResponse();
      errorMessage.setErrorCode(-1);
      errorMessage.setErrorMessage("No Active Avogadro Window");
      errorMessage.send();
    }
  } else if (method == "kill") {
    // Only allow avogadro to be killed through RPC if it was started with the
    // '--testing' flag.
    if (qApp->arguments().contains("--testing")) {
      MoleQueue::Message response = message.generateResponse();
      response.setResult(true);
      response.send();

      qApp->quit();
    } else {
      // send error response
      MoleQueue::Message errorMessage = message.generateErrorResponse();
      errorMessage.setErrorCode(-1);
      errorMessage.setErrorMessage(
        "Ignoring kill command. Start with '--testing' to enable.");
      errorMessage.send();
    }
  } else {
    MoleQueue::Message errorMessage = message.generateErrorResponse();
    errorMessage.setErrorCode(-32601);
    errorMessage.setErrorMessage("Method not found");
    QJsonObject errorDataObject;
    errorDataObject.insert("request", message.toJsonObject());
    errorMessage.setErrorData(errorDataObject);
    errorMessage.send();
  }
}

} // End of Avogadro namespace
