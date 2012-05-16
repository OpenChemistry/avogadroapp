#include <QtGui/QApplication>
#include <QtGui/QMessageBox>
#include <QtOpenGL/QGLFormat>

#include "mainwindow.h"

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

  Avogadro::MainWindow window;
  window.show();

  return app.exec();
}
