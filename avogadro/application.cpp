/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#include "application.h"
#include "mainwindow.h"

#include <avogadro/io/fileformat.h>
#include <avogadro/io/fileformatmanager.h>
#include <avogadro/qtgui/fileformatdialog.h>

#include <QtGui/QFileOpenEvent>
#include <QtGui/QWindow>

#include <QDebug>

namespace Avogadro {

Application::Application(int& argc, char** argv)
  : QApplication(argc, argv)
{ }

// Handle open events (e.g., Mac OS X open files)
bool Application::event(QEvent* event)
{
  switch (event->type()) {
    case QEvent::FileOpen:
      return loadFile(static_cast<QFileOpenEvent*>(event)->file());
    default:
      return QGuiApplication::event(event);
  }
}

bool Application::loadFile(const QString& fileName)
{
  if (fileName.isEmpty()) {
    return false;
  }

  // check to see if we already have an open window
  // (we'll let MainWindow handle the real work)
  const MainWindow* window = NULL;
  foreach (const QWidget* item, topLevelWidgets()) {
    window = qobject_cast<const MainWindow*>(item);
    if (window)
      break;
  }

  // if not, need to make this spawn a new instance
  if (!window) {
    qDebug() << " don't have a window! ";
    return false;
  }

  if (!const_cast<MainWindow*>(window)->openFile(fileName)) {
    qDebug() << " failed to open through MainWindow";
    return false;
  }

  return true;
}

} // end namespace Avogadro
