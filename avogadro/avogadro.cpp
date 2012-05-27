/******************************************************************************

  This source file is part of the Avogadro project.

  Copyright 2012 Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/

#include <QtGui/QApplication>
#include <QtGui/QMessageBox>
#include <QtOpenGL/QGLFormat>

#include "mainwindow.h"
#include <avogadro/io/cmlformat.h>

int main(int argc, char *argv[])
{
  QCoreApplication::setOrganizationName("OpenChemistry");
  QCoreApplication::setOrganizationDomain("openchemistry.org");
  QCoreApplication::setApplicationName("Avogaro");

  QApplication app(argc, argv);

  if (!QGLFormat::hasOpenGL()) {
    QMessageBox::information(0, "Avogadro",
                             "This system does not support OpenGL!");
    return 1;
  }
  // Set up the default format for our GL contexts.
  QGLFormat defaultFormat = QGLFormat::defaultFormat();
  defaultFormat.setSampleBuffers(true);
  QGLFormat::setDefaultFormat(defaultFormat);

  QString fileName;
  if (argc > 1)
    fileName = argv[1];
  Avogadro::MainWindow *window = new Avogadro::MainWindow(fileName);
  window->show();

  return app.exec();
}
