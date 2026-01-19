/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#ifndef AVOGADRO_RPCLISTENER_H
#define AVOGADRO_RPCLISTENER_H

#include <QtCore/QJsonObject>
#include <QtCore/QObject>

#include "rpc/connectionlistener.h"
#include "rpc/message.h"

namespace Avogadro::RPC {
class JsonRpc;
class JsonRpcClient;
}

namespace Avogadro {

namespace QtGui {
class Molecule;
}

class MainWindow;

/**
 * @brief The RpcListener class is used to implement the remote procedure call
 * interface for the Avogadro application.
 */

class RpcListener : public QObject
{
  Q_OBJECT

public:
  explicit RpcListener(QObject* parent = nullptr);
  ~RpcListener();

  void start();

signals:
  /**
   * Calls the MainWidow::setMolecule() method with @p molecule.
   */
  void callSetMolecule(QtGui::Molecule* molecule);

private:
  // These are connected using new-style connect, no need for slots keyword
  void connectionError(RPC::ConnectionListener::Error, const QString&);
  void receivePingResponse(const QJsonObject& response = QJsonObject());
  void messageReceived(const RPC::Message& message);
  RPC::JsonRpc* m_rpc;
  RPC::ConnectionListener* m_connectionListener;
  MainWindow* m_window;
  RPC::JsonRpcClient* m_pingClient;
};

} // End Avogadro namespace

#endif
