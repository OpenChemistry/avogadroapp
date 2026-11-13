/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#include "reducemotion.h"

#include <QtCore/QSettings>
#include <QtCore/QVariant>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace Avogadro {

#if defined(Q_OS_WIN)
bool systemPrefersReducedMotion()
{
  // SPI_GETCLIENTAREAANIMATION is true when animations are wanted (the
  // "Animation effects" switch in Settings > Accessibility > Visual effects),
  // so reduce motion is its inverse. If the query fails, assume no
  // preference.
  BOOL animations = TRUE;
  if (!SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0))
    return false;
  return animations == FALSE;
}
#elif !defined(Q_OS_MAC)
bool systemPrefersReducedMotion()
{
  // No portable query on Linux and the BSDs.
  return false;
}
#endif // macOS: see mac.mm

bool reduceMotionEnabled()
{
  // A missing key gives an invalid QVariant, which means "follow the system".
  // Read every time rather than cached, so a change applies at the next
  // highlight with no signal to wire up.
  QSettings settings;
  const QVariant override = settings.value("accessibility/reduceMotion");
  if (override.isValid() && override.canConvert<bool>())
    return override.toBool();
  return systemPrefersReducedMotion();
}

} // namespace Avogadro
