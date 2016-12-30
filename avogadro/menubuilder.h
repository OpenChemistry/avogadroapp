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

#ifndef AVOGADRO_MENUBUILDER_H
#define AVOGADRO_MENUBUILDER_H

#include <QtCore/QObject>
#include <QtCore/QMap>

class QAction;
class QMenu;
class QMenuBar;

/**
 * @brief The MenuBuilder class helps to dynamically build up the application
 * menus.
 *
 * This class allows you to build up a list of menu entries, order them by
 * priority/section and then add them to a QMenu once all elements have been
 * added.
 *
 * Separators are inserted as needed when priorities cross multiples of 100
 * (e.g. The ranges ..., (-100)-(-1), 0-99, 100-199, ... will be grouped
 * together).
 */

namespace Avogadro {

class MenuBuilder : public QObject
{
  Q_OBJECT

public:
  MenuBuilder();

  /**
   * @brief Add a new action to the menu builder object.
   * @param path The menu path, where each element specifies a menu level.
   * @param action The action that will be added at the path.
   * @param priority The priority of the entry, higher will be at the top.
   */
  void addAction(const QStringList &path, QAction *action, int priority = -1);

  /**
   * @brief Populate the supplied menu bar with the items added to builder. Ordering
   * is attempted, ensuring File is first, Help is last and ordering by priority
   * and then alphanumerically.
   * @param menu The menu to be populated.
   */
  void buildMenuBar(QMenuBar *menuBar);

  /**
   * @brief Populate a menu with the appropriate sub-entries.
   */
  void buildMenu(QMenu *menu, const QString &path);

  /**
   * @brief Print the contents of the MenuBuilder, intended for debug.
   */
  void print();

private:
  /** A map of string to action lists. */
  QMap<QString, QList<QAction *> > m_menuActions;
  /** Mapping QString from m_menuActions to QStringLists. */
  QMap<QString, QStringList> m_menuPaths;
  /** Store entry priority orders mapped to the QActions. */
  QMap<QAction *, int> m_priorities;
  /** Top level menus mapped to text. */
  QMap<QString, QMenu *> m_topLevelMenus;

  /** Get the priority of a submenu (takes the highest priority). */
  int priorityGroup(const QString &path);
};

} // End namespace Avogadro.

#endif // AVOGADRO_MENUBUILDER_H
