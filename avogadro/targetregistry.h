/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#ifndef AVOGADRO_TARGETREGISTRY_H
#define AVOGADRO_TARGETREGISTRY_H

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QPair>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QVariant>

class QAction;
class QDockWidget;
class QMenu;
class QMenuBar;
class QToolBar;
class QToolButton;
class QWidget;

namespace Avogadro {

namespace QtGui {
class ExtensionPlugin;
class ToolPlugin;
} // namespace QtGui

class HighlightOverlay;
class MainWindow;

/**
 * @brief Names the parts of the user interface that a guided tutorial can
 * point at, and draws the outline around them.
 *
 * A target is a stable name such as "tool:draw" or "editor:element". The
 * registry resolves the name to a widget or action when it is needed (never
 * earlier, since menus are rebuilt and docks come and go), reports the text
 * and value of each target to the extensions that want it, and draws the
 * highlight overlay on request. It also answers the highlight RPC verbs.
 *
 * Extensions are talked to by signal and slot name, and only when their
 * meta-object has them, so this works with an avogadrolibs that predates the
 * highlight members of ExtensionPlugin.
 */
class TargetRegistry : public QObject
{
  Q_OBJECT

public:
  explicit TargetRegistry(MainWindow* window);
  ~TargetRegistry() override;

  /// The toolbar that holds the tool buttons.
  void setToolBar(QToolBar* toolBar);
  /// The dock that shows the active tool's settings widget.
  void setToolDock(QDockWidget* dock);
  /// Make a tool available as "tool:<short name>".
  void addTool(QtGui::ToolPlugin* tool);
  /// Make a main window action available under a name such as "new"
  /// ("action:new"). The action's triggered() signal reports the change.
  void addAction(const QString& name, QAction* action);
  /// Connect an extension's highlight signals, and remember it as a listener.
  void addExtension(QtGui::ExtensionPlugin* extension);

  /// Call once the menus and tools exist: sends the first setUiTargets.
  void start();
  /// Call after the menus have been rebuilt.
  void menusRebuilt();
  /// Call when the active tool may have changed.
  void activeToolChanged();

  /// The short name of a tool: "draw" for the Editor, and so on.
  static QString toolShortName(const QString& objectName);

  /// Every known target: name -> { text, value, resolved, visible }.
  QVariantMap describeTargets() const;

  /// Outline a target. Returns false and fills @p error if it cannot be.
  bool highlight(const QString& target, const QString& callout, QString* error);
  /// Like highlight(), but also moves focus or opens the menu, and announces.
  bool reveal(const QString& target, QString* error);
  /// Remove the outline; close a menu or tool bar overflow opened by reveal().
  void clearHighlight();

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
  // Slots for the extension signals that are connected by name.
  void onRequestHighlight(const QString& target, const QString& callout);
  void onRequestReveal(const QString& target);
  // Report a real change (an action ran, an Editor widget changed) to the
  // extensions.
  void onActionTriggered();
  void onEditorValueChanged();
  /// Push the current target map to the extensions' setUiTargets slot.
  void sendTargets();

private:
  /// What a name refers to right now; defined in the source file.
  struct Resolved;

  /// Look a name up. Nothing is cached, since menus and docks are rebuilt.
  Resolved resolve(const QString& name) const;
  QStringList knownNames() const;
  QAction* findAction(const QString& name) const;
  QtGui::ToolPlugin* editorTool() const;
  QWidget* editorWidget(const QString& name) const;
  QtGui::ToolPlugin* findTool(const QString& objectName) const;
  QString activeToolName() const;
  QString menuPathText(QAction* action) const;
  QString actionText(QAction* action) const;
  /// The menu bar holding @p action; @p chain gets the menus, outermost first.
  QMenuBar* menuBarFor(QAction* action, QList<QPointer<QMenu>>* chain) const;
  QWidget* toolbarButton(QAction* action) const;
  /// The tool bar's overflow ("»") button, shown or not, or nullptr.
  QToolButton* toolbarExtension() const;

  /// Shared body of highlight() and reveal().
  bool show(const QString& target, const QString& callout, bool reveal,
            QString* error);
  /// Open the menus leading to @p action (non-native menu bars only).
  void openMenuChain(QAction* action);
  /// Open level @p level of the chain, then schedule the next level.
  void stepMenuChain(int level);
  void closeOpenedMenus();
  /// Collapse the tool bar overflow if a reveal expanded it.
  void collapseToolBar();
  HighlightOverlay* overlayFor(QWidget* window);
  void connectChangeSignals();
  void notifyChange(const QString& target, const QVariant& value);
  void announce(const QString& text, QObject* object);

  MainWindow* m_window;
  // The pointers below are QPointers because tools, menus and docks can be
  // deleted or rebuilt while the registry lives on.
  QPointer<QToolBar> m_toolBar;
  QPointer<QDockWidget> m_toolDock;
  QList<QPointer<QtGui::ToolPlugin>> m_tools;
  /// Every extension, for finding action:<objectName> targets.
  QList<QPointer<QtGui::ExtensionPlugin>> m_extensionList;
  /// name -> action for the main window's own actions ("new", "open").
  QList<QPair<QString, QPointer<QAction>>> m_actions;
  /// Extensions with the setUiTargets slot, and with uiTargetChanged.
  QList<QPointer<QObject>> m_targetListeners;
  QList<QPointer<QObject>> m_changeListeners;
  /// One overlay per top-level window that has had a target outlined.
  QList<QPointer<HighlightOverlay>> m_overlays;
  /// State of the menus opened by reveal(), so they can be closed again.
  QPointer<QMenuBar> m_openedMenuBar;
  QList<QPointer<QMenu>> m_menuChain; ///< top-level menu first
  QPointer<QAction> m_menuLeaf;
  /// The tool bar whose overflow a reveal expanded, so it can be collapsed.
  QPointer<QToolBar> m_expandedToolBar;
  /// The last tool reported, so a repeated switch is not announced twice.
  QString m_lastTool;
  /// A sendTargets() call is already queued.
  bool m_pushPending = false;
  /// The application-wide Escape/close filter is installed.
  bool m_filterInstalled = false;
};

} // namespace Avogadro

#endif // AVOGADRO_TARGETREGISTRY_H
