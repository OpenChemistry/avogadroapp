/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#ifndef AVOGADRO_I18N_H
#define AVOGADRO_I18N_H

#include <QtGui/QFontDatabase>

#include <QtCore/QDir>
#include <QtCore/QList>
#include <QtCore/QLocale>
#include <QtCore/QStringList>
#include <QtCore/QTranslator>

#include <optional>
#include <utility>

namespace Avogadro {

/**
 * @brief Loads the best-match translation file and installs the translator.
 *
 * Returns the locale code of the installed file, if one was found and
 * successfully installed, or an empty string otherwise.
 *
 * If `localeCode` is not an empty string, first attempts to install a file that
 * matches the exact `localeCode` (with the name `filename + "-" + localeCode`).
 * If this is unsuccessful, or if `localeCode` is an empty string, `locale` is
 * used as the basis for resolving to a best-match translation.
 *
 */
QString loadBestMatchTranslation(QTranslator* translator,
                                 const QString& localeCode,
                                 const QLocale& locale, const QString& filename,
                                 const QString& prefix,
                                 const QStringList& translationPaths);

/**
 * @brief Collects possible locations of translation files for:
 * 1) avogadroapp; 2) avogadrolibs. The existence or contents of directories is
 * not checked.
 */
std::pair<QStringList, QStringList> translationPaths();

/**
 * @brief Assembles a list of locales for which translation files are available
 * (for avogadroapp), as strings.
 * The strings are the locale as indicated in the filenames, not the tag listed
 * in the file itself. The `QLocale` that a string resolves to may differ
 * slightly.
 */
QStringList availableLocales();

/**
 * @brief Converts a QLocale::Script to the corresponding
 * QFontDatabase::WritingSystem if there is one.
 *
 * All of the variants of QFontDatabase::WritingSystem can be obtained from at
 * least one QLocale::Script variant, other than Vietnamese (which uses a
 * Latin-based script and therefore has no direct QLocale::Script equivalent),
 * Any, Symbol, and Other.
 */
std::optional<QFontDatabase::WritingSystem> scriptToWritingSystem(
  QLocale::Script script);

} // namespace Avogadro

#endif // AVOGADRO_I18N_H
