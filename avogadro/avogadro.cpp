/******************************************************************************

  This source file is part of the Avogadro project.

  Copyright 2012-2014 Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/

#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLContext>
#include <QtGui/QSurfaceFormat>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>

#include <QtCore/QDebug>
#include <QtCore/QLibraryInfo>
#include <QtCore/QProcess>
#include <QtCore/QTranslator>

#include "application.h"
#include "mainwindow.h"

#ifdef Q_OS_MAC
void removeMacSpecificMenuItems();
#endif

#ifdef Avogadro_ENABLE_RPC
#include "rpclistener.h"
#endif

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

#ifdef Q_OS_WIN
  // We need to ensure desktop OpenGL is loaded for our rendering.
  QApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
#endif

  Avogadro::Application app(argc, argv);

  // Before we do much else, load translations
  // This ensures help messages and debugging info will be translated
  QStringList translationPaths;
  // check environment variable and local paths
  foreach (const QString& variable, QProcess::systemEnvironment()) {
    QStringList split1 = variable.split('=');
    if (split1[0] == "AVOGADRO_TRANSLATIONS") {
      foreach (const QString& path, split1[1].split(':'))
        translationPaths << path;
    }
  }

  translationPaths << QCoreApplication::applicationDirPath() +
                        "/../share/avogadro/i18n/";

  // Load Qt translations first
  qDebug() << "Locale: " << QLocale::system().name();

  bool tryLoadingQtTranslations = false;
  QTranslator qtTranslator(0);
  if (qtTranslator.load(
        QLocale::system(), "qt", "_",
        QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
    qDebug() << " translation success";
    app.installTranslator(&qtTranslator);
  } else {
    // Check other paths.
    tryLoadingQtTranslations = true;
  }

  QTranslator qtBaseTranslator(0);
  qDebug() << QLibraryInfo::location(QLibraryInfo::TranslationsPath);
  if (qtTranslator.load(
        QLocale::system(), "qtbase", "_",
        QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
    qDebug() << " translation success";
    app.installTranslator(&qtTranslator);
  }

  // TODO: need to separate avogadrolibs from app
  QTranslator avoTranslator(0);
  foreach (const QString& translationPath, translationPaths) {
    // We can't find the normal Qt translations (maybe we're in a "bundle"?)
    if (tryLoadingQtTranslations) {
      if (qtTranslator.load(QLocale::system(), "qt", "_", translationPath)) {
        app.installTranslator(&qtTranslator);
        tryLoadingQtTranslations = false; // already loaded
      }
      if (qtBaseTranslator.load(QLocale::system(), "qtbase", "_",
                                translationPath)) {
        app.installTranslator(&qtBaseTranslator);
      }
    }

    if (avoTranslator.load(QLocale::system(), "avogadrolibs", "_",
                           translationPath)) {
      app.installTranslator(&avoTranslator);
      qDebug() << "Translation successfully loaded.";
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
    QMessageBox::information(
      0, QCoreApplication::translate("main.cpp", "Avogadro"),
      QCoreApplication::translate("main.cpp",
                                  "This system does not support OpenGL."));
    return 1;
  }

  // Use high-resolution (e.g., 2x) icons if available
  app.setAttribute(Qt::AA_UseHighDpiPixmaps);

  // Set up the default format for our GL contexts.
  //  QSurfaceFormat defaultFormat = QSurfaceFormat::defaultFormat();
  //  defaultFormat.setSamples(4);
  //  defaultFormat.setAlphaBufferSize(8);
  //  QSurfaceFormat::setDefaultFormat(defaultFormat);

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
