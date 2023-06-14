/******************************************************************************
  This source file is part of the Avogadro project.

  Copyright (c) 2014-2023 3Dconnexion.

  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

extern "C" {
  extern long NlLoadLibrary();
  extern const long NlErrorCode = NlLoadLibrary();
}
