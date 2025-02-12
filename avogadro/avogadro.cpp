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

// install a message handler (for Windows)
#include <QFile>
#include <QSslSocket>
#include <QTextStream>

#include <avogadro/core/version.h>

#include "application.h"
#include "avogadroappconfig.h"
#include "mainwindow.h"

#ifdef Q_OS_MAC
void removeMacSpecificMenuItems();
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
                  .arg(localMsg.constData(), file)
                  .arg(context.line)
                  .arg(function);
      break;
    case QtWarningMsg:
      message = QString("Warning: %1 (%2:%3, %4)")
                  .arg(localMsg.constData(), file)
                  .arg(context.line)
                  .arg(function);
      break;
    case QtCriticalMsg:
      message = QString("Critical: %1 (%2:%3, %4)")
                  .arg(localMsg.constData(), file)
                  .arg(context.line)
                  .arg(function);
      break;
    case QtFatalMsg:
      message = QString("Fatal: %1 (%2:%3, %4)")
                  .arg(localMsg.constData(), file)
                  .arg(context.line)
                  .arg(function);
      break;
    case QtDebugMsg:
    default:
      message = QString("Debug: %1 (%2:%3, %4)")
                  .arg(localMsg.constData(), file)
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
  QGuiApplication::setDesktopFileName("org.openchemistry.Avogadro2");

  QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
  QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

#ifdef Q_OS_WIN
  // We need to ensure desktop OpenGL is loaded for our rendering.
  QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);

  // remove the previous log file
  QString writableDocs =
    QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  QFile outFile(writableDocs + "/avogadro2.log");
  outFile.remove(); // we don't care if this fails

  // install the message handler (goes to Documents / avogadro2.log)
  qInstallMessageHandler(myMessageOutput);
#endif

  // output the version information
  qDebug() << "Avogadroapp version: " << AvogadroApp_VERSION;
  qDebug() << "Avogadrolibs version: " << Avogadro::version();
  qDebug() << "Qt version: " << qVersion();
  qDebug() << "SSL version: " << QSslSocket::sslLibraryVersionString();

  Avogadro::Application app(argc, argv);

  QSettings settings;
  QString language = settings.value("locale", "System").toString();

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
  auto* qtTranslator = new QTranslator;
  auto* qtBaseTranslator = new QTranslator;
  auto* avoTranslator = new QTranslator;
  auto* avoLibsTranslator = new QTranslator;
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

  // Go through the possible translations / locale codes
  // to get the localized names for the language dialog
  if (successfulPath.isEmpty()) {
    // the default for most systems
    // (e.g., /usr/bin/avogadro2 -> /usr/share/avogadro2/i18n/)
    // or /Applications/Avogadro2.app/Contents/share/avogadro2/i18n/
    // .. etc.
    successfulPath =
      QCoreApplication::applicationDirPath() + "/../share/avogadro2/i18n/";
  }

  QDir dir(successfulPath);
  QStringList files =
    dir.entryList(QStringList() << "avogadroapp*.qm", QDir::Files);
  QStringList languages, codes;

  languages << "System"; // we handle this in the dialog
  codes << "";           // default is the system language

  bool addedUS = false;

  // check what files exist
  foreach (const QString& file, files) {
    // remove "avogadroapp-" and the ".qm"
    QString localeCode = file.left(file.indexOf('.')).remove("avogadroapp-");

    if (localeCode.startsWith("en") && !addedUS) {
      // add US English (default)
      addedUS = true;
      QLocale us("en_US");
      languages << us.nativeLanguageName();
      codes << "en_US";
    }

    QLocale locale(localeCode);
    QString languageName = locale.nativeLanguageName();
    if (languageName.isEmpty() && localeCode == "oc")
      languageName = "Occitan";
    // potentially other exceptions here

    // cases like Brazilian Portuguese show up as duplicates
    if (languages.contains(languageName)) {
      languageName += " (" + locale.nativeCountryName() + ")";
    }

    languages << languageName;
    codes << localeCode;
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
    QMessageBox::information(
      nullptr, QCoreApplication::translate("main.cpp", "Avogadro"),
      QCoreApplication::translate("main.cpp",
                                  "This system does not support OpenGL."));
    return 1;
  }

  // Set up the default format for our GL contexts.
#if defined(Q_OS_MAC)
  QSurfaceFormat defaultFormat = QSurfaceFormat::defaultFormat();
  defaultFormat.setAlphaBufferSize(8);
  QSurfaceFormat::setDefaultFormat(defaultFormat);
#endif

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

  Avogadro::MainWindow window(fileNames, disableSettings);
  window.setTranslationList(languages, codes);
#ifdef QTTESTING
  window.playTest(testFile, testExit);
#endif
  window.show();

#ifdef Avogadro_ENABLE_RPC
  // create rpc listener
  Avogadro::RpcListener listener;
  listener.start();
#endif

  return app.exec();
}
