/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#ifndef AVOGADRO_RPCLISTENER_H
#define AVOGADRO_RPCLISTENER_H

#include <QtCore/QJsonObject>
#include <QtCore/QObject>

#include <molequeue/servercore/connectionlistener.h>

namespace MoleQueue {
class JsonRpc;
class JsonRpcClient;
class Message;
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
  explicit RpcListener(QObject* parent = 0);
  ~RpcListener();

  void start();

signals:
  /**
   * Calls the MainWidow::setMolecule() method with @p molecule.
   */
  void callSetMolecule(Avogadro::QtGui::Molecule* molecule);

private slots:
  void connectionError(MoleQueue::ConnectionListener::Error, const QString&);
  void receivePingResponse(const QJsonObject& response = QJsonObject());
  void messageReceived(const MoleQueue::Message& message);

private:
  MoleQueue::JsonRpc* m_rpc;
  MoleQueue::ConnectionListener* m_connectionListener;
  MainWindow* m_window;
  MoleQueue::JsonRpcClient* m_pingClient;
};

} // End Avogadro namespace

#endif
