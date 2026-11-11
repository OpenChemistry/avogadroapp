/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#ifndef AVOGADRO_REDUCEMOTION_H
#define AVOGADRO_REDUCEMOTION_H

namespace Avogadro {

/**
 * Whether the operating system asks applications to reduce motion.
 *
 * macOS and Windows have a setting; Linux has no portable query without
 * QtDBus or GIO, so it reports false there (use the setting below).
 * The macOS implementation is in mac.mm.
 */
bool systemPrefersReducedMotion();

/**
 * Whether animations should be skipped. The QSettings key
 * "accessibility/reduceMotion" overrides the system when it is set to true or
 * false; when it is missing the system setting is followed. The settings are
 * read on every call, so a change takes effect at the next highlight.
 */
bool reduceMotionEnabled();

} // namespace Avogadro

#endif // AVOGADRO_REDUCEMOTION_H
