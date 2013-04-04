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

#ifndef AVOGADRO_RPCLISTENER_H
#define AVOGADRO_RPCLISTENER_H

#include <QObject>

#include <qjsonobject.h>

#include <molequeue/transport/connectionlistener.h>

namespace MoleQueue {
class JsonRpc;
class JsonRpcClient;
}

namespace Avogadro {

namespace QtGui {
class Molecule;
}

class MainWindow;

class RpcListener : public QObject
{
  Q_OBJECT

public:
  explicit RpcListener(QObject *parent = 0);
  ~RpcListener();

  void start();

signals:
  /**
   * Calls the MainWidow::setMolecule() method with @p molecule.
   */
  void callSetMolecule(Avogadro::QtGui::Molecule *molecule);

private slots:
  void connectionError(MoleQueue::ConnectionListener::Error, const QString &);
  void receivePingResponse(const QJsonObject &response = QJsonObject());
  void messageReceived(const MoleQueue::Message &message);

private:
  MoleQueue::JsonRpc *m_rpc;
  MoleQueue::ConnectionListener *m_connectionListener;
  MainWindow *m_window;
  MoleQueue::JsonRpcClient *m_pingClient;
};

} // End Avogadro namespace

#endif
