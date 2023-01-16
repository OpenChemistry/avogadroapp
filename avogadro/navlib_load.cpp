/*-----------------------------------------------------------------------------
*
* navlib_load.cpp -- navlib library interface.
*
*
* The module loads the navlib dynamic link library at startup.
*
*
* Copyright (c) 2017-2021 3Dconnexion. All rights reserved.
* Permission to use, copy, modify, and distribute this software for all
* purposes and without fees is hereby granted provided that this copyright
* notice appears in all copies.  Permission to modify this software is granted
* and 3Dconnexion will support such modifications only if said modifications are
* approved by 3Dconnexion.
*
*/
extern "C" {
  extern long NlLoadLibrary();
  long NlErrorCode = NlLoadLibrary();
}
