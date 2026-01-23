/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").

  Adapted from MoleQueue. Original copyright:
  Copyright 2012 Kitware, Inc.
******************************************************************************/

#ifndef AVOGADRO_RPC_CONNECTIONLISTENER_H
#define AVOGADRO_RPC_CONNECTIONLISTENER_H

#include "connection.h"

#include <QtCore/QMetaType>
#include <QtCore/QObject>

namespace Avogadro::RPC {

/**
 * @class ConnectionListener connectionlistener.h
 * <avogadro/rpc/connectionlistener.h>
 * @brief The ConnectionListener class is an interface defining a listener
 * waiting for connections to a server. Implementations should emit the
 * newConnection() signal. Subclasses provide concrete implementations, for
 * example based on local sockets @see LocalSocketConnectionListener
 */
class ConnectionListener : public QObject
{
  Q_OBJECT
  Q_ENUMS(Error)
public:
  explicit ConnectionListener(QObject* parentObject = nullptr)
    : QObject(parentObject)
  {
  }

  /// Start the connection listener, start listening for incoming connections.
  virtual void start() = 0;

  /**
   * Stop the connection listener.
   * @param force if true, "extreme" measures may be taken to stop the listener.
   */
  virtual void stop(bool force) = 0;

  /// Stop the connection listener without forcing it, equivalent to stop(false)
  virtual void stop() = 0;

  /// @return the "address" the listener will listen on.
  virtual QString connectionString() const = 0;

  /// Defines the errors that will be emitted by connectionError()
  enum Error
  {
    AddressInUseError,
    UnknownError = -1
  };

signals:
  /**
   * Emitted when a new connection is received. The new connection is only
   * valid for the lifetime of the connection listener instance that emitted it.
   */
  void newConnection(Avogadro::RPC::Connection* connection);

  /**
   * Emitted when an error occurs.
   * @param errorCode The error code @see Error
   * @param message The error message provided by the implementation.
   */
  void connectionError(Avogadro::RPC::ConnectionListener::Error errorCode,
                       const QString& message);
};

} // namespace Avogadro::RPC

Q_DECLARE_METATYPE(Avogadro::RPC::ConnectionListener::Error)

#endif // AVOGADRO_RPC_CONNECTIONLISTENER_H
