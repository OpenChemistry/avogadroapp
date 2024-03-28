/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#ifndef AVOGADRO_MAINWINDOW_H
#define AVOGADRO_MAINWINDOW_H

#include <QtCore/QStringList>
#include <QtCore/QVariantMap>
#include <QtWidgets/QMainWindow>

#ifdef QTTESTING
class pqTestUtility;
#endif

class QProgressDialog;
class QThread;
class QTreeView;
class QNetworkAccessManager;
class QNetworkReply;

namespace Ui {
class AboutDialog;
}

namespace Avogadro {

class BackgroundFileFormat;
class MenuBuilder;
class ViewFactory;

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
class MoleculeModel;
class MultiViewWidget;
class RWMolecule;
class LayerModel;
}

#ifdef _3DCONNEXION
class TDxController;
#endif
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
  MainWindow(const QStringList& fileNames, bool disableSettings = false);
  ~MainWindow();

public slots:
  void setMolecule(Avogadro::QtGui::Molecule* molecule);
  void autosaveDocument(); //line to declare the autosave slot
  /**
   * Update internal state to reflect that the molecule has been modified.
   */
  void markMoleculeDirty();

  /**
   * Update internal state to reflect that the molecule is not modified.
   */
  void markMoleculeClean();

  /**
   * Update the main window title.
   */
  void updateWindowTitle();

  /**
   * Use the FileFormat @a reader to load @a fileName. This method
   * takes ownership of @a reader and will delete it before returning.
   */
  bool openFile(const QString& fileName, Io::FileFormat* reader = nullptr);

  void exportGraphics(QString fileName);

  /**
   * Export a file, using the full selection of formats capable of writing.
   * The format will be guessed based on the filename extension.
   * If @a async is true (default), the file is saved asynchronously.
   * @return If @a async is true, this function returns true if a suitable
   * writer was found (not if the write was successful). If @a async is
   * false, the return value indicates whether or not the file was written
   * successfully.
   */
  bool exportFile(const QString& fileName, bool async = true);

  /**
   * Export a file, using the full selection of formats capable of writing.
   * Will use @a format to determine the file format to use.
   * @return String-representation of the exported file, or an empty string if
   * the export failed.
   */
  std::string exportString(const std::string& format);

  /**
   * Move @a fileName as a plugin script (i.e. put it in the correct dir)
   */
  bool addScript(const QString& fileName);

#ifdef QTTESTING
  void playTest(const QString& fileName, bool exit = true);
#endif

public:
  QtGui::Molecule* molecule() { return m_molecule; }

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

  /**
   * Set the list of possible translations
   */
  void setTranslationList(const QStringList& list, const QStringList& codes)
  {
    m_translationList = list;
    m_localeCodes = codes;
  }

  /**
   * Handle script commands
   * @param command The command to execute
   * @param options The options to the command
   *
   * @return True if the command was handled, false otherwise
   */
  bool handleCommand(const QString& command, const QVariantMap& options);

signals:
  /**
   * Emitted when the active molecule in the application has changed.
   */
  void moleculeChanged(QtGui::Molecule* molecue);

protected:
  void closeEvent(QCloseEvent* event);

  // Handle drag and drop -- accept files dragged on the window
  void dragEnterEvent(QDragEnterEvent* event);
  void dropEvent(QDropEvent* event);

protected slots:

  /**
   * Set the preferred locale
   */
  void setLocale(const QString& locale);

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
   * Open file in the recent files list.
   */
  void openRecentFile();

  /**
   * Update the list of recent files.
   */
  void updateRecentFiles();

  /**
   * Save the current molecule to its current fileName. If it is not a standard
   * format, offer to export and warn about possible data loss.
   * If @a async is true (default), the file is saved asynchronously.
   * @return If @a async is true, this function returns true if a suitable
   * writer was found (not if the write was successful). If @a async is
   * false, the return value indicates whether or not the file was written
   * successfully.
   */
  bool saveFile(bool async = true);

  /**
   * Prompt for a file location, and attempt to save the active molecule to the
   * specified location.
   * If @a async is true (default), the file is saved asynchronously.
   * @return If @a async is true, this function returns true if a suitable
   * writer was found (not if the write was successful). If @a async is
   * false, the return value indicates whether or not the file was written
   * successfully.
   */
  bool saveFileAs(bool async = true);

  /**
   * Export a file, using the full selection of formats capable of writing.
   * If @a async is true (default), the file is saved asynchronously.
   * @return If @a async is true, this function returns true if a suitable
   * writer was found (not if the write was successful). If @a async is
   * false, the return value indicates whether or not the file was written
   * successfully.
   */
  bool exportFile(bool async = true);

  /**
   * If specified, use the FileFormat @a writer to save the file. This method
   * takes ownership of @a writer and will delete it before returning.
   * If @a async is true (default), the file is saved asynchronously.
   * @return If @a async is true, this function returns true if the write begins
   * successfully (not if the writer completes). If @a async is
   * false, the return value indicates whether or not the file was written
   * successfully.
   */
  bool saveFileAs(const QString& fileName, Io::FileFormat* writer,
                  bool async = true);

  /**
   * Set the active tool for the currently active widget by name.
   * @param toolName Name of the tool to select.
   */
  void setActiveTool(QString toolName);

  /**
   * @brief Set the active display types, and cause the scene to be updated.
   * @param displayTypes A list of
   */
  void setActiveDisplayTypes(QStringList displayTypes);
  void setDisabledDisplayTypes(QStringList displayTypes);

  void undoEdit();
  void redoEdit();
  void activeMoleculeEdited();
  void refreshDisplayTypes();

#ifdef QTTESTING
protected slots:
  void record();
  void play();
  void playTest();
  void popup();
#endif

private slots:
  void showAboutDialog();

  void showLanguageDialog();

  void openURL(const QString& url);

  void openForum();

  void openWebsite();

  void openBugReport();

  void openFeatureRequest();

  void checkUpdate();

  void finishUpdateRequest(QNetworkReply*);

  void registerToolCommand(QString command, QString description);

  void registerExtensionCommand(QString command, QString description);

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
   * @brief The background file writer thread has completed, set the active
   * molecule, and clean up after the threaded write.
   */
  bool backgroundWriterFinished();

  /**
   * @brief Called when a toolbar action is clicked. The sender is expected to
   * be the action, and the parent of the action should be the toolPlugin to
   * activate.
   */
  void toolActivated();

  /**
   * @brief When a view configuration is activated let the user configure the
   * view plugins properties.
   */
  void viewConfigActivated();

  /**
   * @brief Triggered if a renderer cannot get a valid context.
   */
  void rendererInvalid();

  /**
   * @brief Change the active molecule
   */
  void moleculeActivated(const QModelIndex& index);

  /**
   * @brief Change the active layer
   */
  void layerActivated(const QModelIndex& index);

  /**
   * @brief Change the configuration dialog to reflect active scene item.
   */
  void sceneItemActivated(const QModelIndex& index);

  /**
   * @brief Change the active view widget, initialize plugins if needed.
   */
  void viewActivated(QWidget* widget);

  QImage renderToImage(const QSize& size);

  void exportGraphics();

  void copyGraphics();

  void setBackgroundColor();

  void setRenderingSettings();

  void setProjectionOrthographic();

  void setProjectionPerspective();

private:

  QtGui::Molecule* m_molecule;
  QtGui::RWMolecule* m_rwMolecule;
  QtGui::MoleculeModel* m_moleculeModel;
  QtGui::LayerModel* m_layerModel;
  QtGui::ScenePlugin* m_activeScenePlugin;
  bool m_queuedFilesStarted;
  QStringList m_queuedFiles;
  QTimer* m_autosaveTimer;  // for the autosave timer
  int m_autosaveInterval;   // for autosave interval in minutes
  QStringList m_recentFiles;
  QList<QAction*> m_actionRecentFiles;

  QStringList m_translationList;
  QStringList m_localeCodes;

  MenuBuilder* m_menuBuilder;

  // These variables take care of background file reading.
  QThread* m_fileReadThread;
  QThread* m_fileWriteThread;
  BackgroundFileFormat* m_threadedReader;
  BackgroundFileFormat* m_threadedWriter;
  QProgressDialog* m_progressDialog;
  QtGui::Molecule* m_fileReadMolecule;

  QToolBar* m_fileToolBar;
  QToolBar* m_toolToolBar;

  bool m_moleculeDirty;

  QtGui::MultiViewWidget* m_multiViewWidget;
  QTreeView* m_sceneTreeView;
  QTreeView* m_layerTreeView;
  QTreeView* m_moleculeTreeView;
  QDockWidget* m_toolDock;
  QDockWidget* m_viewDock;
  QDockWidget* m_sceneDock;
  QDockWidget* m_layerDock;
  QDockWidget* m_moleculeDock;
  QList<QtGui::ToolPlugin*> m_tools;
  QList<QtGui::ExtensionPlugin*> m_extensions;
  // map from script commands to tools and extensions
  QMap<QString, QString> m_toolCommandMap;
  QMap<QString, QtGui::ExtensionPlugin*> m_extensionCommandMap;
  // used for help - provide description for a command
  QMap<QString, QString> m_commandDescriptionsMap;

  QAction* m_undo;
  QAction* m_redo;
  QAction* m_copyImage;
  QAction* m_viewPerspective;
  QAction* m_viewOrthographic;

  ViewFactory* m_viewFactory;

  QNetworkAccessManager* m_network = nullptr;
#ifdef _3DCONNEXION
  TDxController* m_TDxController;
#endif

#ifdef QTTESTING
  pqTestUtility* m_testUtility;
  QString m_testFile;
  bool m_testExit;
#endif

  /**
   * Set up the main window widgets, connect signals and slots, etc.
   */
  void setupInterface();

  /** Show a dialog to remap custom elements, if present. */
  void reassignCustomElements();

  /**
   * Build the main menu, delayed until all plugins have registered actions.
   */
  void buildMenu();

  /**
   * Add the menu entries for the extension passed in.
   */
  void buildMenu(QtGui::ExtensionPlugin* extension);

  /**
   * Initialize the tool plugins.
   */
  void buildTools();

  /**
   * Convenience function that converts a file extension to a wildcard
   * expression, e.g. "out" to "*.out". This method also checks for "extensions"
   * that aren't really extensions but full filenames, e.g. HISTORY files from
   * DL-POLY. These are returned unmodified.
   */
  static QString extensionToWildCard(const QString& extension);

  /**
   * Convenience function to generate a filter string for the supplied formats.
   */
  QString generateFilterString(
    const std::vector<const Io::FileFormat*>& formats, bool addAllEntry = true);

  /**
   * Prompt to save the current molecule if is has been modified. Returns false
   * if the molecule is not saved, or the user cancels.
   */
  bool saveFileIfNeeded();
};

} // End Avogadro namespace

#endif