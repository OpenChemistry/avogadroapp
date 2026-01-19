/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").

  Adapted from MoleQueue. Original copyright:
  Copyright 2012 Kitware, Inc.
******************************************************************************/

#include "messageidmanager_p.h"

#include "message.h"

#include <cstdlib>

namespace Avogadro::RPC {

MessageIdManager* MessageIdManager::m_instance = nullptr;

MessageIdManager::MessageIdManager()
  : m_generator(0)
{
  // Clean up when program exits.
  atexit(&cleanup);
}

void MessageIdManager::init()
{
  if (!m_instance)
    m_instance = new MessageIdManager();
}

void MessageIdManager::cleanup()
{
  delete m_instance;
  m_instance = nullptr;
}

MessageIdType MessageIdManager::registerMethod(const QString& method)
{
  init();
  double result = ++m_instance->m_generator;
  m_instance->m_lookup.insert(result, method);
  return MessageIdType(result);
}

QString MessageIdManager::lookupMethod(const MessageIdType& id)
{
  init();
  return id.isDouble() ? m_instance->m_lookup.take(id.toDouble()) : QString();
}

} // namespace Avogadro::RPC
