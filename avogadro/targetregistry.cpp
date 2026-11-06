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

#include <functional>

namespace Avogadro {

using QtGui::ExtensionPlugin;
using QtGui::ToolPlugin;

namespace {

constexpr int kMaxMenuDepth = 8;
constexpr int kMenuStepMs = 30;
// The Editor's element box uses this for "Other…"; it is a selector, not an
// element, so it is never reported.
constexpr int kOtherElement = 255;

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

const char* const editorNames[] = { "element", "bondOrder", "adjustHydrogens",
                                    "adjustBondLengths" };

QString stripMnemonic(const QString& text)
{
  QString result = text;
  result.replace(QLatin1String("&&"), QStringLiteral("\x01"));
  result.remove(QLatin1Char('&'));
  result.replace(QLatin1Char('\x01'), QLatin1Char('&'));
  return result;
}

QString arrowSeparator()
{
  return QStringLiteral(" %1 ").arg(QChar(0x25B8));
}

QString shortcutText(const QAction* action)
{
  if (action == nullptr || action->shortcut().isEmpty())
    return QString();
  return action->shortcut().toString(QKeySequence::NativeText);
}

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
    const int value = combo->currentData().toInt(&ok);
    if (!ok || (name == QLatin1String("element") && value == kOtherElement))
      return QVariant();
    return value;
  }
  if (const auto* check = qobject_cast<const QCheckBox*>(widget))
    return check->isChecked();
  return QVariant();
}

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
  // "Element:" reads better as "Element".
  while (text.endsWith(QLatin1Char(':')) || text.endsWith(QChar(0xFF1A)))
    text.chop(1);
  return text.trimmed();
}

QRect globalRectOf(const QPointer<QWidget>& widget)
{
  if (widget.isNull() || !widget->isVisible())
    return QRect();
  return QRect(widget->mapToGlobal(QPoint(0, 0)), widget->size());
}

} // namespace

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
  if (meta->indexOfMethod("setUiTargets(QVariantMap)") >= 0)
    m_targetListeners.append(extension);
  if (meta->indexOfMethod("uiTargetChanged(QString,QVariant)") >= 0)
    m_changeListeners.append(extension);
}

void TargetRegistry::start()
{
  m_lastTool = activeToolName();
  if (!m_pushPending) {
    m_pushPending = true;
    QMetaObject::invokeMethod(this, &TargetRegistry::sendTargets,
                              Qt::QueuedConnection);
  }
}

void TargetRegistry::menusRebuilt()
{
  start();
}

void TargetRegistry::activeToolChanged()
{
  connectChangeSignals(); // the active view may have changed
  const QString name = activeToolName();
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
  return objectName.toLower();
}

// ---------------------------------------------------------------------------
// Lookup

ToolPlugin* TargetRegistry::findTool(const QString& objectName) const
{
  for (const auto& tool : m_tools) {
    if (!tool.isNull() && tool->objectName() == objectName)
      return tool;
  }
  return nullptr;
}

QString TargetRegistry::activeToolName() const
{
  if (m_window == nullptr)
    return QString();
  auto* glWidget = m_window->activeGLWidget();
  const ToolPlugin* tool = glWidget ? glWidget->activeTool() : nullptr;
  return tool ? toolShortName(tool->objectName()) : QString();
}

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
  return findTool(QStringLiteral("Editor"));
}

QWidget* TargetRegistry::editorWidget(const QString& name) const
{
  ToolPlugin* editor = editorTool();
  QWidget* panel = editor ? editor->toolWidget() : nullptr;
  if (panel == nullptr)
    return nullptr;
  return panel->findChild<QWidget*>(name);
}

QStringList TargetRegistry::knownNames() const
{
  QStringList names{ QStringLiteral("view"), QStringLiteral("tool") };
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
  names << QStringLiteral("action:new") << QStringLiteral("action:open")
        << QStringLiteral("action:optimize");
  for (const auto& entry : m_actions) {
    const QString name = QStringLiteral("action:") + entry.first;
    if (!names.contains(name))
      names << name;
  }
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

QMenuBar* TargetRegistry::menuBarFor(QAction* action,
                                     QList<QPointer<QMenu>>* chain) const
{
  if (action == nullptr)
    return nullptr;
  if (chain != nullptr)
    chain->clear();

  QList<QPointer<QMenu>> found; // innermost first
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
      if (bar == nullptr)
        bar = qobject_cast<QMenuBar*>(owner);
    }
    if (menu != nullptr) {
      found.append(menu);
      current = menu->menuAction();
      continue;
    }
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

QString TargetRegistry::actionText(QAction* action) const
{
  return withShortcut(menuPathText(action), action);
}

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
    const QString shortName = name.mid(5);
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
    QAction* action = tool->activateAction();
    r.action = action;
    r.resolved = action != nullptr;
    if (action == nullptr)
      return r;
    r.text = withShortcut(stripMnemonic(action->text()), action);
    r.widget = m_toolBar ? m_toolBar->widgetForAction(action) : nullptr;
    r.visible = !r.widget.isNull() && r.widget->isVisible();
    if (!r.visible)
      r.reason = tr("The tool bar button is not showing.");
    return r;
  }

  if (name.startsWith(QLatin1String("action:"))) {
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
    r.widget = toolbarButton(action);
    return r;
  }

  if (name.startsWith(QLatin1String("editor:"))) {
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

void TargetRegistry::sendTargets()
{
  m_pushPending = false;
  connectChangeSignals();

  QVariantMap targets;
  const QStringList names = knownNames();
  for (const QString& name : names) {
    const Resolved r = resolve(name);
    if (!r.resolved)
      continue;
    QVariantMap entry;
    entry[QStringLiteral("text")] = r.text;
    entry[QStringLiteral("value")] = r.value;
    targets[name] = entry;
  }

  for (const auto& listener : m_targetListeners) {
    if (!listener.isNull()) {
      QMetaObject::invokeMethod(listener.data(), "setUiTargets",
                                Q_ARG(QVariantMap, targets));
    }
  }
}

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
  // is polled.
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
    if (value.isValid())
      notifyChange(QStringLiteral("editor:") + widgetName, value);
  }
}

// ---------------------------------------------------------------------------
// Highlight and reveal

HighlightOverlay* TargetRegistry::overlayFor(QWidget* window)
{
  m_overlays.removeAll(QPointer<HighlightOverlay>());
  for (const auto& overlay : m_overlays) {
    if (overlay->parentWidget() == window)
      return overlay;
  }
  auto* overlay = new HighlightOverlay(window);
  m_overlays.append(overlay);
  return overlay;
}

bool TargetRegistry::highlight(const QString& target, const QString& callout,
                               QString* error)
{
  return show(target, callout, false, error);
}

bool TargetRegistry::reveal(const QString& target, QString* error)
{
  return show(target, QString(), true, error);
}

bool TargetRegistry::show(const QString& target, const QString& callout,
                          bool reveal, QString* error)
{
  auto fail = [error](const QString& message) {
    if (error != nullptr)
      *error = message;
    return false;
  };
  const QString shown = target.left(64);

  Resolved r = resolve(target);
  if (!r.known)
    return fail(tr("Unknown UI target \"%1\".").arg(shown));

  // "Show me" on the Draw tool's settings brings the settings panel back.
  if (reveal && r.resolved && !r.visible && !r.stateOnly &&
      target.startsWith(QLatin1String("editor:")) && !m_toolDock.isNull() &&
      !m_toolDock->isVisible()) {
    m_toolDock->show();
    m_toolDock->raise();
    r = resolve(target);
  }

  if (r.stateOnly)
    return fail(r.reason);
  if (!r.resolved)
    return fail(tr("UI target \"%1\" is not available.").arg(shown));
  if (!r.visible) {
    return fail(r.reason.isEmpty()
                  ? tr("UI target \"%1\" is not visible.").arg(shown)
                  : r.reason);
  }

  closeOpenedMenus();

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

  HighlightOverlay::Locator locator;
  QWidget* anchor = m_window;
  if (isMenuAction && (reveal || r.widget.isNull())) {
    if (!menuIsNative && !chain.isEmpty() && !chain.first().isNull()) {
      QPointer<QMenuBar> barPointer(bar);
      QPointer<QAction> top(chain.first()->menuAction());
      anchor = bar;
      locator = [barPointer, top]() {
        if (barPointer.isNull() || top.isNull() || !barPointer->isVisible())
          return QRect();
        const QRect local = barPointer->actionGeometry(top);
        if (local.isEmpty())
          return QRect();
        return QRect(barPointer->mapToGlobal(local.topLeft()), local.size());
      };
    }
  } else if (!r.widget.isNull()) {
    QPointer<QWidget> widget = r.widget;
    anchor = widget;
    locator = [widget]() { return globalRectOf(widget); };
  }
  if (anchor == nullptr)
    return fail(tr("There is no window to draw on."));

  QWidget* window = anchor->window();
  // At most one outline at a time.
  for (const auto& other : m_overlays) {
    if (!other.isNull())
      other->clearTarget();
  }
  HighlightOverlay* overlay = overlayFor(window);
  overlay->showTarget(locator, callout.isEmpty() ? r.text : callout, reveal,
                      reduceMotionEnabled());

  if (!m_filterInstalled) {
    qApp->installEventFilter(this);
    m_filterInstalled = true;
  }

  if (reveal) {
    if (isMenuAction) {
      if (!menuIsNative)
        openMenuChain(r.action);
    } else if (!r.widget.isNull()) {
      r.widget->setFocus(Qt::OtherFocusReason);
    }
    announce(r.text, r.widget.isNull() ? static_cast<QObject*>(m_window)
                                       : r.widget.data());
  }
  return true;
}

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
  // nested menus open from a zero-delay timer.
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

void TargetRegistry::clearHighlight()
{
  closeOpenedMenus();
  for (const auto& overlay : m_overlays) {
    if (!overlay.isNull())
      overlay->clearTarget();
  }
  if (m_filterInstalled) {
    qApp->removeEventFilter(this);
    m_filterInstalled = false;
  }
}

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

void TargetRegistry::onRequestReveal(const QString& target)
{
  QString error;
  reveal(target, &error);
}

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
  return QObject::eventFilter(watched, event);
}

} // namespace Avogadro
