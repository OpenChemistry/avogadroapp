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

#ifdef QTTESTING
class pqTestUtility;
#endif

namespace Ui {
class MainWindow;
}

namespace Avogadro {

namespace QtOpenGL {
class GLWidget;
}

namespace Io {
class FileFormat;
}

namespace QtGui {
class ScenePlugin;
class ScenePluginModel;
class ToolPlugin;
class ExtensionPlugin;
class Molecule;
}

class MainWindow : public QMainWindow
{
Q_OBJECT
public:
  MainWindow(const QString &filename = QString(), bool disableSettings = false);
  ~MainWindow();

public slots:
  void setMolecule(Avogadro::QtGui::Molecule *molecule);

#ifdef QTTESTING
  void playTest(const QString &fileName, bool exit = true);
#endif

public:
  QtGui::Molecule * molecule() { return m_molecule; }

  void writeSettings();
  void readSettings();

signals:
  void moleculeChanged(QtGui::Molecule *molecue);

protected:
  void closeEvent(QCloseEvent *event);

protected slots:
  void moleculeReady(int number);
  void newMolecule();
  void openFile();
  void openFile(const QString &fileName);
  void openRecentFile();
  void updateRecentFiles();
  void saveFile();
  void exportFile();
  void saveFile(const QString &fileName);
  void updateScenePlugins();
  void about();

#ifdef QTTESTING
protected slots:
  void record();
  void play();
  void playTest();
  void popup();
#endif

private:
  Ui::MainWindow *m_ui;
  QtGui::Molecule *m_molecule;
  QtGui::ScenePluginModel *m_scenePluginModel;

  QStringList m_recentFiles;
  QList<QAction*> m_actionRecentFiles;

#ifdef QTTESTING
  pqTestUtility *m_testUtility;
  QString m_testFile;
  bool m_testExit;
#endif

  void buildMenu(QtGui::ExtensionPlugin *extension);
  void buildTools(QList<Avogadro::QtGui::ToolPlugin *> toolList);
  QString generateFilterString(
      const std::vector<const Io::FileFormat *> &formats,
      bool addAllEntry = true);
};

} // End Avogadro namespace

#endif
