/**********************************************************************
This source file is part of the Avogadro project.

Copyright (C) 2018 by Geoffrey R. Hutchison

This source code is released under the New BSD License, (the "License").

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

This file is part of the Avogadro molecular editor project.
For more information, see <http://avogadro.cc/>
 ***********************************************************************/

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
