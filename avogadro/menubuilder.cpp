/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#include "menubuilder.h"

#include <QtCore/QDebug>
#include <QtCore/QOperatingSystemVersion>
#include <QtWidgets/QMenuBar>

#include <algorithm>
#include <utility>

namespace Avogadro {

namespace {
/**
 * @brief The PriorityText struct allows for comparison of text with different
 * priorities, where the strings should be sorted primarily on the priority and
 * secondarily on the string.
 */
struct PriorityText
{
  PriorityText(QString string, int pri)
    : text(std::move(string))
    , priority(pri)
  {
  }

  QString text;
  int priority;
};

bool lessThan(const PriorityText& left, const PriorityText& right)
{
  if (left.priority == right.priority) // Alphanumeric less than.
    return left.text < right.text;
  else
    return left.priority > right.priority;
}

/** Round @a x up to the next multiple of 100. */
int floor100(int x)
{
  return x >= 0 ? (x / 100) * 100 : ((x - 99) / 100) * 100;
}
}

MenuBuilder::MenuBuilder()
{
#ifdef Q_OS_MAC
  bool oldQtVersion = true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  oldQtVersion = false;
#endif
  auto currentOS = QOperatingSystemVersion::current();
  if (oldQtVersion || currentOS.majorVersion() < 26) {
    // If we're on Mac, check if we're on Tahoe or later.
    if (currentOS.majorVersion() < 26)
      m_showIcons = false;
  }
#endif
}

void MenuBuilder::addAction(const QStringList& pathList, QAction* action,
                            int priority)
{
  QString path(pathList.join("|"));
  if (m_menuActions.contains(path)) {
#ifdef Q_OS_MAC
    // If we're on Mac, don't show icons by default
    if (!m_showIcons) {
      action->setIcon(QIcon());
    }
#endif
    m_menuActions[path].append(action);
  } else {
    QList<QAction*> list;
    list << action;
    m_menuActions[path] = list;
    m_menuPaths[QString(path).replace("&", "")] = pathList;
  }
  if (priority == -1) {
    bool hasPriority;
    int newPriority = action->property("menu priority").toInt(&hasPriority);
    if (hasPriority)
      priority = newPriority;
  }
  m_priorities[action] = priority;
}

void MenuBuilder::buildMenuBar(QMenuBar* menuBar)
{
  // Get expected top level entries to the menu, inducing expected order on
  // them.
  QStringList orderedFirst, orderedEnd;
  orderedFirst << tr("&File") << tr("&Edit") << tr("&View") << tr("&Build");
  orderedFirst << tr("&Select"); // all in the mainwindow.ui

  // not in the UI
  orderedEnd << tr("Se&ttings") << tr("&Window") << tr("&Help");

  // grab the existing menus
  foreach (QMenu* menu, menuBar->findChildren<QMenu*>()) {
    QString title = menu->title();

    if (!orderedFirst.contains(title) && !orderedEnd.contains(title))
      continue; // not a standard top-level menu

    title.replace("&", ""); // remove the shortcut marks
    m_topLevelMenus[title] = menu;
  }

  // look through the paths we're given by the calling code
  QMap<QString, QString> topLevelStrings;
  foreach (QStringList list, m_menuPaths) {
    if (list.empty())
      continue;

    // Build a list of unique top level menu entries.
    // Make sure to check against the list of already-built menus
    QString topLevel = list[0];
    topLevel.replace("&", "");
    if (!topLevelStrings.contains(topLevel) &&
        !m_topLevelMenus.contains(topLevel))
      topLevelStrings[topLevel] = list[0];
  }

  foreach (QString text, orderedFirst) {
    QString plainText = text;
    plainText.replace("&", "");
    if (topLevelStrings.contains(plainText) &&
        !m_topLevelMenus.contains(plainText)) {
      m_topLevelMenus[plainText] = menuBar->addMenu(text);
      topLevelStrings.remove(plainText);
    }
  }
  QMapIterator<QString, QString> remaining(topLevelStrings);
  while (remaining.hasNext()) {
    remaining.next();
    if (orderedEnd.contains(remaining.value()))
      continue;
    m_topLevelMenus[remaining.key()] = menuBar->addMenu(remaining.value());
    topLevelStrings.remove(remaining.key());
  }
  foreach (QString text, orderedEnd) {
    QString plainText = text;
    plainText.replace("&", "");
    if (topLevelStrings.contains(plainText)) {
      m_topLevelMenus[plainText] = menuBar->addMenu(text);
      topLevelStrings.remove(plainText);
    }
  }

  // Now to iterate through the top level entries.
  foreach (const QString& path, m_topLevelMenus.keys())
    buildMenu(m_topLevelMenus[path], path);
}

void MenuBuilder::buildMenu(QMenu* menu, const QString& path)
{
  QList<QAction*> items;
  QList<PriorityText> actionText;
  QMap<QString, QAction*> actions;
  QList<QString> submenuPaths;
  QMap<QString, QString> submenuMap;
  // Find our concrete entries, and submenus.
  foreach (const QString& key, m_menuActions.keys()) {
    if (QString(key).replace("&", "") == path)
      items.append(m_menuActions[key]);
    else if (QString(key).replace("&", "").contains(path))
      submenuPaths.append(QString(key).replace("&", ""));
  }
  // Build up the list of entries that can be sorted.
  foreach (QAction* action, items) {
    actionText.append(PriorityText(action->text(), m_priorities[action]));
    actions[action->text()] = action;
  }
  foreach (const QString& subPath, submenuPaths) {
    int group(priorityGroup(subPath));
    int level(m_menuPaths[path].size());
    if (m_menuPaths[subPath].size() > level) {
      actionText.append(PriorityText(m_menuPaths[subPath][level], group));
      submenuMap[m_menuPaths[subPath][level]] = subPath;
      actions[subPath] = nullptr;
    }
  }

  std::sort(actionText.begin(), actionText.end(), lessThan);

  // When an action's priority is below this value, insert a separator.
  // Separators are inserted as needed at multiples of 100.
  int sepLimit =
    floor100(actionText.isEmpty() ? 0 : actionText.first().priority);
  foreach (const PriorityText& text, actionText) {
    if (text.priority < sepLimit) {
      menu->addSeparator();
      sepLimit = floor100(text.priority);
    }

    if (actions[text.text]) {
      // check to see if it's in the menu already
      bool replacedItem = false;
      foreach (QAction* action, menu->actions()) {
        if (action->text() == text.text) {
          // insert the new action and then remove the old one
          menu->insertAction(action, actions[text.text]);
          menu->removeAction(action);
          replacedItem = true;
          break;
        }
      }
      if (!replacedItem)
        menu->addAction(actions[text.text]);
    } else {
      // check if a sub-menu already exists
      bool replacedSubmenu = false;
      foreach (QAction* action, menu->actions()) {
        if (action->text() == text.text) {
          // build the new submenu, insert,
          // then remove the placeholder
          auto* subMenu = new QMenu(text.text, menu);
          buildMenu(subMenu, submenuMap[text.text]);
          menu->insertMenu(action, subMenu);
          menu->removeAction(action);
          replacedSubmenu = true;
          break;
        }
      }
      if (!replacedSubmenu)
        buildMenu(menu->addMenu(text.text), submenuMap[text.text]);
    }
  }
}

void MenuBuilder::print()
{
  qDebug() << "We have" << m_menuActions.values().size();
  QMapIterator<QString, QList<QAction*>> i(m_menuActions);
  while (i.hasNext()) {
    i.next();
    qDebug() << "Menu:" << i.key();
    foreach (QAction* action, i.value())
      qDebug() << "  action ->" << action->text() << "="
               << m_priorities[action];
  }
}

int MenuBuilder::priorityGroup(const QString& path)
{
  QList<QAction*> items;
  int result(-1);
  foreach (const QString& key, m_menuActions.keys())
    if (QString(key).replace("&", "").contains(path))
      items.append(m_menuActions[key]);
  foreach (QAction* action, items)
    if (m_priorities[action] > result)
      result = m_priorities[action];
  return result;
}

} // End namespace Avogadro.
