/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#include "targetregistry.h"

#include "highlightoverlay.h"
#include "mainwindow.h"
#include "reducemotion.h"

#include <avogadro/qtgui/extensionplugin.h>
#include <avogadro/qtgui/toolplugin.h>
#include <avogadro/qtopengl/glwidget.h>

#include <QtCore/QEvent>
#include <QtCore/QMetaObject>
#include <QtCore/QTimer>
#include <QtGui/QAccessible>
#include <QtGui/QAction>
#include <QtGui/QKeyEvent>
#include <QtGui/QKeySequence>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QToolButton>

#include <functional>

namespace Avogadro {

using QtGui::ExtensionPlugin;
using QtGui::ToolPlugin;

namespace {

// Menus nest a few levels at most; this bounds the walk up from an action so a
// malformed (cyclic) menu structure cannot loop forever.
constexpr int kMaxMenuDepth = 8;
// Delay, in milliseconds, between opening one level of a menu chain and the
// next (see stepMenuChain()).
constexpr int kMenuStepMs = 30;
// The Editor's element box uses this for "Other…"; it is a selector, not an
// element, so it is never reported.
constexpr int kOtherElement = 255;

// Maps a tool plugin's object name (the first argument of avogadro_plugin())
// to the short name used in "tool:<short name>".
struct ToolName
{
  const char* identifier;
  const char* shortName;
};

const ToolName toolNames[] = {
  { "Navigator", "navigate" },     { "Editor", "draw" },
  { "TemplateTool", "template" },  { "Selection", "select" },
  { "Manipulator", "manipulate" }, { "MeasureTool", "measure" },
};

// Object names of the Editor tool's settings widgets that can be targeted as
// "editor:<name>". These are looked up with findChild() inside the Editor's own
// tool widget, so they must match the names set in the Editor plugin.
const char* const editorNames[] = { "element", "bondOrder", "adjustHydrogens",
                                    "adjustBondLengths" };

// Removes the '&' keyboard-mnemonic markers from a Qt label, as in "&File".
// A doubled "&&" is Qt's escape for a literal ampersand, so it is parked on a
// placeholder character while the single ones are removed.
QString stripMnemonic(const QString& text)
{
  QString result = text;
  result.replace(QLatin1String("&&"), QStringLiteral("\x01"));
  result.remove(QLatin1Char('&'));
  result.replace(QLatin1Char('\x01'), QLatin1Char('&'));
  return result;
}

// The separator between menu levels in a path, e.g. "File ▸ New". U+25B8 is
// the small right-pointing triangle.
QString arrowSeparator()
{
  return QStringLiteral(" %1 ").arg(QChar(0x25B8));
}

// The action's shortcut as the user's platform writes it (⌘N on macOS,
// Ctrl+N elsewhere), or an empty string if it has none.
QString shortcutText(const QAction* action)
{
  if (action == nullptr || action->shortcut().isEmpty())
    return QString();
  return action->shortcut().toString(QKeySequence::NativeText);
}

// "Text (Shortcut)", or just the text when there is no shortcut.
QString withShortcut(const QString& text, const QAction* action)
{
  const QString shortcut = shortcutText(action);
  return shortcut.isEmpty() ? text
                            : QStringLiteral("%1 (%2)").arg(text, shortcut);
}

/// The value of one of the Editor's widgets, or an invalid QVariant.
QVariant editorValue(const QString& name, const QWidget* widget)
{
  if (const auto* combo = qobject_cast<const QComboBox*>(widget)) {
    bool ok = false;
    // The Editor stores the element number or bond order as item data.
    const int value = combo->currentData().toInt(&ok);
    // The item data may not be an integer; "Other…" is a prompt for another
    // element rather than a choice, so report no value for it.
    if (!ok || (name == QLatin1String("element") && value == kOtherElement))
      return QVariant();
    return value;
  }
  if (const auto* check = qobject_cast<const QCheckBox*>(widget))
    return check->isChecked();
  return QVariant();
}

// The visible name of one of the Editor's settings widgets, which is what a
// tutorial shows the student. Tried in order: the form layout's label for the
// field, a check box's own text, then the accessible name.
QString editorLabel(const QWidget* toolWidget, const QWidget* widget)
{
  QString text;
  if (const auto* form =
        qobject_cast<const QFormLayout*>(toolWidget->layout())) {
    if (const auto* label = qobject_cast<const QLabel*>(
          form->labelForField(const_cast<QWidget*>(widget))))
      text = label->text();
  }
  if (text.isEmpty()) {
    if (const auto* check = qobject_cast<const QCheckBox*>(widget))
      text = check->text();
  }
  if (text.isEmpty())
    text = widget->accessibleName();
  text = stripMnemonic(text).trimmed();
  // "Element:" reads better as "Element". U+FF1A is the full-width colon that
  // CJK translations use.
  while (text.endsWith(QLatin1Char(':')) || text.endsWith(QChar(0xFF1A)))
    text.chop(1);
  return text.trimmed();
}

// The widget's rectangle in global (screen) coordinates, or an empty rectangle
// when it is gone or hidden. Global coordinates are used because the widget
// and the overlay can be in different top-level windows (a floating dock), so
// the overlay converts to its own coordinates with mapFromGlobal().
QRect globalRectOf(const QPointer<QWidget>& widget)
{
  if (widget.isNull() || !widget->isVisible())
    return QRect();
  return QRect(widget->mapToGlobal(QPoint(0, 0)), widget->size());
}

} // namespace

// What a target name turned out to mean at this moment. Built fresh on every
// call to resolve(), since menus, docks and tool widgets come and go.
struct TargetRegistry::Resolved
{
  bool known = false;     ///< The name is one we understand.
  bool stateOnly = false; ///< Reports a value but has nothing to outline.
  bool resolved = false;  ///< There is a live widget or action behind it.
  bool visible = false;   ///< It can be outlined right now.
  QString text;
  QVariant value;
  QPointer<QAction> action;
  QPointer<QWidget> widget; ///< What to outline, if there is a widget.
  /// The tool bar's extension ("»") button, shown instead of a tool button
  /// that is hidden in the tool bar overflow (visible or not).
  QPointer<QToolButton> extension;
  QString reason;           ///< Why it is not visible, for the caller.
};

TargetRegistry::TargetRegistry(MainWindow* window)
  : QObject(window), m_window(window)
{
}

TargetRegistry::~TargetRegistry() = default;

void TargetRegistry::setToolBar(QToolBar* toolBar)
{
  m_toolBar = toolBar;
}

void TargetRegistry::setToolDock(QDockWidget* dock)
{
  m_toolDock = dock;
}

void TargetRegistry::addTool(ToolPlugin* tool)
{
  if (tool != nullptr)
    m_tools.append(tool);
}

void TargetRegistry::addAction(const QString& name, QAction* action)
{
  if (action != nullptr)
    m_actions.append(qMakePair(name, QPointer<QAction>(action)));
}

void TargetRegistry::addExtension(ExtensionPlugin* extension)
{
  if (extension == nullptr)
    return;
  m_extensionList.append(extension);

  // The members below only exist in an avogadrolibs that has the highlight
  // additions to ExtensionPlugin. Checking the meta-object first keeps older
  // builds from printing "no such signal" warnings from connect().
  // TODO(GTut PR 2): switch to typed connect() once ExtensionPlugin declares
  // requestHighlight, requestReveal, setUiTargets and uiTargetChanged in the
  // avogadrolibs this app is built against. Until then they are found by
  // name, so an older avogadrolibs gets no connections and no warnings.
  const QMetaObject* meta = extension->metaObject();
  if (meta->indexOfSignal("requestHighlight(QString,QString)") >= 0) {
    connect(extension, SIGNAL(requestHighlight(QString, QString)), this,
            SLOT(onRequestHighlight(QString, QString)));
  }
  if (meta->indexOfSignal("requestReveal(QString)") >= 0) {
    connect(extension, SIGNAL(requestReveal(QString)), this,
            SLOT(onRequestReveal(QString)));
  }
  // Not connected: these are called with invokeMethod() on the extensions
  // that have them, so only those are remembered.
  if (meta->indexOfMethod("setUiTargets(QVariantMap)") >= 0)
    m_targetListeners.append(extension);
  if (meta->indexOfMethod("uiTargetChanged(QString,QVariant)") >= 0)
    m_changeListeners.append(extension);
}

void TargetRegistry::start()
{
  m_lastTool = activeToolName();
  // Queued and coalesced by m_pushPending, so a burst of menu rebuilds sends
  // one update.
  if (!m_pushPending) {
    m_pushPending = true;
    QMetaObject::invokeMethod(this, &TargetRegistry::sendTargets,
                              Qt::QueuedConnection);
  }
}

void TargetRegistry::menusRebuilt()
{
  // Rebuilding the menu bar replaces the QMenu objects and may change the
  // actions' text, so the extensions need a fresh map.
  start();
}

void TargetRegistry::activeToolChanged()
{
  connectChangeSignals(); // the active view may have changed
  const QString name = activeToolName();
  // Called from both toolActivated() and setActiveTool(), so the same switch
  // can arrive twice; only report a real change.
  if (name.isEmpty() || name == m_lastTool)
    return;
  m_lastTool = name;
  notifyChange(QStringLiteral("tool"), name);
}

QString TargetRegistry::toolShortName(const QString& objectName)
{
  for (const ToolName& entry : toolNames) {
    if (objectName == QLatin1String(entry.identifier))
      return QString::fromLatin1(entry.shortName);
  }
  // Any other tool still gets a target, under its lower-cased object name.
  return objectName.toLower();
}

// ---------------------------------------------------------------------------
// Lookup

// A tool by plugin object name among those registered for the tool bar.
ToolPlugin* TargetRegistry::findTool(const QString& objectName) const
{
  for (const auto& tool : m_tools) {
    if (!tool.isNull() && tool->objectName() == objectName)
      return tool;
  }
  return nullptr;
}

// The short name of the active view's active tool, or empty if there is none.
QString TargetRegistry::activeToolName() const
{
  if (m_window == nullptr)
    return QString();
  auto* glWidget = m_window->activeGLWidget();
  const ToolPlugin* tool = glWidget ? glWidget->activeTool() : nullptr;
  return tool ? toolShortName(tool->objectName()) : QString();
}

// The QAction behind "action:<name>": the main window's own actions first,
// then extension actions matched by object name.
QAction* TargetRegistry::findAction(const QString& name) const
{
  for (const auto& entry : m_actions) {
    if (entry.first == name && !entry.second.isNull())
      return entry.second;
  }
  // The names that PR 2 gives to extension actions are aliases for the v1
  // names; any other extension action with an object name is reachable as
  // "action:<objectName>".
  QString objectName = name;
  if (name == QLatin1String("optimize"))
    objectName = QStringLiteral("optimizeGeometry");
  if (objectName.isEmpty())
    return nullptr;
  for (const auto& extension : m_extensionList) {
    if (extension.isNull())
      continue;
    const QList<QAction*> actions = extension->actions();
    for (QAction* action : actions) {
      if (action != nullptr && action->objectName() == objectName)
        return action;
    }
  }
  return nullptr;
}

// The Editor tool instance whose settings widget is (or would be) in the tool
// dock.
ToolPlugin* TargetRegistry::editorTool() const
{
  // Each view owns its own set of tools; the settings panel in the dock
  // belongs to the active view's Editor, not to the one behind the tool bar.
  if (auto* glWidget = m_window ? m_window->activeGLWidget() : nullptr) {
    const QList<ToolPlugin*> tools = glWidget->tools();
    for (ToolPlugin* tool : tools) {
      if (tool != nullptr && tool->objectName() == QLatin1String("Editor"))
        return tool;
    }
  }
  // No active view: fall back to the tool registered for the tool bar.
  return findTool(QStringLiteral("Editor"));
}

// One of the Editor's settings widgets by object name, searched only inside
// the Editor's own panel so a same-named widget elsewhere cannot match. The
// Editor builds its panel in its constructor, so this works before the tool has
// ever been activated.
QWidget* TargetRegistry::editorWidget(const QString& name) const
{
  ToolPlugin* editor = editorTool();
  QWidget* panel = editor ? editor->toolWidget() : nullptr;
  if (panel == nullptr)
    return nullptr;
  return panel->findChild<QWidget*>(name);
}

// Every name worth listing: the fixed ones, plus whatever tools and extension
// actions have registered. A name may be known here yet not resolved (for
// example action:optimize before the extension that provides it has loaded).
QStringList TargetRegistry::knownNames() const
{
  QStringList names{ QStringLiteral("view"), QStringLiteral("tool") };
  // The fixed tool names are always listed so a tutorial can tell "no such
  // tool" from "tool not loaded".
  for (const ToolName& entry : toolNames)
    names << QStringLiteral("tool:") + QLatin1String(entry.shortName);
  for (const auto& tool : m_tools) {
    if (tool.isNull())
      continue;
    const QString name =
      QStringLiteral("tool:") + toolShortName(tool->objectName());
    if (!names.contains(name))
      names << name;
  }
  // Always list the action names from the tutorial target table, resolved or
  // not.
  names << QStringLiteral("action:new") << QStringLiteral("action:open")
        << QStringLiteral("action:optimize");
  for (const auto& entry : m_actions) {
    const QString name = QStringLiteral("action:") + entry.first;
    if (!names.contains(name))
      names << name;
  }
  // The generic rule: any extension action with an object name is a target.
  for (const auto& extension : m_extensionList) {
    if (extension.isNull())
      continue;
    const QList<QAction*> actions = extension->actions();
    for (const QAction* action : actions) {
      if (action == nullptr || action->objectName().isEmpty())
        continue;
      const QString name = QStringLiteral("action:") + action->objectName();
      if (!names.contains(name))
        names << name;
    }
  }
  for (const char* editor : editorNames)
    names << QStringLiteral("editor:") + QLatin1String(editor);
  return names;
}

// Finds the menu bar that ultimately holds @p action, by walking up through
// the menus that contain it. If @p chain is given it receives those menus,
// outermost (the menu-bar title) first. Returns nullptr when the action is not
// reachable from a menu bar.
QMenuBar* TargetRegistry::menuBarFor(QAction* action,
                                     QList<QPointer<QMenu>>* chain) const
{
  if (action == nullptr)
    return nullptr;
  if (chain != nullptr)
    chain->clear();

  QList<QPointer<QMenu>> found; // innermost first
  // associatedObjects() lists everything the action has been added to. An
  // action in a menu lists that QMenu; a submenu's own menuAction() in turn
  // lists its parent QMenu, or the QMenuBar at the top.
  QAction* current = action;
  for (int depth = 0; depth < kMaxMenuDepth && current != nullptr; ++depth) {
    QMenu* menu = nullptr;
    QMenuBar* bar = nullptr;
    const QList<QObject*> owners = current->associatedObjects();
    for (QObject* owner : owners) {
      if (auto* candidate = qobject_cast<QMenu*>(owner)) {
        menu = candidate;
        break;
      }
      // Prefer a menu; remember a menu bar in case no menu lists the action
      // (the top-level menu's own action sits only on the bar).
      if (bar == nullptr)
        bar = qobject_cast<QMenuBar*>(owner);
    }
    if (menu != nullptr) {
      found.append(menu);
      current = menu->menuAction();
      continue;
    }
    // Reached the menu bar; hand the chain back outermost first.
    if (bar != nullptr && !found.isEmpty()) {
      if (chain != nullptr) {
        for (int i = found.size() - 1; i >= 0; --i)
          chain->append(found.at(i));
      }
      return bar;
    }
    break;
  }
  return nullptr;
}

// "File ▸ New" style path to a menu action. An action that is not in a menu
// bar gets just its own text.
QString TargetRegistry::menuPathText(QAction* action) const
{
  QList<QPointer<QMenu>> chain;
  QString path;
  if (menuBarFor(action, &chain) != nullptr) {
    for (const auto& menu : chain) {
      if (!menu.isNull())
        path += stripMnemonic(menu->title()) + arrowSeparator();
    }
  }
  return path + stripMnemonic(action->text());
}

// The tool bar's "»" button, shown when some buttons do not fit. It belongs to
// QToolBar's private layout, so it is found by its object name.
QToolButton* TargetRegistry::toolbarExtension() const
{
  if (m_toolBar.isNull())
    return nullptr;
  return m_toolBar->findChild<QToolButton*>(
    QStringLiteral("qt_toolbar_ext_button"), Qt::FindDirectChildrenOnly);
}

// The text a tutorial shows for a menu action: path plus shortcut.
QString TargetRegistry::actionText(QAction* action) const
{
  return withShortcut(menuPathText(action), action);
}

// The visible tool button for an action that is also on a tool bar, or nullptr
// (most menu actions are not).
QWidget* TargetRegistry::toolbarButton(QAction* action) const
{
  if (action == nullptr)
    return nullptr;
  const QList<QObject*> owners = action->associatedObjects();
  for (QObject* owner : owners) {
    if (auto* bar = qobject_cast<QToolBar*>(owner)) {
      QWidget* button = bar->widgetForAction(action);
      if (button != nullptr && button->isVisible())
        return button;
    }
  }
  return nullptr;
}

// Turns a target name into what it refers to right now. Never cached: the
// result depends on the current menus, dock and active view. Each early
// return leaves the later fields at their defaults, so callers can tell an
// unknown name (known = false) from one that is known but missing or hidden.
TargetRegistry::Resolved TargetRegistry::resolve(const QString& name) const
{
  Resolved r;

  if (name == QLatin1String("view")) {
    r.known = true;
    r.text = tr("3D view");
    QWidget* view = m_window ? m_window->activeGLWidget() : nullptr;
    r.widget = view;
    r.resolved = view != nullptr;
    r.visible = view != nullptr && view->isVisible();
    if (!r.visible)
      r.reason = tr("The 3D view is not showing.");
    return r;
  }

  // "tool" is state only: it reports the active tool as its value but there is
  // nothing to draw around.
  if (name == QLatin1String("tool")) {
    r.known = true;
    r.stateOnly = true;
    r.resolved = true;
    r.value = activeToolName();
    r.reason = tr("\"tool\" reports the active tool; it has nothing to "
                  "outline.");
    return r;
  }

  if (name.startsWith(QLatin1String("tool:"))) {
    // 5 == length of "tool:".
    const QString shortName = name.mid(5);
    // A built-in tool name stays "known" even if that plugin did not load, so
    // the caller gets "not available" rather than "unknown".
    bool fixed = false;
    for (const ToolName& entry : toolNames) {
      if (shortName == QLatin1String(entry.shortName))
        fixed = true;
    }
    ToolPlugin* tool = nullptr;
    for (const auto& candidate : m_tools) {
      if (!candidate.isNull() &&
          toolShortName(candidate->objectName()) == shortName)
        tool = candidate;
    }
    r.known = fixed || tool != nullptr;
    if (tool == nullptr)
      return r;
    // The tool bar button is the tool's activate action's widget.
    QAction* action = tool->activateAction();
    r.action = action;
    r.resolved = action != nullptr;
    if (action == nullptr)
      return r;
    r.text = withShortcut(stripMnemonic(action->text()), action);
    r.widget = m_toolBar ? m_toolBar->widgetForAction(action) : nullptr;
    r.visible = !r.widget.isNull() && r.widget->isVisible();
    // A narrow window hides the buttons that do not fit behind the tool bar's
    // extension button; the target can still be shown at that button.
    r.extension = toolbarExtension();
    if (!r.visible && !r.widget.isNull() && !r.extension.isNull())
      r.visible = m_toolBar->isVisible() && r.extension->isVisible();
    if (!r.visible)
      r.reason = tr("The tool bar button is not showing.");
    return r;
  }

  if (name.startsWith(QLatin1String("action:"))) {
    // 7 == length of "action:".
    const QString actionName = name.mid(7);
    QAction* action = findAction(actionName);
    r.known = action != nullptr || actionName == QLatin1String("new") ||
              actionName == QLatin1String("open") ||
              actionName == QLatin1String("optimize");
    if (action == nullptr)
      return r;
    r.action = action;
    r.resolved = true;
    r.visible = true; // always reachable from a menu
    r.text = actionText(action);
    // Only set when the action also has a visible tool bar button, which a
    // highlight prefers to outline over the menu.
    r.widget = toolbarButton(action);
    return r;
  }

  if (name.startsWith(QLatin1String("editor:"))) {
    // 7 == length of "editor:".
    const QString widgetName = name.mid(7);
    bool fixed = false;
    for (const char* editor : editorNames) {
      if (widgetName == QLatin1String(editor))
        fixed = true;
    }
    r.known = fixed;
    if (!fixed)
      return r;
    QWidget* widget = editorWidget(widgetName);
    ToolPlugin* editor = editorTool();
    if (widget == nullptr || editor == nullptr)
      return r;
    r.resolved = true;
    r.widget = widget;
    r.text = editorLabel(editor->toolWidget(), widget);
    r.value = editorValue(widgetName, widget);
    // The settings panel shows only the active tool's widget. The widget
    // itself is shown a moment after the dock takes it over (Qt queues that),
    // so it is not asked here; the overlay waits for it to appear.
    r.visible = !m_toolDock.isNull() && m_toolDock->isVisible() &&
                m_toolDock->widget() == editor->toolWidget();
    if (!r.visible)
      r.reason = tr("The Draw tool's settings are not showing. Choose the "
                    "Draw tool first.");
    return r;
  }

  return r;
}

// The read-back for the listUiTargets RPC. Unlike sendTargets() this includes
// unresolved names, with resolved and visible flags, so a caller can see why a
// target is not usable.
QVariantMap TargetRegistry::describeTargets() const
{
  QVariantMap targets;
  const QStringList names = knownNames();
  for (const QString& name : names) {
    const Resolved r = resolve(name);
    QVariantMap entry;
    entry[QStringLiteral("text")] = r.text;
    entry[QStringLiteral("value")] = r.value;
    entry[QStringLiteral("resolved")] = r.resolved;
    entry[QStringLiteral("visible")] = r.visible;
    targets[name] = entry;
  }
  return targets;
}

// ---------------------------------------------------------------------------
// Notifying the extensions

// Pushes the current text and value of every resolvable target to the
// extensions that have a setUiTargets slot. Runs from the event loop (queued
// by start()), after the menus have been built.
void TargetRegistry::sendTargets()
{
  m_pushPending = false;
  // Also hook up the change signals here: extensions and the Editor's widgets
  // may only exist by now.
  connectChangeSignals();

  QVariantMap targets;
  const QStringList names = knownNames();
  for (const QString& name : names) {
    const Resolved r = resolve(name);
    // Only resolved targets are sent, so a missing key means "not available
    // right now".
    if (!r.resolved)
      continue;
    QVariantMap entry;
    entry[QStringLiteral("text")] = r.text;
    entry[QStringLiteral("value")] = r.value;
    targets[name] = entry;
  }

  // By name, since the slot may not be declared in the avogadrolibs this app
  // was built against.
  for (const auto& listener : m_targetListeners) {
    if (!listener.isNull()) {
      QMetaObject::invokeMethod(listener.data(), "setUiTargets",
                                Q_ARG(QVariantMap, targets));
    }
  }
}

// Tells the extensions that have uiTargetChanged that a target changed. The
// value is invalid (a null QVariant) for an action that was triggered.
void TargetRegistry::notifyChange(const QString& target, const QVariant& value)
{
  for (const auto& listener : m_changeListeners) {
    if (!listener.isNull()) {
      QMetaObject::invokeMethod(listener.data(), "uiTargetChanged",
                                Q_ARG(QString, target), Q_ARG(QVariant, value));
    }
  }
}

void TargetRegistry::connectChangeSignals()
{
  // Real Qt signals, connected once per object (UniqueConnection); nothing
  // is polled. This is safe to call repeatedly, and is called whenever the
  // set of objects may have changed (a new view, a menu rebuild).
  for (const auto& entry : m_actions) {
    if (!entry.second.isNull()) {
      connect(entry.second.data(), &QAction::triggered, this,
              &TargetRegistry::onActionTriggered, Qt::UniqueConnection);
    }
  }
  for (const auto& extension : m_extensionList) {
    if (extension.isNull())
      continue;
    const QList<QAction*> actions = extension->actions();
    for (QAction* action : actions) {
      if (action != nullptr && !action->objectName().isEmpty()) {
        connect(action, &QAction::triggered, this,
                &TargetRegistry::onActionTriggered, Qt::UniqueConnection);
      }
    }
  }
  // The Editor's widgets belong to the active view's Editor, so they are
  // re-found each time rather than remembered.
  for (const char* editor : editorNames) {
    QWidget* widget = editorWidget(QLatin1String(editor));
    if (auto* combo = qobject_cast<QComboBox*>(widget)) {
      connect(combo, &QComboBox::currentIndexChanged, this,
              &TargetRegistry::onEditorValueChanged, Qt::UniqueConnection);
    } else if (auto* check = qobject_cast<QCheckBox*>(widget)) {
      connect(check, &QCheckBox::toggled, this,
              &TargetRegistry::onEditorValueChanged, Qt::UniqueConnection);
    }
  }
}

// Slot for every watched QAction; reports each target name that maps to the
// action that fired (an action can have an alias, such as optimize).
void TargetRegistry::onActionTriggered()
{
  auto* action = qobject_cast<QAction*>(sender());
  if (action == nullptr)
    return;
  const QStringList names = knownNames();
  for (const QString& name : names) {
    if (name.startsWith(QLatin1String("action:")) &&
        findAction(name.mid(7)) == action)
      notifyChange(name, QVariant());
  }
}

// Slot for the Editor's combo boxes and check boxes. The sender is matched
// against each named widget to work out which target changed.
void TargetRegistry::onEditorValueChanged()
{
  const QObject* changed = sender();
  if (changed == nullptr)
    return;
  for (const char* editor : editorNames) {
    const QString widgetName = QLatin1String(editor);
    QWidget* widget = editorWidget(widgetName);
    if (widget != changed)
      continue;
    const QVariant value = editorValue(widgetName, widget);
    // Invalid for "Other…", which is not a value worth reporting.
    if (value.isValid())
      notifyChange(QStringLiteral("editor:") + widgetName, value);
  }
}

// ---------------------------------------------------------------------------
// Highlight and reveal

// The overlay for a top-level window, created on first use. There is one per
// window so that a floating tool dock gets its own, parented to the window the
// target is in.
HighlightOverlay* TargetRegistry::overlayFor(QWidget* window)
{
  // Drop overlays that were deleted along with their window.
  m_overlays.removeAll(QPointer<HighlightOverlay>());
  for (const auto& overlay : m_overlays) {
    if (overlay->parentWidget() == window)
      return overlay;
  }
  auto* overlay = new HighlightOverlay(window);
  m_overlays.append(overlay);
  return overlay;
}

// Highlight never moves focus or opens anything, so a student can keep typing
// or drawing while the outline is up.
bool TargetRegistry::highlight(const QString& target, const QString& callout,
                               QString* error)
{
  return show(target, callout, false, error);
}

bool TargetRegistry::reveal(const QString& target, QString* error)
{
  return show(target, QString(), true, error);
}

// The shared body of highlight() and reveal(). The steps are: resolve and
// check the target, work out where to draw (a locator in global coordinates),
// show the overlay, then, for a reveal only, move focus or open the menu and
// announce. On failure returns false with a reason for the caller in *error.
bool TargetRegistry::show(const QString& target, const QString& callout,
                          bool reveal, QString* error)
{
  auto fail = [error](const QString& message) {
    if (error != nullptr)
      *error = message;
    return false;
  };
  // The target name comes from an RPC client; keep it short in messages.
  const QString shown = target.left(64);

  // A tool bar expanded by an earlier reveal goes back before the lookup,
  // unless this reveals a tool button too (it may need the overflow open).
  if (!(reveal && target.startsWith(QLatin1String("tool:"))))
    collapseToolBar();

  Resolved r = resolve(target);
  if (!r.known)
    return fail(tr("Unknown UI target \"%1\".").arg(shown));

  // "Show me" on the Draw tool's settings brings the settings panel back
  // (the tool dock may have been closed); then look the target up again.
  if (reveal && r.resolved && !r.visible && !r.stateOnly &&
      target.startsWith(QLatin1String("editor:")) && !m_toolDock.isNull() &&
      !m_toolDock->isVisible()) {
    m_toolDock->show();
    m_toolDock->raise();
    r = resolve(target);
  }

  // Report why it cannot be shown, most specific reason first.
  if (r.stateOnly)
    return fail(r.reason);
  if (!r.resolved)
    return fail(tr("UI target \"%1\" is not available.").arg(shown));
  if (!r.visible) {
    return fail(r.reason.isEmpty()
                  ? tr("UI target \"%1\" is not visible.").arg(shown)
                  : r.reason);
  }

  // A menu opened by an earlier reveal must not be left hanging.
  closeOpenedMenus();

  // A reveal of a tool button in the tool bar overflow opens the overflow
  // (unless it is open already, which a click would close). The tool bar
  // lays out and collapses its buttons later, so the outline below follows
  // whichever of the button and the "»" button is showing at each tick.
  if (reveal && !r.extension.isNull() && !r.widget.isNull() &&
      !r.widget->isVisible() && r.extension->isVisible() &&
      !r.extension->isChecked()) {
    r.extension->click();
    m_expandedToolBar = m_toolBar;
  }

  const bool isMenuAction =
    target.startsWith(QLatin1String("action:")) && !r.action.isNull();

  // A menu action is outlined at its menu title (non-native menu bar) or not
  // at all (native menu bar: callout only). A reveal always goes to the menu;
  // a highlight prefers the action's tool bar button when it has a visible
  // one.
  QMenuBar* bar = nullptr;
  QList<QPointer<QMenu>> chain;
  if (isMenuAction)
    bar = menuBarFor(r.action, &chain);
  const bool menuIsNative = bar == nullptr || bar->isNativeMenuBar();

  // The locator is called by the overlay on every tick, so the outline follows
  // the target if the window is moved or resized. It returns global
  // coordinates. The anchor decides which top-level window gets the overlay.
  HighlightOverlay::Locator locator;
  QWidget* anchor = m_window;
  if (isMenuAction && (reveal || r.widget.isNull())) {
    if (!menuIsNative && !chain.isEmpty() && !chain.first().isNull()) {
      // Outline the menu bar title. QPointers, as the menus can be rebuilt
      // (and the old ones deleted) while the outline is showing.
      QPointer<QMenuBar> barPointer(bar);
      QPointer<QAction> top(chain.first()->menuAction());
      anchor = bar;
      locator = [barPointer, top]() {
        if (barPointer.isNull() || top.isNull() || !barPointer->isVisible())
          return QRect();
        // actionGeometry() is in the menu bar's own coordinates.
        const QRect local = barPointer->actionGeometry(top);
        if (local.isEmpty())
          return QRect();
        return QRect(barPointer->mapToGlobal(local.topLeft()), local.size());
      };
    }
  } else if (!r.widget.isNull()) {
    // A plain widget target (tool button, Editor widget, 3D view).
    QPointer<QWidget> widget = r.widget;
    QPointer<QToolButton> extension = r.extension;
    QPointer<QToolBar> toolBar = m_toolBar;
    anchor = widget;
    locator = [widget, extension, toolBar]() {
      if (!widget.isNull() && widget->isVisible())
        return globalRectOf(widget);
      if (!extension.isNull() && !toolBar.isNull() && toolBar->isVisible())
        return globalRectOf(extension);
      return QRect();
    };
  }
  if (anchor == nullptr)
    return fail(tr("There is no window to draw on."));

  // With no locator (native macOS menu bar), the overlay shows its callout
  // alone at the top of the window.
  QWidget* window = anchor->window();
  // At most one outline at a time.
  for (const auto& other : m_overlays) {
    if (!other.isNull())
      other->clearTarget();
  }
  HighlightOverlay* overlay = overlayFor(window);
  // The callout always has text (so the target is not marked by colour alone):
  // the caller's, or else the target's own text. The reduce-motion setting is
  // re-read each time so a change in the settings applies at the next call.
  overlay->showTarget(locator, callout.isEmpty() ? r.text : callout, reveal,
                      reduceMotionEnabled());

  // Watch application-wide for Escape and window close, but only while an
  // outline is up.
  if (!m_filterInstalled) {
    qApp->installEventFilter(this);
    m_filterInstalled = true;
  }

  if (reveal) {
    if (isMenuAction) {
      // A native menu bar (macOS) cannot be opened or outlined from here, so
      // the callout and the announcement are all the student gets.
      if (!menuIsNative)
        openMenuChain(r.action);
    } else if (!r.widget.isNull()) {
      QWidget* focus = r.widget;
      if (!r.widget->isVisible() && !r.extension.isNull())
        focus = r.extension;
      focus->setFocus(Qt::OtherFocusReason);
    }
    // Announce on reveal only; the extension already announces its step on a
    // highlight, so doing it here too would read it twice.
    announce(r.text, r.widget.isNull() ? static_cast<QObject*>(m_window)
                                       : r.widget.data());
  }
  return true;
}

// Opens the menus that lead to a menu action, with the action selected, so a
// screen reader reads it and Enter runs it. It is a popup, not a modal dialog,
// so it does not block the RPC server. Does nothing on a native menu bar.
void TargetRegistry::openMenuChain(QAction* action)
{
  QList<QPointer<QMenu>> chain;
  QMenuBar* bar = menuBarFor(action, &chain);
  if (bar == nullptr || bar->isNativeMenuBar() || chain.isEmpty())
    return;
  m_openedMenuBar = bar;
  m_menuChain = chain;
  m_menuLeaf = action;
  stepMenuChain(0);
}

// Opens one more level of m_menuChain, then schedules the next. Level 0
// activates the menu-bar title, level n the n-th submenu, and the last level
// selects the action itself. The pointers are re-checked on every step since
// a menu rebuild can delete them during the delays.
void TargetRegistry::stepMenuChain(int level)
{
  if (m_openedMenuBar.isNull())
    return;
  const int count = static_cast<int>(m_menuChain.size());
  if (level < 0 || level > count)
    return;
  for (const auto& menu : m_menuChain) {
    if (menu.isNull())
      return;
  }

  // Each level is set a moment after the one above has popped up, since
  // nested menus open from a zero-delay timer. Setting every level at once
  // could select an item in a submenu that is not showing yet.
  if (level == 0) {
    m_openedMenuBar->setActiveAction(m_menuChain.first()->menuAction());
  } else if (level < count) {
    m_menuChain.at(level - 1)->setActiveAction(
      m_menuChain.at(level)->menuAction());
  } else if (!m_menuLeaf.isNull()) {
    m_menuChain.at(count - 1)->setActiveAction(m_menuLeaf);
  }

  if (level < count) {
    QTimer::singleShot(kMenuStepMs, this,
                       [this, level]() { stepMenuChain(level + 1); });
  }
}

// Closes the menus opened by a reveal, innermost first, and forgets them. Safe
// to call when none are open.
void TargetRegistry::closeOpenedMenus()
{
  if (m_openedMenuBar.isNull()) {
    m_menuChain.clear();
    return;
  }
  for (int i = static_cast<int>(m_menuChain.size()) - 1; i >= 0; --i) {
    if (!m_menuChain.at(i).isNull() && m_menuChain.at(i)->isVisible())
      m_menuChain.at(i)->hide();
  }
  m_openedMenuBar->setActiveAction(nullptr);
  m_openedMenuBar.clear();
  m_menuChain.clear();
  m_menuLeaf.clear();
}

// Closes the tool bar overflow if a reveal opened it. A tool bar the user
// expanded is left alone.
void TargetRegistry::collapseToolBar()
{
  if (m_expandedToolBar.isNull())
    return;
  QToolButton* extension = toolbarExtension();
  if (extension != nullptr && extension->isChecked())
    extension->click();
  m_expandedToolBar.clear();
}

// Removes the outline everywhere, and stops watching for Escape.
void TargetRegistry::clearHighlight()
{
  closeOpenedMenus();
  collapseToolBar();
  for (const auto& overlay : m_overlays) {
    if (!overlay.isNull())
      overlay->clearTarget();
  }
  if (m_filterInstalled) {
    qApp->removeEventFilter(this);
    m_filterInstalled = false;
  }
}

// Speaks the text through the platform's screen reader. The announcement
// event needs Qt 6.8 or later and an active accessibility client.
void TargetRegistry::announce(const QString& text, QObject* object)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
  if (!QAccessible::isActive() || object == nullptr || text.isEmpty())
    return;
  QAccessibleAnnouncementEvent event(object, text);
  QAccessible::updateAccessibility(&event);
#else
  // Qt before 6.8 has no announcements; the tutorial panel shows the same
  // text.
  Q_UNUSED(text)
  Q_UNUSED(object)
#endif
}

// Slot for an extension's requestHighlight signal (connected by name). An
// empty target means "clear". Failures are dropped: unlike the RPC verbs there
// is no caller to report them to.
void TargetRegistry::onRequestHighlight(const QString& target,
                                        const QString& callout)
{
  if (target.isEmpty()) {
    clearHighlight();
    return;
  }
  QString error;
  highlight(target, callout, &error);
}

// Slot for an extension's requestReveal signal (connected by name).
void TargetRegistry::onRequestReveal(const QString& target)
{
  QString error;
  reveal(target, &error);
}

// Installed on qApp only while an outline is showing (see show()).
bool TargetRegistry::eventFilter(QObject* watched, QEvent* event)
{
  // Escape clears the outline but is not consumed, so the tool or dialog that
  // has focus still sees it. Closing the outlined window clears it too.
  if (event->type() == QEvent::KeyPress) {
    if (static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape)
      clearHighlight();
  } else if (event->type() == QEvent::Close) {
    for (const auto& overlay : m_overlays) {
      if (!overlay.isNull() && watched == overlay->parentWidget()) {
        clearHighlight();
        break;
      }
    }
  }
  // Never consume anything: this filter only observes.
  return QObject::eventFilter(watched, event);
}

} // namespace Avogadro
