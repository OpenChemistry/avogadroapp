/******************************************************************************

  This source file is part of the Avogadro project.

  Copyright 2012-2013 Kitware, Inc.

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

class QProgressDialog;
class QThread;

namespace Ui {
class MainWindow;
}

namespace Avogadro {
class BackgroundFileFormat;
class MenuBuilder;

namespace QtOpenGL {
class GLWidget;
}

namespace Io {
class FileFormat;
}

namespace QtGui {
class ScenePlugin;
class ToolPlugin;
class ExtensionPlugin;
class Molecule;
}

/**
 * @class MainWindow
 * @author Marcus D. Hanwell
 *
 * The MainWindow class for the Avogadro application. Takes care of initializing
 * the application and overall layout.
 */

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

  /**
   * Write out all application settings, normally done as part of the
   * application close event.
   */
  void writeSettings();

  /**
   * Read in all settings, and initialize our application based on the stored
   * settings.
   */
  void readSettings();

signals:
  /**
   * Emitted when the active molecule in the application has changed.
   */
  void moleculeChanged(QtGui::Molecule *molecue);

protected:
  void closeEvent(QCloseEvent *event);

protected slots:
  /**
   * Slot provided for extensions to indicate a molecule is ready to be read in.
   * This slot will then pass a molecule to the extension for the data to be
   * read in to.
   */
  void moleculeReady(int number);

  /**
   * Create a new molecule and make it the active molecule.
   */
  void newMolecule();

  /**
   * Prompt for a file location, and attempt to open the specified file using
   * our native readers.
   */
  void openFile();

  /**
   * Import a file, using the full selection of formats capable of reading.
   */
  void importFile();

  /**
   * If specified, use the FileFormat @a reader to save the file. This method
   * takes ownership of @a reader and will delete it before returning. If not
   * specified, a reader will be selected based on fileName's extension.
   */
  bool openFile(const QString &fileName, Io::FileFormat *reader = NULL);

  /**
   * Open file in the recent files list.
   */
  void openRecentFile();

  /**
   * Update the list of recent files.
   */
  void updateRecentFiles();

  /**
   * Prompt for a file location, and attempt to save the active molecule to the
   * specified location.
   */
  void saveFile();

  /**
   * Export a file, using the full selection of formats capable of writing.
   */
  void exportFile();

  /**
   * If specified, use the FileFormat @a writer to save the file. This method
   * takes ownership of @a writer and will delete it before returning. If not
   * specified, a writer will be selected based on fileName's extension.
   */
  void saveFile(const QString &fileName, Io::FileFormat *writer = NULL);

#ifdef QTTESTING
protected slots:
  void record();
  void play();
  void playTest();
  void popup();
#endif

private slots:
  void showAboutDialog();

  /**
   * @brief Register file formats from extensions when ready.
   */
  void fileFormatsReady();

  /**
   * @brief Attempt to read any files requested on the command line, intended to
   * be called after the fileFormatsReady slot is triggered for formats added by
   * extensions. Any that are successfully read will be removed from the list,
   * after the timeout triggers the list will be cleared.
   */
  void readQueuedFiles();

  /**
   * @brief Clear the list of queued files, triggered by a timeout to allow
   * delayed file readying within the first few seconds of application start up.
   */
  void clearQueuedFiles();

  /**
   * @brief Register molequeue open-with handlers for RPC and executable file
   * handling. Called 5 seconds after startup to give extensions a chance to
   * register file formats.
   */
  void registerMoleQueue();

  /**
   * @brief The background file reader thread has completed, set the active
   * molecule, and clean up after the threaded read.
   */
  void backgroundReaderFinished();

  /**
   * @brief Called when a toolbar action is clicked. The sender is expected to
   * be the action, and the parent of the action should be the toolPlugin to
   * activate.
   */
  void toolActivated();

private:
  Ui::MainWindow *m_ui;
  QtGui::Molecule *m_molecule;
  QList<QString> m_queuedFiles;

  QStringList m_recentFiles;
  QList<QAction*> m_actionRecentFiles;

  MenuBuilder *m_menuBuilder;

  // These variables take care of background file reading.
  QThread *m_fileReadThread;
  BackgroundFileFormat *m_threadedReader;
  QProgressDialog *m_fileReadProgress;
  QtGui::Molecule *m_fileReadMolecule;
  QToolBar *m_fileToolBar;
  QToolBar *m_toolToolBar;

#ifdef QTTESTING
  pqTestUtility *m_testUtility;
  QString m_testFile;
  bool m_testExit;
#endif

  /**
   * Build the main menu, delayed until all plugins have registered actions.
   */
  void buildMenu();

  /**
   * Add the menu entries for the extension passed in.
   */
  void buildMenu(QtGui::ExtensionPlugin *extension);

  /**
   * Initialize the tool plugins.
   */
  void buildTools(QList<Avogadro::QtGui::ToolPlugin *> toolList);

  /**
   * Convenience function that converts a file extension to a wildcard
   * expression, e.g. "out" to "*.out". This method also checks for "extensions"
   * that aren't really extensions but full filenames, e.g. HISTORY files from
   * DL-POLY. These are returned unmodified.
   */
  static QString extensionToWildCard(const QString &extension);

  /**
   * Convenience function to generate a filter string for the supplied formats.
   */
  QString generateFilterString(
      const std::vector<const Io::FileFormat *> &formats,
      bool addAllEntry = true);

  /**
   * Helper function for loading a freedesktop QIcon in a cross-platform way.
   * @param name The freedesktop name of the icon:
   * http://standards.freedesktop.org/icon-naming-spec/icon-naming-spec-latest.html
   * @note The icons in avogadro/icons/freedesktop-fallback/ are used if the
   * current platform doesn't support freedesktop icon themes.
   */
  static QIcon standardIcon(const QString &name);
};

} // End Avogadro namespace

#endif
