/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").

  Adapted from MoleQueue. Original copyright:
  Copyright 2012 Kitware, Inc.
******************************************************************************/

#include "connectionlistener.h"

// This file exists to allow AUTOMOC to generate the moc file for the
// ConnectionListener Q_OBJECT class. The class itself is header-only
// but needs moc processing for signals/slots to work.
