/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#include "exportcopy.h"

#include <avogadro/core/layermanager.h>
#include <avogadro/core/molecule.h>
#include <avogadro/core/moleculeinfo.h>

#include <algorithm>
#include <cctype>
#include <utility>
#include <vector>

namespace Avogadro {

namespace {

std::vector<bool> hiddenMask(const Core::Molecule& molecule)
{
  const Index count = molecule.atomCount();
  std::vector<bool> mask(count, false);
  const auto info = Core::LayerManager::getMoleculeInfo(&molecule);
  if (!info)
    return mask;
  for (Index i = 0; i < count; ++i) {
    const size_t layer = info->layer.getLayerID(i);
    mask[i] = layer < info->visible.size() && !info->visible[layer];
  }
  return mask;
}

} // namespace

bool hasHiddenAtoms(const Core::Molecule& molecule)
{
  const std::vector<bool> mask = hiddenMask(molecule);
  return std::find(mask.begin(), mask.end(), true) != mask.end();
}

std::unique_ptr<Core::Molecule> copyWithoutHiddenAtoms(
  const Core::Molecule& molecule)
{
  auto result = std::make_unique<Core::Molecule>(molecule);
  const Index count = molecule.atomCount();
  const std::vector<bool> hidden = hiddenMask(molecule);

  // origin[i] is the index in `molecule` of the atom now at index i.
  std::vector<Index> origin(count);
  for (Index i = 0; i < count; ++i)
    origin[i] = i;

  // removeAtom() swaps the last atom into the hole, so go from the back.
  bool any = false;
  for (Index i = count; i > 0; --i) {
    const Index index = i - 1;
    if (!hidden[index] || index >= origin.size())
      continue;
    if (!result->removeAtom(index))
      continue;
    any = true;
    origin[index] = origin.back();
    origin.pop_back();
  }

  if (any) {
    // Restore the original order of the survivors.
    std::vector<Index> sorted(origin);
    std::sort(sorted.begin(), sorted.end());
    std::vector<Index> position(count, MaxIndex);
    for (Index i = 0; i < origin.size(); ++i)
      position[origin[i]] = i;
    for (Index i = 0; i < sorted.size(); ++i) {
      if (origin[i] == sorted[i])
        continue;
      const Index j = position[sorted[i]];
      if (j >= origin.size())
        continue;
      result->swapAtom(i, j);
      position[origin[i]] = j;
      position[origin[j]] = i;
      std::swap(origin[i], origin[j]);
    }
  }
  return result;
}

bool isNativeFormat(const std::string& format)
{
  std::string lower(format);
  std::transform(lower.begin(), lower.end(), lower.begin(), [](char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  });
  return lower == "cjson" || lower == "avogadro: cjson";
}

} // namespace Avogadro
