/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLContext>
#include <QtGui/QSurfaceFormat>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QLibraryInfo>
#include <QtCore/QLocale>
#include <QtCore/QProcess>
#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>
#include <QtCore/QTranslator>

#include "application.h"
#include "mainwindow.h"

#ifdef Q_OS_MAC
void removeMacSpecificMenuItems();
#endif

#ifdef Avogadro_ENABLE_RPC
#include "rpclistener.h"
#endif

#define DEBUG false

int main(int argc, char* argv[])
{
#ifdef Q_OS_MAC
  // call some Objective-C++
  removeMacSpecificMenuItems();
  // Native Mac applications do not have icons in the menus
  QCoreApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
#endif

  QCoreApplication::setOrganizationName("OpenChemistry");
  QCoreApplication::setOrganizationDomain("openchemistry.org");
  QCoreApplication::setApplicationName("Avogadro");

  QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
  QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

#ifdef Q_OS_WIN
  // We need to ensure desktop OpenGL is loaded for our rendering.
  QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
#endif

  Avogadro::Application app(argc, argv);

  QSettings settings;
  QString language = settings.value("language", "System").toString();

  // Before we do much else, load translations
  // This ensures help messages and debugging info can be translated
  QLocale currentLocale;
  if (language != "System") {
    currentLocale = QLocale(language);
  }
  qDebug() << "Using locale: " << currentLocale.name();

  QStringList translationPaths;
  // check environment variable and local paths
  foreach (const QString& variable, QProcess::systemEnvironment()) {
    QStringList split1 = variable.split('=');
    if (split1[0] == "AVOGADRO_TRANSLATIONS") {
      foreach (const QString& path, split1[1].split(':'))
        translationPaths << path;
    }
  }

  translationPaths << QLibraryInfo::location(QLibraryInfo::TranslationsPath);
  translationPaths << QCoreApplication::applicationDirPath() +
                        "/../share/avogadro2/i18n/";
  // add the possible plugin download paths
  QStringList stdPaths =
    QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation);
  foreach (const QString& dirStr, stdPaths) {
    QString path = dirStr + "/i18n/avogadro-i18n/avogadroapp";
    translationPaths << path; // we'll check if these exist below
    path = dirStr + "/i18n/avogadro-i18n/avogadrolibs";
    translationPaths << path;
  }

  // Make sure to use pointers:
  //
  QTranslator* qtTranslator = new QTranslator;
  QTranslator* qtBaseTranslator = new QTranslator;
  QTranslator* avoTranslator = new QTranslator;
  QTranslator* avoLibsTranslator = new QTranslator;
  bool qtLoaded = false;
  bool avoLoaded = false;
  bool libsLoaded = false;
  QString successfulPath;

  foreach (const QString& translationPath, translationPaths) {
    if (!qtLoaded &&
        qtTranslator->load(currentLocale, "qt", "_", translationPath)) {
      if (app.installTranslator(qtTranslator)) {
        qDebug() << "qt Translation successfully loaded.";
        qtLoaded = true;
      }
    }
    if (!qtLoaded &&
        qtBaseTranslator->load(currentLocale, "qtbase", "_", translationPath)) {
      if (app.installTranslator(qtBaseTranslator)) {
        qDebug() << "qtbase Translation successfully loaded.";
        qtLoaded = true;
      }
    }
    if (!avoLoaded && avoTranslator->load(currentLocale, "avogadroapp", "-",
                                          translationPath)) {
      if (app.installTranslator(avoTranslator)) {
        qDebug() << "AvogadroApp Translation " << currentLocale.name()
                 << " loaded " << translationPath;
        avoLoaded = true;
        successfulPath = translationPath;
      }
    }
    if (!libsLoaded && avoLibsTranslator->load(currentLocale, "avogadrolibs",
                                               "-", translationPath)) {
      if (app.installTranslator(avoLibsTranslator)) {
        qDebug() << "AvogadroLibs Translation " << currentLocale.name()
                 << " loaded " << translationPath;
        libsLoaded = true;
      }
    }
  } // done looking for translations

  // we'll also go through to get the localized names for the language dialog
  // we iterate through the files in the successfulPath
  QDir dir(successfulPath);
  QStringList files = dir.entryList(QStringList() << "avogadroapp*.qm", QDir::Files);
  QStringList languages, codes;

  languages << QCoreApplication::translate("main.cpp","System Language");
  codes << "";
  foreach (const QString& file, files) {
    // remove "avogadroapp-" and the ".qm"
    QString localeCode = file.left(file.indexOf('.')).remove("avogadroapp-");
    QLocale locale(localeCode);
    QString language = locale.nativeLanguageName();
    if (language.isEmpty() && localeCode == "oc")
      language = "Occitan";
    
    if (languages.contains(language)) {
      language += " (" + locale.nativeCountryName() + ")";
    }

    languages << language;
    codes << localeCode;
  }
  qDebug() << "Languages: " << languages;

  // Check for valid OpenGL support.
  auto offscreen = new QOffscreenSurface;
  offscreen->create();
  auto context = new QOpenGLContext;
  context->create();
  context->makeCurrent(offscreen);
  bool contextIsValid = context->isValid();
  delete context;
  delete offscreen;

  if (!contextIsValid) {
    QMessageBox::information(
      0, QCoreApplication::translate("main.cpp", "Avogadro"),
      QCoreApplication::translate("main.cpp",
                                  "This system does not support OpenGL."));
    return 1;
  }

  // Set up the default format for our GL contexts.
  QSurfaceFormat defaultFormat = QSurfaceFormat::defaultFormat();
  defaultFormat.setSamples(4);
#if defined(Q_OS_MAC) || defined(Q_OS_WIN)
  defaultFormat.setAlphaBufferSize(8);
#endif
  QSurfaceFormat::setDefaultFormat(defaultFormat);

  QStringList fileNames;
  bool disableSettings = false;
#ifdef QTTESTING
  QString testFile;
  bool testExit = true;
#endif
  QStringList args = QCoreApplication::arguments();
  for (QStringList::const_iterator it = args.constBegin() + 1;
       it != args.constEnd(); ++it) {
    if (*it == "--test-file" && it + 1 != args.constEnd()) {
#ifdef QTTESTING
      testFile = *(++it);
#else
      qWarning("Avogadro called with --test-file but testing is disabled.");
      return EXIT_FAILURE;
#endif
    } else if (*it == "--test-no-exit") {
#ifdef QTTESTING
      testExit = false;
#else
      qWarning("Avogadro called with --test-no-exit but testing is disabled.");
      return EXIT_FAILURE;
#endif
    } else if (*it == "--disable-settings") {
      disableSettings = true;
    } else if (it->startsWith("-")) {
      qWarning("Unknown command line option '%s'", qPrintable(*it));
      return EXIT_FAILURE;
    } else { // Assume it is a file name.
      fileNames << *it;
    }
  }

  Avogadro::MainWindow* window =
    new Avogadro::MainWindow(fileNames, disableSettings);
  window->setTranslationList(languages, codes);
#ifdef QTTESTING
  window->playTest(testFile, testExit);
#endif
  window->show();

#ifdef Avogadro_ENABLE_RPC
  // create rpc listener
  Avogadro::RpcListener listener;
  listener.start();
#endif

  return app.exec();
}
