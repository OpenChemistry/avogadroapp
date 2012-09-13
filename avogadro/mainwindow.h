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

#ifndef AVOGADRO_MAINWINDOW_H
#define AVOGADRO_MAINWINDOW_H

#include <QtGui/QMainWindow>
#include <QtCore/QString>

namespace Avogadro {

namespace Core {
class Molecule;
}

namespace QtOpenGL {
class GLWidget;
}

namespace QtGui {
class ScenePlugin;
class ExtensionPlugin;
}

class MainWindow : public QMainWindow
{
Q_OBJECT
public:
  MainWindow(const QString &filename = QString());
  ~MainWindow();

  void setMolecule(Core::Molecule *molecule);
  Core::Molecule * molecule() { return m_molecule; }

  void writeSettings();
  void readSettings();

protected:
  void closeEvent(QCloseEvent *event);

protected slots:
  void openFile();
  void openFile(const QString &fileName);
  void openRecentFile();

private:
  QtOpenGL::GLWidget *m_glWidget;
  Core::Molecule     *m_molecule;
  QStringList         m_recentFiles;

  QList<QAction*>     m_actionRecentFiles;

  QtGui::ScenePlugin *m_scenePlugin;

  void updateRecentFiles();

  void buildMenu(QtGui::ExtensionPlugin *extension);
};

} // End Avogadro namespace

#endif
