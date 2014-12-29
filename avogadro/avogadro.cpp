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

#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtOpenGL/QGLFormat>

#include "mainwindow.h"

#ifdef Avogadro_ENABLE_RPC
# include "rpclistener.h"
#endif

int main(int argc, char *argv[])
{
  QCoreApplication::setOrganizationName("OpenChemistry");
  QCoreApplication::setOrganizationDomain("openchemistry.org");
  QCoreApplication::setApplicationName("Avogadro");
#ifdef Q_WS_MAC
  // Native Mac applications do not have icons in the menus
  QCoreApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
#endif

  QApplication app(argc, argv);

  if (!QGLFormat::hasOpenGL()) {
    QMessageBox::information(0, "Avogadro",
                             "This system does not support OpenGL!");
    return 1;
  }

  // Use high-resolution (e.g., 2x) icons if available
  app.setAttribute(Qt::AA_UseHighDpiPixmaps);

  // Set up the default format for our GL contexts.
  QGLFormat defaultFormat = QGLFormat::defaultFormat();
  defaultFormat.setSampleBuffers(true);
  defaultFormat.setAlpha(true);
  QGLFormat::setDefaultFormat(defaultFormat);

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
    }
    else if (*it == "--test-no-exit") {
#ifdef QTTESTING
      testExit = false;
#else
      qWarning("Avogadro called with --test-no-exit but testing is disabled.");
      return EXIT_FAILURE;
#endif
    }
    else if (*it == "--disable-settings") {
      disableSettings = true;
    }
    else if (it->startsWith("-")) {
      qWarning("Unknown command line option '%s'", qPrintable(*it));
      return EXIT_FAILURE;
    }
    else { // Assume it is a file name.
      fileNames << *it;
    }
  }

  Avogadro::MainWindow *window = new Avogadro::MainWindow(fileNames,
                                                          disableSettings);
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
