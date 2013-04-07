/******************************************************************************

  This source file is part of the Avogadro project.

  Copyright 2013 Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/

#include "menubuilder.h"

#include <QtCore/QDebug>
#include <QtGui/QMenuBar>

namespace Avogadro {

namespace {
/**
 * @brief The PriorityText struct allows for comparison of text with different
 * priorities, where the strings should be sorted primarily on the priority and
 * secondarily on the string.
 */
struct PriorityText {
  PriorityText(const QString &string, int pri) : text(string), priority(pri)
  {
  }

  QString text;
  int priority;
};

bool lessThan(const PriorityText &left, const PriorityText &right)
{
  if (left.priority == right.priority) // Alphanumeric less than.
    return left.text < right.text;
  else
    return left.priority > right.priority;
}
}

MenuBuilder::MenuBuilder()
{
}

void MenuBuilder::addAction(const QStringList &pathList, QAction *action,
                            int priority)
{
  QString path(pathList.join("|"));
  if (m_menuActions.contains(path)) {
    m_menuActions[path].append(action);
  }
  else {
    QList<QAction *> list;
    list << action;
    m_menuActions[path] = list;
    m_menuPaths[QString(path).replace("&", "")] = pathList;
  }
  m_priorities[action] = priority;
}

void MenuBuilder::buildMenu(QMenuBar *menu)
{
  menu->clear();

  QMap<QString, QString> topLevelStrings;
  foreach (QStringList list, m_menuPaths) {
    if (list.empty())
      continue;
    // Build a list of unique top level menu entries.
    QString topLevel = list[0];
    topLevel.replace("&", "");
    if (!topLevelStrings.contains(topLevel))
      topLevelStrings[topLevel] = list[0];
  }
  // Now add top level entries to the menu, inducing expected order on them.
  QStringList orderedFirst, orderedEnd;
  orderedFirst << tr("&File") << tr("&Edit") << tr("&View") << tr("&Tools");
  orderedEnd << tr("&Window") << tr("&Settings") << tr("&Help");
  foreach (QString text, orderedFirst) {
    QString plainText = text;
    plainText.replace("&", "");
    if (topLevelStrings.contains(plainText)) {
      m_topLevelMenus[plainText] = menu->addMenu(text);
      topLevelStrings.remove(plainText);
    }
  }
  QMapIterator<QString, QString> remaining(topLevelStrings);
  while (remaining.hasNext()) {
    remaining.next();
    if (orderedEnd.contains(remaining.value()))
      continue;
    m_topLevelMenus[remaining.key()] = menu->addMenu(remaining.value());
    topLevelStrings.remove(remaining.key());
  }
  foreach (QString text, orderedEnd) {
    QString plainText = text;
    plainText.replace("&", "");
    if (topLevelStrings.contains(plainText)) {
      m_topLevelMenus[plainText] = menu->addMenu(text);
      topLevelStrings.remove(plainText);
    }
  }

  // Now to iterate through the top level entries.
  foreach (const QString &path, m_topLevelMenus.keys())
    buildMenu(m_topLevelMenus[path], path);
}

void MenuBuilder::buildMenu(QMenu *menu, const QString &path)
{
  QList<QAction *> items;
  QList<PriorityText> actionText;
  QMap<QString, QAction *> actions;
  QList<QString> submenuPaths;
  QMap<QString, QString> submenuMap;
  // Find our concreate entries, and submenus.
  foreach (const QString &key, m_menuActions.keys()) {
    if (QString(key).replace("&", "") == path)
      items.append(m_menuActions[key]);
    else if (QString(key).replace("&", "").contains(path))
      submenuPaths.append(QString(key).replace("&", ""));
  }
  // Build up the list of entries that can be sorted.
  foreach (QAction *action, items) {
    actionText.append(PriorityText(action->text(), m_priorities[action]));
    actions[action->text()] = action;
  }
  foreach (const QString &subPath, submenuPaths) {
    int group(priorityGroup(subPath));
    int level(m_menuPaths[path].size());
    if (m_menuPaths[subPath].size() > level) {
      actionText.append(PriorityText(m_menuPaths[subPath][level], group));
      submenuMap[m_menuPaths[subPath][level]] = subPath;
      actions[subPath] = NULL;
    }
  }

  qSort(actionText.begin(), actionText.end(), lessThan);
  foreach (const PriorityText &text, actionText) {
    if (actions[text.text]) {
      menu->addAction(actions[text.text]);
    }
    else {
      buildMenu(menu->addMenu(text.text), submenuMap[text.text]);
    }
  }
}

void MenuBuilder::print()
{
  qDebug() << "We have" << m_menuActions.values().size();
  QMapIterator<QString, QList<QAction *> > i(m_menuActions);
  while (i.hasNext()) {
    i.next();
    qDebug() << "Menu:" << i.key();
    foreach (QAction * action, i.value())
      qDebug() << "  action ->" << action->text() << "=" << m_priorities[action];
  }
}

int MenuBuilder::priorityGroup(const QString &path)
{
  QList<QAction *> items;
  int result(-1);
  foreach (const QString &key, m_menuActions.keys())
    if (QString(key).replace("&", "").contains(path))
      items.append(m_menuActions[key]);
  foreach (QAction *action, items)
    if (m_priorities[action] > result)
      result = m_priorities[action];
  return result;
}

} // End namespace Avogadro.
