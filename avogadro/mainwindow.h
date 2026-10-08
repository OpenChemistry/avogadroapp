/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#ifndef AVOGADRO_MAINWINDOW_H
#define AVOGADRO_MAINWINDOW_H

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QStringList>
#include <QtCore/QVariantMap>
#include <QtWidgets/QMainWindow>

#include <vector>

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
  /**
   * @param skipDialogs Decline every modal dialog a script could not answer
   * (see --skip-dialogs). This is recorded in
   * QtGui::Utilities::dialogsSkipped(), which plugins read too; there is no
   * other copy of the flag.
   */
  MainWindow(const QStringList& fileNames, bool disableSettings = false,
             bool skipAutosave = false, bool skipDialogs = false);
  ~MainWindow() override;

public slots:
  void setMolecule(Avogadro::QtGui::Molecule* molecule);

  /**
   * Show a molecule that was just read from a file (the command line, File >
   * Open, or an RPC openFile/loadMolecule). Like setMolecule(), except that
   * an empty, unmodified active molecule -- the blank document Avogadro starts
   * with -- is closed instead of being left behind next to the new one.
   * Plain switching between molecules must keep using setMolecule().
   */
  void setOpenedMolecule(Avogadro::QtGui::Molecule* molecule);
  void autosaveDocument(); // Autosave the current document
  /**
   * Update internal state to reflect that the molecule has been modified.
   * @param changes The QtGui::Molecule::MoleculeChanges flags; the default
   * (all bits) is treated as a real edit.
   */
  void markMoleculeDirty(unsigned int changes = ~0u);

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

  /**
   * Set the default directory for open/save dialogs.
   * Called when opening files from the command line to respect the
   * current working directory.
   */
  void setDefaultFileDialogPath(const QString& path);

  /**
   * Save a picture of the active view to @p fileName (".png" is added when
   * there is no suffix). Under --skip-dialogs a failed save is logged instead
   * of shown in a message box.
   * @return True if the image was written.
   */
  bool exportGraphics(QString fileName);

  /**
   * Export a file, using the full selection of formats capable of writing.
   * The format will be guessed based on the filename extension.
   * If @a async is true (default), the file is saved asynchronously.
   * @param token When @a async is true and non-zero, commandCompleted() is
   * emitted with this token once the background write finishes, the same
   * way a plugin command reports back. Used by the RPC listener's "wait"
   * support; ignored otherwise.
   * @return If @a async is true, this function returns true if a suitable
   * writer was found (not if the write was successful). If @a async is
   * false, the return value indicates whether or not the file was written
   * successfully.
   */
  bool exportFile(const QString& fileName, bool async = true,
                  quint64 token = 0);

  /**
   * Export a file, using the full selection of formats capable of writing.
   * Will use @a format to determine the file format to use.
   * @return String-representation of the exported file, or an empty string if
   * the export failed.
   */
  std::string exportString(const std::string& format);

#ifdef QTTESTING
  void playTest(const QString& fileName, bool exit = true);
#endif

public:
  QtGui::Molecule* molecule() { return m_molecule; }

  /**
   * Whether @p molecule has unsaved changes: m_moleculeDirty for the active
   * molecule, the state saved by setMolecule() for any other.
   */
  bool isModified(const QtGui::Molecule* molecule) const;

  /// Whether the active molecule's undo stack has an edit to undo.
  bool canUndo() const;

  /// Whether the active molecule's undo stack has an edit to redo.
  bool canRedo() const;

  /**
   * One map per open molecule, in the molecule list's order, with "index",
   * "active", "atomCount", "formula", "fileName" and "modified". For the RPC
   * "listMolecules" method.
   */
  QVariantList moleculeSummaries() const;

  /**
   * One map per tool of the active view, with "name" (the tool's object name:
   * what activateTool and the toolbar use), "displayName" (ToolPlugin::name(),
   * translated, for people only) and "active". Empty when there is no GL view.
   * For the RPC "listTools" and "activateTool" methods.
   */
  QVariantList toolSummaries() const;

  /**
   * "index" (in the molecule list) of the active molecule and "count" of open
   * molecules: the reply data of the RPC molecule verbs.
   */
  QVariantMap moleculePosition() const;

  /**
   * The active molecule's undo stack, for the RPC "undo", "redo" and
   * "moleculeInfo" methods: "canUndo", "canRedo", "undoText", "redoText"
   * (the stack's own text, without menu mnemonics) and "modified".
   */
  QVariantMap undoState() const;

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
   * Attempt to recover autosave files from a crash
   */
  void checkAutosaveRecovery();

  /**
   * Scan standard directories for pyproject.toml-based plugin packages.
   */
  void loadPackages();

  void startAutosaveTimer();

  /**
   * The autosave file name (no directory) of @p molecule. Chosen on first use
   * and kept for the molecule's lifetime, in a dynamic property on the molecule
   * so it is never written into saved files.
   */
  QString autosaveNameFor(QtGui::Molecule* molecule);

  /**
   * Delete exactly the autosave file (either form) that @p molecule wrote, if
   * any, and forget its name.
   */
  void removeAutosave(QtGui::Molecule* molecule);

  /**
   * Set the list of possible translations
   */
  void setTranslationList(const QStringList& list, const QStringList& codes)
  {
    m_translationList = list;
    m_localeCodes = codes;
  }

  /**
   * The outcome of dispatching a script command.
   */
  enum class CommandStatus
  {
    NotHandled, ///< No tool or extension claims this command.
    Finished,   ///< The command ran to completion.
    Failed,     ///< The command was claimed, but could not be carried out.
    Started,    ///< Still running; commandCompleted() will follow.
    Busy        ///< That plugin is already running a command.
  };

  /**
   * Handle script commands
   * @param command The command to execute
   * @param options The options to the command
   * @param token Identifies this command in a later commandCompleted()
   * @param message Set to a message from the plugin, if it supplied one
   * @param result Set to any results the plugin returned
   *
   * @return The outcome of the command. A return of CommandStatus::Started
   * means the command is still running, and commandCompleted() will be
   * emitted with @a token once it ends.
   */
  CommandStatus handleCommand(const QString& command,
                              const QVariantMap& options, quint64 token = 0,
                              QString* message = nullptr,
                              QVariantMap* result = nullptr);

  /**
   * Stop waiting for the command identified by @a token.
   *
   * The plugin may still be working -- this does not cancel anything -- but
   * its result will be ignored and it is free to accept another command.
   * Used when the caller has given up waiting, so that a plugin which never
   * reports back does not stay busy for the life of the session.
   */
  void abandonCommand(quint64 token);

  /**
   * The commands registered by tools and extensions, for RPC introspection
   * (the "listCommands" method). Built-in commands answered directly by the
   * RPC listener are not included; it has its own static table for those.
   *
   * Each entry is a map with "name", "description", "kind" ("tool" or
   * "extension") and "plugin" (the owning plugin's display name).
   */
  QVariantList pluginCommands() const;

  /**
   * The size, in pixels, of the currently active view. An empty size if
   * there is no active view.
   */
  QSize activeViewSize() const;

  /**
   * The active view, if it is an OpenGL widget. Returns nullptr for a
   * non-GL view (e.g. a VTK crystal view) or when there is no active view.
   */
  QtOpenGL::GLWidget* activeGLWidget() const;

  /**
   * Render the active view to an image.
   * @param requestedSize The desired output size, in pixels. If null or
   * empty (the default), the native framebuffer grab is returned untouched,
   * at whatever resolution the view actually rendered (i.e. widget size
   * times device pixel ratio). Otherwise the native grab is scaled with
   * Qt::KeepAspectRatio and centred on a canvas of exactly this size.
   * @param transparentBackground If true (the default, matching prior
   * behaviour) the background is left transparent; otherwise the image is
   * composited over the view's current background colour.
   * @param nativeSize If non-null, receives the native grab's actual
   * dimensions (before any scaling to requestedSize was applied).
   */
  QImage renderToImage(const QSize& requestedSize = QSize(),
                       bool transparentBackground = true,
                       QSize* nativeSize = nullptr);

  /**
   * True once the file formats that plugins register after startup (Open
   * Babel's) are available, or once Avogadro has stopped waiting for them.
   * Until then, reading or writing a file may fail for those formats.
   */
  bool pluginFormatsSettled() const { return m_pluginFormatsSettled; }

signals:
  /**
   * Emitted once, when pluginFormatsSettled() becomes true.
   */
  void pluginFormatsReady();

  /**
   * Emitted when the active molecule in the application has changed.
   */
  void moleculeChanged(QtGui::Molecule* molecue);

  /**
   * Emitted when a command that returned CommandStatus::Started has ended.
   * @param token The token that was passed to handleCommand()
   * @param success True if the command finished, false if it failed
   * @param message An optional message from the plugin
   * @param result Any results the plugin returned
   */
  void commandCompleted(quint64 token, bool success, const QString& message,
                        const QVariantMap& result);

protected:
  void closeEvent(QCloseEvent* event) override;

  // Handle drag and drop -- accept files dragged on the window
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent* event) override;

  // handle theme changes
  void changeEvent(QEvent* event) override;

  // Route the reserved camera-navigation keyboard shortcut to the GL widget
  // even when keyboard focus is elsewhere (docks, tool settings, layer view).
  bool eventFilter(QObject* watched, QEvent* event) override;

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

  void closeActiveMolecule();

  void finishUpdateRequest(QNetworkReply*);

  void registerToolCommand(QString command, QString description);

  void registerExtensionCommand(QString command, QString description);

  /** A plugin reports that a command is running in the background. */
  void pluginCommandStarted();

  /** A plugin reports that a command has finished. */
  void pluginCommandFinished(const QString& message, const QVariantMap& result);

  /** A plugin reports that a started command could not be completed. */
  void pluginCommandFailed(const QString& message);

  /** Drop a destroyed plugin from the in-flight command map. */
  void pluginDestroyed(QObject* plugin);

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
   * @brief Mark the plugin file formats as settled (once), see
   * pluginFormatsSettled().
   */
  void settlePluginFormats();

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
   * @brief Shortcut to move to the next molecule (down in the list)
   */
  void nextMolecule();

  /**
   * @brief Shortcut to move to the previous molecule (up in the list)
   */
  void previousMolecule();

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

  void exportGraphics();

  void copyGraphics();

  void setBackgroundColor();

  void setRenderingSettings();

  // void setFogColor();
  void setProjectionOrthographic();

  void setProjectionPerspective();

private:
  /**
   * Close @p molecule without asking to save it: make a neighbour (or a new
   * empty molecule, if it was the only one) active when it is the active
   * molecule, then remove its autosave and drop it from the molecule list.
   * Callers that must not lose work ask to save, or check isModified(), first.
   */
  void closeMolecule(QtGui::Molecule* molecule);

  /// The icon (and weight) of a message-only dialog; see warnUser().
  enum class Severity
  {
    Information,
    Warning,
    Critical
  };

  /**
   * Tell the user something with a message-only dialog (just an OK button).
   * Under --skip-dialogs nobody can close the box, and its nested event loop
   * would hold an RPC reply hostage, so the message is logged instead, as
   * "--skip-dialogs: skipped '<title>' dialog; <text>". Only for dialogs that
   * ask nothing: a question has to be declined or answered by its own caller.
   */
  void warnUser(Severity severity, const QString& title, const QString& text);

  /**
   * Connect a plugin's command lifecycle signals. Safe to call repeatedly --
   * the connections are unique. Tool instances belong to each GLWidget rather
   * than to m_tools, so this is done on first use rather than at load time.
   */
  template<typename PluginType>
  void connectCommandSignals(PluginType* plugin)
  {
    // Each connection is remembered so that ~MainWindow can sever it while
    // this object's members are still alive. See disconnectCommandSignals().
    rememberConnection(connect(plugin, &PluginType::commandStarted, this,
                               &MainWindow::pluginCommandStarted,
                               Qt::UniqueConnection));
    rememberConnection(connect(plugin, &PluginType::commandFinished, this,
                               &MainWindow::pluginCommandFinished,
                               Qt::UniqueConnection));
    rememberConnection(connect(plugin, &PluginType::commandFailed, this,
                               &MainWindow::pluginCommandFailed,
                               Qt::UniqueConnection));
    rememberConnection(connect(plugin, &QObject::destroyed, this,
                               &MainWindow::pluginDestroyed,
                               Qt::UniqueConnection));
  }

  /**
   * Keep a connection made by connectCommandSignals() so it can be undone.
   * A repeated Qt::UniqueConnection returns an invalid handle, which is
   * dropped rather than accumulated.
   */
  void rememberConnection(const QMetaObject::Connection& connection)
  {
    if (connection)
      m_pluginCommandConnections.append(connection);
  }

  /**
   * Sever every connection made by connectCommandSignals().
   *
   * This must happen before ~MainWindow lets its members go. Plugins are
   * parented to the views, which QWidget::~QWidget deletes *after* our own
   * members are destroyed but *before* QObject::~QObject would have severed
   * these connections. Without this, a plugin's destroyed() signal still
   * reaches pluginDestroyed(), which then reads a destroyed
   * m_inFlightCommands and the application crashes on quit.
   */
  void disconnectCommandSignals();

  /** Start tracking the signals a plugin emits while handling a command. */
  void beginPluginCommand(QObject* plugin);

  /** Stop tracking, and work out how the command ended. */
  CommandStatus endPluginCommand(QObject* plugin, bool claimed, quint64 token,
                                 QString* message, QVariantMap* result);

  /**
   * @name Layer actions
   * The per-action bodies behind the Layers dock's click handler
   * (layerActivated()), shared with the addLayer/removeLayer/
   * setActiveLayer/setLayerVisible/setLayerLocked RPC commands so that both
   * drive the exact same code. Every layer is identified by its id (as
   * QtGui::LayerModel::layerForRow() and layerCount() report it), not by a
   * Layers dock row.
   */
  ///@{

  /** Add a new layer, inheriting the active layer's settings. */
  void addLayer();

  /** Remove @p layer. */
  void removeLayer(size_t layer);

  /** Make @p layer the active layer. */
  void setActiveLayer(size_t layer);

  /**
   * Show or hide @p layer, updating the active view if it changed. A no-op
   * if it already matches @p visible.
   */
  void setLayerVisible(size_t layer, bool visible);

  /**
   * Lock or unlock @p layer against edits. A no-op if it already matches
   * @p locked.
   */
  void setLayerLocked(size_t layer, bool locked);

  /**
   * Validate and translate the "layer" RPC option: on success, fills
   * @p layer with the requested layer id and returns true; on failure,
   * fills @p message with a translated explanation and returns false.
   * Callers must check for an open molecule themselves first, since that
   * failure uses a different message.
   */
  bool layerIdFromOptions(const QVariantMap& options, QString* message,
                          size_t* layer) const;

  ///@}

  QtGui::Molecule* m_molecule;
  QtGui::RWMolecule* m_rwMolecule;
  QtGui::MoleculeModel* m_moleculeModel;
  QtGui::LayerModel* m_layerModel;
  QtGui::ScenePlugin* m_activeScenePlugin;
  bool m_queuedFilesStarted;
  QStringList m_queuedFiles;
  bool m_pluginFormatsSettled = false;
  QTimer* m_autosaveTimer = nullptr; // for the autosave timer
  // Skip autosave recovery and writing autosaves entirely, so that a
  // scripted or automated run neither prompts nor leaves files behind.
  bool m_skipAutosave = false;
  QStringList m_recentFiles;
  QList<QAction*> m_actionRecentFiles;

  QStringList m_translationList;
  QStringList m_localeCodes;

  MenuBuilder* m_menuBuilder;
  bool m_initialized = false; ///< true after the initial buildMenu() completes

  // These variables take care of background file reading.
  QThread* m_fileReadThread;
  QThread* m_fileWriteThread;
  BackgroundFileFormat* m_threadedReader;
  BackgroundFileFormat* m_threadedWriter;
  QProgressDialog* m_progressDialog;
  QtGui::Molecule* m_fileReadMolecule;
  // Set by exportFile() when the RPC listener is waiting on an async write;
  // backgroundWriterFinished() reports back through commandCompleted().
  quint64 m_pendingExportToken = 0;

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

  // Tracks the command currently inside handleCommand(), so that a plugin
  // emitting commandStarted() / commandFinished() / commandFailed() during
  // the call can be attributed without waiting for the signal to come back.
  QObject* m_currentCommandPlugin = nullptr;
  bool m_currentCommandStarted = false;
  bool m_currentCommandEnded = false;
  bool m_currentCommandFailed = false;
  QString m_currentCommandMessage;
  QVariantMap m_currentCommandResult;
  // Plugins running a command in the background, and the token to report.
  QHash<QObject*, quint64> m_inFlightCommands;
  // Connections made by connectCommandSignals(), undone in ~MainWindow.
  QList<QMetaObject::Connection> m_pluginCommandConnections;

  QAction* m_undo;
  QAction* m_redo;
  QAction* m_copyImage;
  QAction* m_viewPerspective;
  QAction* m_viewOrthographic;
  QAction* m_nextMolecule;

  ViewFactory* m_viewFactory;

  QNetworkAccessManager* m_network = nullptr;
#ifdef _3DCONNEXION
  TDxController* m_TDxController;
#endif

#ifdef QTTESTING
  pqTestUtility* m_testUtility = nullptr;
  QString m_testFile;
  bool m_testExit = true;
#endif

  /**
   * Set up the main window widgets, connect signals and slots, etc.
   */
  void setupInterface();

  /**
   * Add the dock widgets contributed by extension plugins to the window.
   * Called once, after the plugins are loaded and the built-in docks exist.
   */
  void setupExtensionDocks();

  /**
   * If the window is not visible on any available screen, move it to the
   * primary screen. Called on startup and when a screen is removed.
   */
  void ensureWindowOnScreen();

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
