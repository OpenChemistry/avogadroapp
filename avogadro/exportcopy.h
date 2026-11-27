/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#ifndef AVOGADRO_EXPORTCOPY_H
#define AVOGADRO_EXPORTCOPY_H

#include <memory>
#include <string>

namespace Avogadro {

namespace Core {
class Molecule;
}

/**
 * Atoms on a hidden layer are written only to the native format (CJSON, which
 * stores the layers and their visibility). Every other format gets a copy of
 * the molecule without them.
 *
 * An atom is hidden if its layer id l satisfies
 * l < visible.size() && !visible[l]; an atom in no layer, or on a layer past
 * the end of the visibility list, is not hidden.
 *
 * This mirrors Core::moleculeWithoutHiddenAtoms() in avogadrolibs; once the
 * app builds against libs that have it, these can go.
 * @{
 */

/** @return true if @p molecule has at least one atom on a hidden layer. */
bool hasHiddenAtoms(const Core::Molecule& molecule);

/**
 * @return a copy of @p molecule without its hidden atoms. The remaining atoms
 * keep their relative order.
 */
std::unique_ptr<Core::Molecule> copyWithoutHiddenAtoms(
  const Core::Molecule& molecule);

/**
 * @return true if @p format (a format identifier or extension, any case) is the
 * native CJSON format, which keeps hidden atoms.
 */
bool isNativeFormat(const std::string& format);
/** @} */

} // namespace Avogadro

#endif
