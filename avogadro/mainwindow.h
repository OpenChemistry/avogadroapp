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

namespace Ui {
class MainWindow;
}

namespace Avogadro {

namespace QtOpenGL {
class GLWidget;
}

namespace QtGui {
class ScenePlugin;
class ScenePluginModel;
class ExtensionPlugin;
class Molecule;
}

class MainWindow : public QMainWindow
{
Q_OBJECT
public:
  MainWindow(const QString &filename = QString());
  ~MainWindow();

  void setMolecule(QtGui::Molecule *molecule);
  QtGui::Molecule * molecule() { return m_molecule; }

  void writeSettings();
  void readSettings();

signals:
  void moleculeChanged(QtGui::Molecule *molecue);

protected:
  void closeEvent(QCloseEvent *event);

protected slots:
  void newMolecule();
  void openFile();
  void openFile(const QString &fileName);
  void openRecentFile();
  void updateRecentFiles();
  void updateScenePlugins();

  void updateTool();
  void updateElement();

private:
  Ui::MainWindow *m_ui;
  QtGui::Molecule *m_molecule;
  QtGui::ScenePluginModel *m_scenePluginModel;

  QStringList m_recentFiles;
  QList<QAction*> m_actionRecentFiles;

  QVector<unsigned char> m_elementLookup;
  void buildElements();
  void addElement(unsigned char atomicNum);
  void buildMenu(QtGui::ExtensionPlugin *extension);
};

} // End Avogadro namespace

#endif
