/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#include <QtWidgets/QApplication>

#include <QtGui/QFontDatabase>

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QProcess>
#include <QtCore/QStandardPaths>

namespace Avogadro {

std::pair<QStringList, QStringList> translationPaths()
{
  // Paths are mostly specific to each component, though some might have
  // translations for either
  QStringList appPaths;
  QStringList libsPaths;

  // Environment variable takes priority
  foreach (const QString& variable, QProcess::systemEnvironment()) {
    QStringList split1 = variable.split('=');
    if (split1[0] == "AVOGADRO_TRANSLATIONS") {
      foreach (const QString& path, split1[1].split(':')) {
        appPaths << path;
        libsPaths << path;
      }
    }
  }

  // Next look in the user's data directory
  QStringList stdPaths =
    QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation);
  foreach (const QString& dirStr, stdPaths) {
    appPaths << (dirStr + "/i18n/avogadro-i18n/avogadroapp");
    libsPaths << (dirStr + "/i18n/avogadro-i18n/avogadrolibs");
  }

  // Otherwise we can expect them to be installed next to the binary,
  // in the default location for application files on most systems e.g.
  // Linux: /usr/bin/avogadro2 -> /usr/share/avogadro2/i18n/
  // macOS: -> /Applications/Avogadro2.app/Contents/share/avogadro2/i18n/
  QString path =
    QCoreApplication::applicationDirPath() + "/../share/avogadro2/i18n/";
  appPaths << path;
  libsPaths << path;

  return { std::move(appPaths), std::move(libsPaths) };
}

std::pair<QList<QDir>, QList<QDir>> translationDirs()
{
  auto [appPaths, libsPaths] = translationPaths();

  // Check if paths exist and contain translation files
  QList<QDir> appDirs;
  QList<QDir> libsDirs;

  auto hasQmFiles = [](QDir dir) {
    if (!dir.exists())
      return false;
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList("*.qm"));
    // count() is equivalent to entryList().count() so filters are applied
    return (dir.entryList().count() > 0);
  };

  foreach (const QString& dirStr, appPaths) {
    QDir dir(dirStr);
    if (hasQmFiles(dir)) {
      appDirs << dir;
    }
  }

  foreach (const QString& dirStr, libsPaths) {
    QDir dir(dirStr);
    if (hasQmFiles(dir)) {
      libsDirs << dir;
    }
  }

  return { std::move(appDirs), std::move(libsDirs) };
}

QStringList availableLocales()
{
  QStringList available;

  auto [appPaths, _libsPaths] = translationPaths();
  foreach (const QString& dirStr, appPaths) {
    QDir dir(dirStr);
    if (!dir.exists())
      continue;
    qDebug() << "Looking for app translation files in" << dir.absolutePath();
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList("avogadroapp-*.qm"));
    foreach (const QString& file, dir.entryList()) {
      // remove "avogadroapp-" and the ".qm"
      QString localeCode = file.left(file.indexOf('.')).remove("avogadroapp-");
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
