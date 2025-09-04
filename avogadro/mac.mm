/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#import <AppKit/NSWindow.h>

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
