/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#include "i18n.h"

#include <QtWidgets/QApplication>

#include <QtGui/QFontDatabase>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QtEnvironmentVariables>
#include <QtCore/QFileInfo>
#include <QtCore/QLatin1StringView>
#include <QtCore/QList>
#include <QtCore/QProcess>
#include <QtCore/QTranslator>
#include <QtCore/QStandardPaths>
#include <QtCore/QString>

#include <optional>
#include <utility>

using namespace Qt::StringLiterals;

namespace Avogadro {

QString loadBestMatchTranslation(QTranslator* translator,
                                 const QString& localeCode,
                                 const QLocale& locale, const QString& filename,
                                 const QString& prefix,
                                 const QStringList& translationPaths)
{
  QCoreApplication* app = QCoreApplication::instance();
  bool loaded = false;
  QString translationLocaleCode;
  // Try to find a translation file that corresponds to the exact locale code,
  // if one was provided
  if (!loaded && !localeCode.isEmpty()) {
    for (const QString& translationPath : translationPaths) {
      if (!loaded &&
          // Don't allow stripping of parts, we want only an exact match here
          translator->load(filename + prefix + localeCode, translationPath)) {
        if (app->installTranslator(translator)) {
          loaded = true;
          translationLocaleCode = localeCode;
          qDebug() << "Translation (exact match for configured locale code):"
                   << translationLocaleCode << "loaded from" << translationPath;
        }
      }
    }
  }
  // If that fails, try to resolve to a translation file based on the locale
  if (!loaded) {
    for (const QString& translationPath : translationPaths) {
      if (!loaded &&
          translator->load(locale, filename, prefix, translationPath)) {
        if (app->installTranslator(translator)) {
          loaded = true;
          // Get the actual locale code of the loaded file (e.g., id, pt_BR,
          // ca@valencia), not the locale we started from, and not the one
          // stored in the .qm metadata, which can differ (ar -> ar_EG,
          // ca@valencia -> ca_ES)
          QString translationBaseName =
            QFileInfo(translator->filePath()).completeBaseName();
          if (translationBaseName.startsWith(filename)) {
            translationLocaleCode =
              translationBaseName.mid((filename + prefix).length());
          } else {
            translationLocaleCode = translationBaseName;
          }
          qDebug() << "Translation (best match for locale):"
                   << translationLocaleCode << "loaded from" << translationPath;
        }
      }
    }
  }
  return translationLocaleCode;
}

std::pair<QStringList, QStringList> translationPaths()
{
  // Paths are mostly specific to each component, though some might have
  // translations for either
  QStringList appPaths;
  QStringList libsPaths;

  // Environment variable takes priority
  const QStringList envVarPaths =
    qEnvironmentVariable("AVOGADRO_TRANSLATIONS")
      .split(QDir::listSeparator(), Qt::SkipEmptyParts);
  appPaths << envVarPaths;
  libsPaths << envVarPaths;

  // Next look in the user's data directory
  QStringList stdPaths =
    QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation);
  foreach (const QString& dirStr, stdPaths) {
    appPaths << (dirStr + "/i18n/avogadro-i18n/avogadroapp"_L1);
    libsPaths << (dirStr + "/i18n/avogadro-i18n/avogadrolibs"_L1);
  }

  // Otherwise we can expect them to be installed next to the binary,
  // in the default location for application files on most systems e.g.
  // Linux: /usr/bin/avogadro2 -> /usr/share/avogadro2/i18n/
  // macOS: -> /Applications/Avogadro2.app/Contents/share/avogadro2/i18n/
  QString path =
    QCoreApplication::applicationDirPath() + "/../share/avogadro2/i18n/"_L1;
  appPaths << path;
  libsPaths << path;

  return { std::move(appPaths), std::move(libsPaths) };
}

QStringList availableLocales()
{
  QStringList available;

  auto appPaths = translationPaths().first;
  foreach (const QString& dirStr, appPaths) {
    QDir dir(dirStr);
    if (!dir.exists())
      continue;
    qDebug() << "Looking for app translation files in" << dir.absolutePath();
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList("avogadroapp-*.qm"_L1));
    foreach (const QString& file, dir.entryList()) {
      // remove "avogadroapp-" and the ".qm"
      QString localeCode =
        file.left(file.indexOf(u'.')).remove("avogadroapp-"_L1);
      if (!available.contains(localeCode)) {
        qDebug() << "Found translation file for" << localeCode << ":" << file;
        available << localeCode;
      }
    }
  }
  return available;
}

std::optional<QFontDatabase::WritingSystem> scriptToWritingSystem(
  QLocale::Script script)
{
  switch (script) {
    case QLocale::LatinScript:
      return QFontDatabase::Latin;
    case QLocale::GreekScript:
      return QFontDatabase::Greek;
    case QLocale::CyrillicScript:
      return QFontDatabase::Cyrillic;
    case QLocale::ArmenianScript:
      return QFontDatabase::Armenian;
    case QLocale::HebrewScript:
      return QFontDatabase::Hebrew;
    case QLocale::ArabicScript:
      return QFontDatabase::Arabic;
    case QLocale::SyriacScript:
      return QFontDatabase::Syriac;
    case QLocale::ThaanaScript:
      return QFontDatabase::Thaana;
    case QLocale::DevanagariScript:
      return QFontDatabase::Devanagari;
    case QLocale::BanglaScript:
      return QFontDatabase::Bengali;
    case QLocale::GurmukhiScript:
      return QFontDatabase::Gurmukhi;
    case QLocale::GujaratiScript:
      return QFontDatabase::Gujarati;
    case QLocale::OdiaScript:
      return QFontDatabase::Oriya;
    case QLocale::TamilScript:
      return QFontDatabase::Tamil;
    case QLocale::TeluguScript:
      return QFontDatabase::Telugu;
    case QLocale::KannadaScript:
      return QFontDatabase::Kannada;
    case QLocale::MalayalamScript:
      return QFontDatabase::Malayalam;
    case QLocale::SinhalaScript:
      return QFontDatabase::Sinhala;
    case QLocale::ThaiScript:
      return QFontDatabase::Thai;
    case QLocale::LaoScript:
      return QFontDatabase::Lao;
    case QLocale::TibetanScript:
      return QFontDatabase::Tibetan;
    case QLocale::MyanmarScript:
      return QFontDatabase::Myanmar;
    case QLocale::GeorgianScript:
      return QFontDatabase::Georgian;
    case QLocale::KhmerScript:
      return QFontDatabase::Khmer;
    case QLocale::SimplifiedHanScript:
      return QFontDatabase::SimplifiedChinese;
    case QLocale::TraditionalHanScript:
      return QFontDatabase::TraditionalChinese;
    case QLocale::JapaneseScript:
      return QFontDatabase::Japanese;
    case QLocale::KoreanScript:
      return QFontDatabase::Korean;
    case QLocale::HangulScript:
      return QFontDatabase::Korean;
    case QLocale::OghamScript:
      return QFontDatabase::Ogham;
    case QLocale::RunicScript:
      return QFontDatabase::Runic;
    case QLocale::NkoScript:
      return QFontDatabase::Nko;
    default:
      return std::nullopt;
  }
}

} // namespace Avogadro
