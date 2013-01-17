/******************************************************************************

  This source file is part of the Avogadro project.

  Copyright 2012 Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/

#include "rpclistener.h"

#include <QApplication>

#include "qjsonvalue.h"
#include "qjsonobject.h"

#include "mainwindow.h"

#include <molequeue/transport/jsonrpc.h>
#include <molequeue/transport/localsocketconnectionlistener.h>

namespace Avogadro {

RpcListener::RpcListener(QObject *parent_)
  : QObject(parent_)
{
  m_rpc = new MoleQueue::JsonRpc(this);

  m_connectionListener =
    new MoleQueue::LocalSocketConnectionListener(this, "avogadro");

  m_rpc->addConnectionListener(m_connectionListener);

  connect(m_rpc, SIGNAL(messageReceived(const MoleQueue::Message&)),
          this, SLOT(messageReceived(const MoleQueue::Message&)));
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
  Q_UNUSED(error)
  qDebug() << "RpcListener connection error: " << message;
}

void RpcListener::messageReceived(const MoleQueue::Message &message)
{
  QString method = message.method();
  QJsonObject params = message.params().toObject();

  if (method == "open_file") {
    // find main window
    MainWindow *window = 0;
    foreach (QWidget *widget, QApplication::topLevelWidgets())
      if ((window = qobject_cast<MainWindow *>(widget)))
        break;

    if (window) {
      // call openFile()
      QString fileName = params["fileName"].toString();
      QMetaObject::invokeMethod(window,
                                "openFile",
                                Qt::DirectConnection,
                                Q_ARG(QString, fileName));

      // set response
      MoleQueue::Message response = message.generateResponse();
      response.setResult(true);
      response.send();
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
