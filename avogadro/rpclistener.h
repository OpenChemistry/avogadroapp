/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#ifndef AVOGADRO_RPCLISTENER_H
#define AVOGADRO_RPCLISTENER_H

#include <QtCore/QHash>
#include <QtCore/QJsonObject>
#include <QtCore/QObject>
#include <QtCore/QPointer>

#include "rpc/connectionlistener.h"
#include "rpc/message.h"

class QTimer;

namespace Avogadro::RPC {
class Connection;
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
  ~RpcListener() override;

  void start();

signals:
  /**
   * Calls the MainWidow::setMolecule() method with @p molecule.
   */
  void callSetMolecule(QtGui::Molecule* molecule);

private:
  /**
   * A request whose reply is being held until the command finishes.
   */
  struct PendingCommand
  {
    RPC::Message request;
    QPointer<RPC::Connection> connection;
    QTimer* timer = nullptr;
  };

  // These are connected using new-style connect, no need for slots keyword
  void connectionError(RPC::ConnectionListener::Error, const QString&);
  void receivePingResponse(const QJsonObject& response = QJsonObject());
  void messageReceived(const RPC::Message& message);

  /**
   * Hold @p message until the command identified by @p token completes.
   * @param timeoutSeconds How long to wait before giving up on the plugin.
   */
  void holdReply(const RPC::Message& message, quint64 token,
                 int timeoutSeconds);

  /** Answer a held request, once. */
  void resolvePending(quint64 token, bool success, const QString& message,
                      const QVariantMap& result);

  /** Fail every held request, e.g. when the application is closing. */
  void failAllPending(const QString& reason);

  /** Send a completed response for @p request. */
  static void sendSuccess(const RPC::Message& request, const QString& message,
                          const QVariantMap& result);

  /** Send an error response for @p request. */
  static void sendError(const RPC::Message& request, int code,
                        const QString& message);

  RPC::JsonRpc* m_rpc;
  RPC::ConnectionListener* m_connectionListener;
  MainWindow* m_window;
  RPC::JsonRpcClient* m_pingClient;
  QHash<quint64, PendingCommand> m_pending;
  quint64 m_nextToken = 0;
};

} // End Avogadro namespace

#endif
