/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").

  Adapted from MoleQueue. Original copyright:
  Copyright 2012 Kitware, Inc.
******************************************************************************/

#ifndef AVOGADRO_RPC_MESSAGEIDMANAGER_P_H
#define AVOGADRO_RPC_MESSAGEIDMANAGER_P_H

#include "rpcglobal.h"

#include <QtCore/QMap>
#include <QtCore/QString>

namespace Avogadro::RPC {

/**
 * @brief The MessageIdManager class provides a static lookup table that is used
 * to identify replies to JSON-RPC requests.
 */
class MessageIdManager
{
public:
  /**
   * @brief Request a new message id that is associated with @a method.
   * The new id and method will be registered in the lookup table.
   * @return The assigned message id.
   */
  static MessageIdType registerMethod(const QString& method);

  /**
   * @brief Determine the method associated with the @a id.
   * @note This removes the id from the internal lookup table.
   * @return The method associated with the given id.
   */
  static QString lookupMethod(const MessageIdType& id);

private:
  MessageIdManager();
  static void init();
  static void cleanup();

  static MessageIdManager* m_instance;
  QMap<double, QString> m_lookup;
  double m_generator;
};

} // namespace Avogadro::RPC

#endif // AVOGADRO_RPC_MESSAGEIDMANAGER_P_H
