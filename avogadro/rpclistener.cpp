/******************************************************************************

  This source file is part of the Avogadro project.

  Copyright 2012-2013 Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/

#include "rpclistener.h"
#include "mainwindow.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QInputDialog>

#include <QtCore/QTimer>
#include <QtCore/QJsonValue>

#include <avogadro/qtgui/molecule.h>
#include <avogadro/io/fileformatmanager.h>

#include <molequeue/client/jsonrpcclient.h>
#include <molequeue/servercore/jsonrpc.h>
#include <molequeue/servercore/localsocketconnectionlistener.h>

namespace Avogadro {

using std::string;
using Io::FileFormatManager;
using QtGui::Molecule;

RpcListener::RpcListener(QObject *parent_)
  : QObject(parent_),
    m_pingClient(nullptr)
{
  m_rpc = new MoleQueue::JsonRpc(this);

  m_connectionListener =
    new MoleQueue::LocalSocketConnectionListener(this, "avogadro");

  connect(m_connectionListener,
          SIGNAL(connectionError(MoleQueue::ConnectionListener::Error,QString)),
          SLOT(connectionError(MoleQueue::ConnectionListener::Error,QString)));

  m_rpc->addConnectionListener(m_connectionListener);

  connect(m_rpc, SIGNAL(messageReceived(const MoleQueue::Message&)),
          this, SLOT(messageReceived(const MoleQueue::Message&)));

  // Find the main window.
  m_window = 0;
  foreach (QWidget *widget, QApplication::topLevelWidgets())
    if ((m_window = qobject_cast<MainWindow *>(widget)))
      break;

  if (m_window) {
    connect(this, SIGNAL(callSetMolecule(Avogadro::QtGui::Molecule*)),
            m_window, SLOT(setMolecule(Avogadro::QtGui::Molecule*)));
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
                                  const QString &message)
{
  qDebug() << "Error starting RPC server:" << message;
  if (error == MoleQueue::ConnectionListener::AddressInUseError) {
    // Try to ping the existing server to see if it is alive:
    if (!m_pingClient)
      m_pingClient = new MoleQueue::JsonRpcClient(this);
    bool result(m_pingClient->connectToServer(
                  m_connectionListener->connectionString()));

    if (result) {
      QJsonObject request(m_pingClient->emptyRequest());
      request["method"] = QLatin1String("internalPing");
      connect(m_pingClient, SIGNAL(resultReceived(QJsonObject)),
              SLOT(receivePingResponse(QJsonObject)));
      result = m_pingClient->sendRequest(request);
    }

    // If any of the above failed, trigger a failure now:
    if (!result)
      receivePingResponse();
    else // Otherwise wait 200 ms
      QTimer::singleShot(200, this, SLOT(receivePingResponse()));
  }
}

void RpcListener::receivePingResponse(const QJsonObject &response)
{
  // Disconnect and remove the ping client the first time this is called:
  if (m_pingClient) {
    m_pingClient->deleteLater();
    m_pingClient = nullptr;
  }
  else {
    // In case the single shot timeout is triggered after the slot is called
    // directly or in response to m_pingClient's signal.
    return;
  }

  bool pingSuccessful = response.value("result").toString() == QString("pong");
  if (pingSuccessful) {
    qDebug() << "Other server is alive. Not starting new instance.";
  }
  else {
    QString title(tr("Error starting RPC server:"));
    QString label(
          tr("An error occurred while starting Avogadro's RPC listener. "
             "This may be happen for a\nnumber of reasons:\n\t"
             "A previous instance of Avogadro may have crashed.\n\t"
             "A running Avogadro instance was too busy to respond.\n\n"
             "If no other Avogadro instance is running on this machine, it "
             "is safe to replace the dead\nserver. "
             "Otherwise, this instance of avogadro may be started without "
             "RPC capabilities\n(this will prevent RPC enabled applications "
             "from communicating with Avogadro)."));
    QStringList items;
    items << tr("Replace the dead server with a new instance.");
    items << tr("Start without RPC capabilities.");
    bool ok(false);
    QString item(QInputDialog::getItem(nullptr, title, label, items, 0, false,
                                       &ok));

    if (ok && item == items.first()) {
      qDebug() << "Starting new server.";
      m_connectionListener->stop(true);
      m_connectionListener->start();
    }
    else {
      qDebug() << "Starting without RPC capabilities.";
    }
  }
}

void RpcListener::messageReceived(const MoleQueue::Message &message)
{
  QString method = message.method();
  QJsonObject params = message.params().toObject();

  if (method == "openFile") {
    if (m_window) {
      // Read the supplied file.
      string fileName = params["fileName"].toString().toStdString();
      Molecule *molecule = new Molecule(this);
      bool success =
          FileFormatManager::instance().readFile(*molecule, fileName);
      if (success) {
        emit callSetMolecule(molecule);

        // set response
        MoleQueue::Message response = message.generateResponse();
        response.setResult(true);
        response.send();
      }
      else {
        delete molecule;

        // send error response
        MoleQueue::Message errorMessage = message.generateErrorResponse();
        errorMessage.setErrorCode(-1);
        errorMessage.setErrorMessage(QString("Failed to read file: %1")
                                     .arg(QString::fromStdString(
                                            FileFormatManager::instance().error())));
        errorMessage.send();
      }
    }
    else {
      // send error response
      MoleQueue::Message errorMessage = message.generateErrorResponse();
      errorMessage.setErrorCode(-1);
      errorMessage.setErrorMessage("No Active Avogadro Window");
      errorMessage.send();
    }
  }
  else if (method == "loadMolecule") {
    if (m_window) {
      // get molecule data and format
      string content = params["content"].toString().toStdString();
      string format = params["format"].toString().toStdString();

      // read molecule data
      Molecule *molecule = new Molecule(this);
      bool success = FileFormatManager::instance().readString(*molecule,
                                                              content, format);
      if (success) {
        emit callSetMolecule(molecule);

        // send response
        MoleQueue::Message response = message.generateResponse();
        response.setResult(true);
        response.send();
      }
      else {
        delete molecule;

        // send error response
        MoleQueue::Message errorMessage = message.generateErrorResponse();
        errorMessage.setErrorCode(-1);
        errorMessage.setErrorMessage(QString("Failed to read Chemical JSON: %1")
                                     .arg(QString::fromStdString(
                                            FileFormatManager::instance().error())));
        errorMessage.send();
      }
    }
    else {
      // send error response
      MoleQueue::Message errorMessage = message.generateErrorResponse();
      errorMessage.setErrorCode(-1);
      errorMessage.setErrorMessage("No Active Avogadro Window");
      errorMessage.send();
    }
  }
  else if (method == "kill") {
    // Only allow avogadro to be killed through RPC if it was started with the
    // '--testing' flag.
    if (qApp->arguments().contains("--testing")) {
      MoleQueue::Message response = message.generateResponse();
      response.setResult(true);
      response.send();

      qApp->quit();
    }
    else {
      // send error response
      MoleQueue::Message errorMessage = message.generateErrorResponse();
      errorMessage.setErrorCode(-1);
      errorMessage.setErrorMessage(
        "Ignoring kill command. Start with '--testing' to enable.");
      errorMessage.send();
    }
  }
  else {
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
