/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#import <AppKit/NSWindow.h>
#import <AppKit/AppKit.h>

// For systemPrefersReducedMotion(), defined at the end of this file.
#include "reducemotion.h"

// Set some Mac compatibility bits using Objective-C++
// From https://forum.qt.io/topic/60623/qt-5-4-2-os-x-10-11-el-capitan-how-to-remove-the-enter-full-screen-menu-item/
// From https://github.com/opencor/opencor/blob/master/src/misc/macos.mm
void removeMacSpecificMenuItems() {
  // Remove (disable) the "Start Dictation..." and "Emoji & Symbols" menu items
  // from the "Edit" menu
  [[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"NSDisabledDictationMenuItem"];
  [[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"NSDisabledCharacterPaletteMenuItem"];

#ifdef AVAILABLE_MAC_OS_X_VERSION_10_12_AND_LATER
  if ([NSWindow respondsToSelector:@selector(allowsAutomaticWindowTabbing)])
    NSWindow.allowsAutomaticWindowTabbing = NO;
#endif

  return;
}

namespace Avogadro {

// System Settings > Accessibility > Display > Reduce motion
// The cross-platform declaration is in reducemotion.h; the other platforms are
// in reducemotion.cpp. The ternary turns the Objective-C BOOL into a C++ bool.
bool systemPrefersReducedMotion()
{
  return [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldReduceMotion]
           ? true
           : false;
}

} // namespace Avogadro
