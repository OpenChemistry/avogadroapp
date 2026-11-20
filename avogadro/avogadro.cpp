/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLContext>
#include <QtGui/QSurfaceFormat>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QLatin1StringView>
#include <QtCore/QLibraryInfo>
#include <QtCore/QLocale>
#include <QtCore/QOperatingSystemVersion>
#include <QtCore/QProcess>
#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>
#include <QtCore/QTranslator>
#include <QtCore/QString>

using namespace Qt::StringLiterals;

// install a message handler (for Windows)
#include <QFile>
#include <QSslSocket>
#include <QTextStream>

#include <avogadro/core/version.h>

#include "application.h"
#include "avogadroappconfig.h"
#include "crashreporter.h"
#include "i18n.h"
#include "mainwindow.h"

#ifdef Q_OS_MAC
// void removeMacSpecificMenuItems();
#endif

#ifdef Avogadro_ENABLE_RPC
#include "rpclistener.h"
#endif

#define DEBUG false

// Based on  https://doc.qt.io/qt-5/qtglobal.html#qInstallMessageHandler
void myMessageOutput(QtMsgType type, const QMessageLogContext& context,
                     const QString& msg)
{
  QByteArray localMsg = msg.toLocal8Bit();
  QString file = context.file;
  QString function = context.function;
  // get current date and time
  QDateTime dateTime = QDateTime::currentDateTime();
  QString dateTimeString = dateTime.toString("yyyy-MM-dd hh:mm:ss");

  QString message;
  switch (type) {
    case QtInfoMsg:
      message = QString("Info: %1 (%2:%3, %4)")
                  .arg(localMsg.constData())
                  .arg(file)
                  .arg(context.line)
                  .arg(function);
      break;
    case QtWarningMsg:
      message = QString("Warning: %1 (%2:%3, %4)")
                  .arg(localMsg.constData())
                  .arg(file)
                  .arg(context.line)
                  .arg(function);
      break;
    case QtCriticalMsg:
      message = QString("Critical: %1 (%2:%3, %4)")
                  .arg(localMsg.constData())
                  .arg(file)
                  .arg(context.line)
                  .arg(function);
      break;
    case QtFatalMsg:
      message = QString("Fatal: %1 (%2:%3, %4)")
                  .arg(localMsg.constData())
                  .arg(file)
                  .arg(context.line)
                  .arg(function);
      break;
    case QtDebugMsg:
    default:
      message = QString("Debug: %1 (%2:%3, %4)")
                  .arg(localMsg.constData())
                  .arg(file)
                  .arg(context.line)
                  .arg(function);
      break;
  }

  QString writableDocs =
    QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  QFile outFile(writableDocs + "/avogadro2.log");
  outFile.open(QIODevice::WriteOnly | QIODevice::Append);
  QTextStream ts(&outFile);
  ts << message << Qt::endl;
}

// Fixes issues with Nvidia drivers giving a GLES-style context instead of
// a Desktop context.
// Taken from https://github.com/openscad/openscad/pull/6711
void configureOpenGLContext()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)

  if (qEnvironmentVariableIsEmpty("QT_OPENGL")) {
    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
  }

  auto format = QSurfaceFormat::defaultFormat();
  format.setRenderableType(QSurfaceFormat::OpenGL);
  format.setProfile(QSurfaceFormat::CompatibilityProfile);
  if (format.depthBufferSize() < 24)
    format.setDepthBufferSize(24);
  if (format.stencilBufferSize() < 8)
    format.setStencilBufferSize(8);
  QSurfaceFormat::setDefaultFormat(format);
#endif
}

namespace {

/// Every command line option main() recognizes below. Used so that an option
/// following a value-taking option (e.g. "--rpc-name --disable-settings") is
/// not silently swallowed as that option's value. A value that merely starts
/// with a dash, such as the socket name "-my-socket", is still accepted.
bool isKnownOption(const QString& argument)
{
  static const char* const knownOptions[] = {
    "--rpc-name",      "--test-file",    "--test-no-exit", "--disable-settings",
    "--skip-autosave", "--skip-dialogs", "--crash-test"
  };
  for (const char* option : knownOptions) {
    if (argument == QLatin1String(option))
      return true;
  }
  return false;
}

#ifdef Q_OS_LINUX
// Remove the AppImage's bundled libraries from LD_LIBRARY_PATH
void removeAppImageLibraryPath()
{
  const QString appDir = qEnvironmentVariable("APPDIR");
  if (appDir.isEmpty())
    return;

  const QString root = QDir::cleanPath(appDir);
  const QString rootPrefix = root + '/';
  QStringList kept;
  const auto entries =
    qEnvironmentVariable("LD_LIBRARY_PATH").split(':', Qt::SkipEmptyParts);
  for (const auto& entry : entries) {
    const QString clean = QDir::cleanPath(entry);
    if (clean != root && !clean.startsWith(rootPrefix))
      kept << entry;
  }

  if (kept.isEmpty())
    qunsetenv("LD_LIBRARY_PATH");
  else
    qputenv("LD_LIBRARY_PATH", kept.join(':').toLocal8Bit());
}
#endif

} // namespace

int main(int argc, char* argv[])
{
#ifdef Q_OS_LINUX
  // AppRun puts the AppImage's bundled libraries on LD_LIBRARY_PATH. Every
  // child process (xdg-open and the browser, obabel, plugin scripts, user
  // programs) would inherit them and can fail to start (avogadrolibs#3150).
  // The dynamic loader has already read it, so this only affects children.
  removeAppImageLibraryPath();
#endif

#ifdef Q_OS_MAC
  // call some Objective-C++
  // removeMacSpecificMenuItems();
  // Native Mac applications do not have icons in the menus
  // until Tahoe (macOS 26)
  // (and only do this with Qt 6.7 or later)
  bool oldQtVersion = true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  oldQtVersion = false;
#endif
  auto currentOS = QOperatingSystemVersion::current();
  if (oldQtVersion || currentOS.majorVersion() < 26) {
    QCoreApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
  }
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
  // Use Qt-managed popup windows for combo boxes and context menus.
  // Native macOS popups can fail to appear in windows with QOpenGLWidget.
  QCoreApplication::setAttribute(Qt::AA_DontUseNativeMenuWindows);
#endif
#endif

  QCoreApplication::setOrganizationName("OpenChemistry");
  QCoreApplication::setOrganizationDomain("openchemistry.org");
  QCoreApplication::setApplicationName("Avogadro");
  QGuiApplication::setDesktopFileName("org.openchemistry.Avogadro2");

  configureOpenGLContext();

  QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
  QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

#ifdef Q_OS_WIN
  // We need to ensure desktop OpenGL is loaded for our rendering.
  QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);

#ifndef AVOGADRO_USE_SENTRY
  // remove the previous log file
  QString writableDocs =
    QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  QFile outFile(writableDocs + "/avogadro2.log");
  outFile.remove(); // we don't care if this fails

  // install the message handler (goes to Documents / avogadro2.log)
  qInstallMessageHandler(myMessageOutput);
#endif
#endif

  // Start crash reporting before the QApplication instance exists, so that a
  // crash while Qt loads its platform plugin is still caught. This also has to
  // come after the message handler above: the Qt integration chains to
  // whichever handler was installed first, so installing one afterwards would
  // silently discard every breadcrumb.
  Avogadro::CrashReporterGuard crashReporterGuard;

  // output the version information
  qDebug() << "Avogadroapp version: " << AvogadroApp_VERSION;
  qDebug() << "Avogadrolibs version: " << Avogadro::version();
  qDebug() << "Qt version: " << qVersion();
  qDebug() << "SSL version: " << QSslSocket::sslLibraryVersionString();

  Avogadro::Application app(argc, argv);

  // Load user configuration
  QSettings settings;
  qDebug() << "App configuration loaded from"_L1 << settings.fileName();

  // Before we do much else, load translations
  // This ensures help messages and debugging info can be translated

  QLocale preferredLocale;
  bool noTranslation = false;
  bool useSystem = false;
  if (settings.contains("locale"_L1)) {
    QString configLocale = settings.value("locale"_L1).toString();
    qDebug() << "Preference for locale found in configuration:" << configLocale;
    if (configLocale == "system"_L1) {
      // Explicit request for the system language
      useSystem = true;
    } else if (configLocale == "default"_L1) {
      // Explicit request not to translate at all, just use the hard-coded text
      noTranslation = true;
      useSystem = false;
    } else {
      // If a language has been specified, we should use it
      useSystem = false;
      preferredLocale = QLocale(configLocale);
      qDebug() << "Attempting to use configured locale:"
               << preferredLocale.bcp47Name();
    }
  } else {
    // Otherwise, the system language should be used
    useSystem = true;
  }

  if (useSystem) {
#ifdef Q_OS_MAC
    // On macOS, we can get the system language from the AppleLanguages
    // setting, which is a list of preferred languages.
    // https://www.qtcentre.org/threads/57433-SOLVED-Get-system-language-on-Mac-OS-X
    QVariant osxLanguageSettings = settings.value("AppleLanguages"_L1);
    QStringList displayLanguages = osxLanguageSettings.toStringList();
    if (!displayLanguages.isEmpty()) {
      QString preferredLanguage = displayLanguages.first();
      qDebug() << "System preferred language is:" << preferredLanguage;
      preferredLocale = QLocale(preferredLanguage);
    } else {
      // Better hope that the default constructor succeeds instead
      preferredLocale = QLocale();
    }
#else
    // Default constructor uses the default locale, which is determined from the
    // system's locale settings at application startup
    preferredLocale = QLocale();
#endif
    qDebug() << "Attempting to use system locale: "
             << preferredLocale.bcp47Name();
  }

  // We can skip all of this if the user wants to just use the default interface
  // without any translation
  if (!noTranslation) {
    // Note that if the user uses multiple languages on their system, the list
    // of fallback languages for the system locale will be different to the
    // fallback list for the basic locale e.g. fr-CA falls back only to fr, but
    // if the user has both fr-CA and de as system interface langs, the lookups
    // will check first for fr-CA, then fr, then de.

    // The different components use translation files stored in different
    // places, so collect lists of possible locations separately for each
    // component
    QStringList qtTranslationPaths{
      // With system copies of the Qt libraries for installs from distro repos
      QLibraryInfo::location(QLibraryInfo::TranslationsPath),
      // With the Avogadro installation for bundles (Win, Mac, Flatpak etc.)
      QCoreApplication::applicationDirPath() + "/../share/avogadro2/i18n/"_L1,
    };

    // Get all the paths that *might* have translation files
    // We don't confirm that they actually exist or have translation files, we
    // will just try to load translations from each of them in turn until we
    // find an appropriate file
    auto [appTranslationPaths, libsTranslationPaths] =
      Avogadro::translationPaths();

    // We will need translators for each component -- make sure to use pointers
    auto* qtTranslator = new QTranslator(&app);
    auto* qtBaseTranslator = new QTranslator(&app);
    auto* libsTranslator = new QTranslator(&app);
    auto* appTranslator = new QTranslator(&app);
    // The locale of the translation that we actually end up using
    QString translationLanguage;

    // First, resolve to a avogadroapp translation file based on the preferred
    // locale
    bool appLoaded = false;
    foreach (const QString& translationPath, appTranslationPaths) {
      if (!appLoaded && appTranslator->load(preferredLocale, "avogadroapp"_L1,
                                            "-"_L1, translationPath)) {
        if (app.installTranslator(appTranslator)) {
          appLoaded = true;
          // Store the loaded file's name (e.g., id, pt_BR, ca@valencia); the
          // language stored in the .qm metadata can differ (ar -> ar_EG,
          // ca@valencia -> ca_ES)
          translationLanguage =
            QFileInfo(appTranslator->filePath()).completeBaseName();
          const QString prefix = "avogadroapp-"_L1;
          if (translationLanguage.startsWith(prefix))
            translationLanguage = translationLanguage.mid(prefix.length());
          qDebug() << "AvogadroApp translation (best match for locale):"
                   << translationLanguage << "loaded from" << translationPath;
        }
      }
    }
    if (!appLoaded) {
      // Interface will not be translated, nothing else to do with the locale
      qDebug() << "No translation file found, interface language will be "
                  "untranslated English";
    } else {
      // avogadroapp can be translated, so now we need to load files for the
      // other translators to match. We resolve starting from the locale of the
      // translation we actually found, *not* from preferredLocale, because:
      //
      // If preferredLocale is the system locale and the user has a multilingual
      // setup, then the list of fallback options for the resolution (which
      // comes from the uiLanguages() list for a QLocale) may be mutually
      // incompatible. Resolution starting from preferredLocale for each
      // component individually could resolve to different languages from the
      // uiLanguages() list, depending on the translation files available,
      // potentially resulting in inconsistency.
      //
      // For example, if preferredLocale.uiLanguages() has Indonesian followed
      // by Arabic, Avogadro will use Indonesian as it has a translation file
      // for it, but Qt does not and will fall back to Arabic, mixing the
      // languages (and in this example even the text direction would in theory
      // differ!).
      const QLocale translationLocale(translationLanguage);
      qDebug() << "Corresponding locale: " << translationLocale.bcp47Name();

      // Find translations for Qt itself
      // To translate Qt all we really need is a qtbase_*.qm catalogue, which
      // has translations for all the important modules (Qt Core, Gui, Widgets)
      bool qtbaseLoaded = false;
      foreach (const QString& translationPath, qtTranslationPaths) {
        if (qtTranslator->load(translationLocale, "qt"_L1, "_"_L1,
                               translationPath)) {
          if (app.installTranslator(qtTranslator)) {
            qDebug() << "qt translation" << translationLanguage << "loaded from"
                     << translationPath;
            qtbaseLoaded = true;
            break;
          }
        }
      }
      // The qt_*.qm files are "meta-catalogues" that pull in the qtbase ones +
      // some others (generally just Qt Multimedia), but might fail, so the base
      // ones are preferable. This is here in case the meta-catalogue is the
      // only one available, for whatever reason.
      // See https://doc.qt.io/qt-6/localization.html#available-catalogs
      if (!qtbaseLoaded) {
        foreach (const QString& translationPath, qtTranslationPaths) {
          if (qtBaseTranslator->load(translationLocale, "qtbase"_L1, "_"_L1,
                                     translationPath)) {
            if (app.installTranslator(qtBaseTranslator)) {
              qDebug() << "qtbase translation" << translationLanguage
                       << "loaded from" << translationPath;
              break;
            }
          }
        }
      }

      // Now find translations for avogadrolibs
      foreach (const QString& translationPath, libsTranslationPaths) {
        if (libsTranslator->load("avogadrolibs-" + translationLanguage,
                                 translationPath)) {
          if (app.installTranslator(libsTranslator)) {
            qDebug() << "AvogadroLibs translation" << translationLanguage
                     << "loaded from" << translationPath;
            break;
          }
        }
      } // Done looking for translations

      // Only right-to-left languages (e.g., ar, fa, he) should mirror the UI
      app.setLayoutDirection(translationLocale.textDirection());
    }
  }

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
    // This check runs before the full argument parsing below, so look for the
    // flags that suppress dialogs here (--rpc-name implies --skip-dialogs).
    const QStringList earlyArgs = QCoreApplication::arguments();
    if (earlyArgs.contains(QStringLiteral("--skip-dialogs")) ||
        earlyArgs.contains(QStringLiteral("--rpc-name"))) {
      qCritical("--skip-dialogs: This system does not support OpenGL.");
    } else {
      QMessageBox::information(
        nullptr, QCoreApplication::translate("main.cpp", "Avogadro"),
        QCoreApplication::translate("main.cpp",
                                    "This system does not support OpenGL."));
    }
    return 1;
  }

  // Set up the default format for our GL contexts.
  QSurfaceFormat defaultFormat = QSurfaceFormat::defaultFormat();
#if defined(Q_OS_MAC)
  defaultFormat.setAlphaBufferSize(8);
#endif
  defaultFormat.setVersion(4, 0);
  defaultFormat.setProfile(QSurfaceFormat::CoreProfile);
  QSurfaceFormat::setDefaultFormat(defaultFormat);

  QStringList fileNames;
  bool disableSettings = false;
  bool skipAutosave = false;
  bool skipDialogs = false;
  bool rpcNameGiven = false;
  QString rpcName = QStringLiteral("avogadro");
#ifdef QTTESTING
  QString testFile;
  bool testExit = true;
#endif
  QStringList args = QCoreApplication::arguments();
  for (QStringList::const_iterator it = args.constBegin() + 1;
       it != args.constEnd(); ++it) {
    if (*it == "--rpc-name") {
      if (it + 1 == args.constEnd() || isKnownOption(*(it + 1))) {
        qWarning("Avogadro called with --rpc-name but no socket name.");
        return EXIT_FAILURE;
      }
      rpcName = *(++it);
      rpcNameGiven = true;
    } else if (*it == "--test-file") {
      if (it + 1 == args.constEnd() || isKnownOption(*(it + 1))) {
        qWarning("Avogadro called with --test-file but no file name.");
        return EXIT_FAILURE;
      }
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
    } else if (*it == "--skip-autosave") {
      skipAutosave = true;
    } else if (*it == "--skip-dialogs") {
      skipDialogs = true;
    } else if (*it == "--crash-test") {
#ifdef AVOGADRO_USE_SENTRY
      Avogadro::CrashReporter::triggerTestCrash();
#else
      qWarning("Avogadro called with --crash-test but crash reporting is "
               "disabled.");
      return EXIT_FAILURE;
#endif
    } else if (it->startsWith("-")) {
      qWarning("Unknown command line option '%s'", qPrintable(*it));
      return EXIT_FAILURE;
    } else { // Assume it is a file name.
      fileNames << *it;
    }
  }

  // A scripted (RPC) launch has nobody to click on a startup dialog.
  if (rpcNameGiven && !skipDialogs) {
    skipDialogs = true;
    qInfo("--rpc-name given: implying --skip-dialogs.");
  }

  // MainWindow records skipDialogs in QtGui::Utilities, which plugins read.
  Avogadro::MainWindow window(fileNames, disableSettings, skipAutosave,
                              skipDialogs);
#ifdef QTTESTING
  window.playTest(testFile, testExit);
#endif
  window.show();

  // On a diagnostic build, ask about crash reporting the first time it runs.
  // A no-op everywhere else.
  if (skipDialogs) {
#ifdef AVOGADRO_USE_SENTRY
    if (!Avogadro::CrashReporter::consentRequested())
      qInfo("--skip-dialogs: skipped crash reporting consent dialog; assuming "
            "no consent (stored choice untouched).");
#endif
  } else {
    Avogadro::CrashReporter::promptForConsentIfNeeded(&window);
  }

#ifdef Avogadro_ENABLE_RPC
  // create rpc listener
  Avogadro::RpcListener listener(rpcName);
  listener.start();
#endif

  return app.exec();
}
