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

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <avogadro/core/molecule.h>
#include <avogadro/core/elements.h>
#include <avogadro/io/cmlformat.h>
#include <avogadro/qtopengl/editor.h>
#include <avogadro/qtopengl/glwidget.h>
#include <avogadro/qtgui/pluginmanager.h>
#include <avogadro/qtgui/sceneplugin.h>
#include <avogadro/qtgui/scenepluginmodel.h>
#include <avogadro/qtgui/extensionplugin.h>
#include <avogadro/rendering/glrenderer.h>
#include <avogadro/rendering/scene.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>
#include <QtCore/QDebug>
#include <QtCore/QFileInfo>
#include <QtCore/QSettings>
#include <QtGui/QCloseEvent>
#include <QtGui/QMenu>
#include <QtGui/QMenuBar>
#include <QtGui/QFileDialog>
#include <QtGui/QStatusBar>
#include <QtGui/QToolBar>

#include <QtGui/QDockWidget>
#include <QtGui/QTreeView>
#include <QtGui/QHeaderView>

namespace Avogadro {

MainWindow::MainWindow(const QString &fileName)
  : m_ui(new Ui::MainWindow),
    m_molecule(0),
    m_scenePluginModel(0)
{
  m_ui->setupUi(this);

  // Create the scene plugin model
  m_scenePluginModel = new QtGui::ScenePluginModel(m_ui->scenePluginTreeView);
  m_ui->scenePluginTreeView->setModel(m_scenePluginModel);
  connect(m_scenePluginModel,
          SIGNAL(pluginStateChanged(Avogadro::QtGui::ScenePlugin*)),
          SLOT(updateScenePlugins()));
  /// @todo HACK the molecule should trigger the update
  connect(&m_ui->glWidget->editor(), SIGNAL(moleculeChanged()),
          SLOT(updateScenePlugins()));
  connect(&m_ui->glWidget->manipulator(), SIGNAL(moleculeChanged()),
          SLOT(updateScenePlugins()));

  // Connect the menu actions
  connect(m_ui->actionNewMolecule, SIGNAL(triggered()), SLOT(newMolecule()));
  connect(m_ui->actionOpen, SIGNAL(triggered()), SLOT(openFile()));
  connect(m_ui->actionQuit, SIGNAL(triggered()), qApp, SLOT(quit()));

  // Connect the temporary tool/element selectors
  updateTool();
  buildElements();
  updateElement();
  connect(m_ui->toolComboBox, SIGNAL(currentIndexChanged(int)),
          SLOT(updateTool()));
  connect(m_ui->elementComboBox, SIGNAL(currentIndexChanged(int)),
          SLOT(updateElement()));

  readSettings();

  QtGui::PluginManager *plugin = QtGui::PluginManager::instance();
  plugin->load();
  QList<QtGui::ScenePluginFactory *> scenePluginFactories =
      plugin->pluginFactories<QtGui::ScenePluginFactory>();
  foreach (QtGui::ScenePluginFactory *factory, scenePluginFactories) {
    QtGui::ScenePlugin *scenePlugin = factory->createInstance();
    if (scenePlugin) {
      scenePlugin->setParent(this);
      m_scenePluginModel->addItem(scenePlugin);
    }
  }

  qDebug() << "Calling load plugins again! This is to debug plugin loading...";
  plugin->load();

  QList<QtGui::ExtensionPluginFactory *> extensions =
      plugin->pluginFactories<QtGui::ExtensionPluginFactory>();
  qDebug() << "Extension plugins dynamically found..." << extensions.size();
  foreach (QtGui::ExtensionPluginFactory *factory, extensions) {
    QtGui::ExtensionPlugin *extension = factory->createInstance();
    if (extension) {
      extension->setParent(this);
      qDebug() << "extension:" << extension->name() << extension->menuPath();
      connect(this, SIGNAL(moleculeChanged(Core::Molecule*)),
              extension, SLOT(setMolecule(Core::Molecule*)));
      buildMenu(extension);
    }
  }

  // try to open the file passed in. If opening fails, create a new molecule.
  openFile(fileName);
  if (!m_molecule)
    newMolecule();
  statusBar()->showMessage(tr("Ready..."), 2000);
}

MainWindow::~MainWindow()
{
  writeSettings();
  delete m_molecule;
}

void MainWindow::closeEvent(QCloseEvent *e)
{
  writeSettings();
  QMainWindow::closeEvent(e);
}

void MainWindow::newMolecule()
{
  setMolecule(new Core::Molecule);
}

void MainWindow::setMolecule(Core::Molecule *mol)
{
  if (m_molecule == mol)
    return;

  // Clear the scene to prevent dangling identifiers:
  m_ui->glWidget->renderer().scene().clear();

  // Set molecule
  if (m_molecule)
    delete m_molecule;
  m_molecule = mol;

  emit moleculeChanged(m_molecule);

  m_ui->glWidget->editor().setMolecule(mol);
  m_ui->glWidget->manipulator().setMolecule(mol);
  updateScenePlugins();
  m_ui->glWidget->resetCamera();
}

void MainWindow::writeSettings()
{
  QSettings settings;
  settings.beginGroup("MainWindow");
  settings.setValue("size", size());
  settings.setValue("pos", pos());
  settings.endGroup();
  settings.setValue("recentFiles", m_recentFiles);
}

void MainWindow::readSettings()
{
  QSettings settings;
  settings.beginGroup("MainWindow");
  resize(settings.value("size", QSize(400, 300)).toSize());
  move(settings.value("pos", QPoint(20, 20)).toPoint());
  settings.endGroup();
  m_recentFiles = settings.value("recentFiles", QStringList()).toStringList();
  updateRecentFiles();
}

void MainWindow::openFile()
{
  QString fileName = QFileDialog::getOpenFileName(this,
                                                  tr("Open CML file"),
                                                  "",
                                                  tr("Chemical Markup Language (*.cml)"));
  openFile(fileName);
}

void MainWindow::openFile(const QString &fileName)
{
  if (fileName.isEmpty())
    return;

  Io::CmlFormat cml;
  Core::Molecule *molecule_ = new Core::Molecule;
  bool success = cml.readFile(fileName.toStdString(), *molecule_);
  if (success) {
    m_recentFiles.prepend(fileName);
    updateRecentFiles();
    setMolecule(molecule_);
    statusBar()->showMessage(tr("Molecule loaded (%1 atoms, %2 bonds)")
                             .arg(molecule_->atomCount())
                             .arg(molecule_->bondCount()), 2500);
  }
  else {
    statusBar()->showMessage(tr("Failed to read %1").arg(fileName), 2500);
    delete molecule_;
  }
}

void MainWindow::openRecentFile()
{
  QAction *action = qobject_cast<QAction *>(sender());
  if (action)
    openFile(action->data().toString());
}

void MainWindow::updateRecentFiles()
{
  m_recentFiles.removeDuplicates();
  while (m_recentFiles.size() > 10)
    m_recentFiles.removeLast();

  // Populate the recent file actions list if necessary.
  if (m_actionRecentFiles.isEmpty()) {
    for (int i = 0; i < 10; ++i) {
      m_actionRecentFiles.push_back(m_ui->menuRecentFiles->addAction(""));
      m_actionRecentFiles.back()->setVisible(false);
      connect(m_actionRecentFiles.back(), SIGNAL(triggered()),
              SLOT(openRecentFile()));
    }
    m_actionRecentFiles[0]->setText(tr("No recent files"));
    m_actionRecentFiles[0]->setVisible(true);
    m_actionRecentFiles[0]->setEnabled(false);
  }
  int i = 0;
  foreach (const QString &file, m_recentFiles) {
    QFileInfo fileInfo(file);
    QAction *recentFile = m_actionRecentFiles[i++];
    recentFile->setText(fileInfo.fileName());
    recentFile->setData(file);
    recentFile->setVisible(true);
    recentFile->setEnabled(true);
  }
  for (; i < 10; ++i)
    m_actionRecentFiles[i]->setVisible(false);
}

void MainWindow::updateScenePlugins()
{
  Rendering::Scene &scene = m_ui->glWidget->renderer().scene();
  scene.clear();
  if (m_molecule) {
    foreach (QtGui::ScenePlugin *scenePlugin,
             m_scenePluginModel->activeScenePlugins()) {
      scenePlugin->process(*m_molecule, scene);
    }
  }
  m_ui->glWidget->update();
}

void MainWindow::updateTool()
{
  m_ui->glWidget->setActiveTool(static_cast<QtOpenGL::GLWidget::Tool>(
                                  m_ui->toolComboBox->currentIndex()));
  m_ui->elementComboBox->setEnabled(
        m_ui->glWidget->activeTool() == QtOpenGL::GLWidget::EditTool);
}

void MainWindow::updateElement()
{
  m_ui->glWidget->editor().setAtomicNumber(
        m_elementLookup.at(m_ui->elementComboBox->currentIndex()));
}

void MainWindow::buildMenu(QtGui::ExtensionPlugin *extension)
{
  foreach (QAction *action, extension->actions()) {
    QStringList path = extension->menuPath(action);
    qDebug() << "Menu:" << extension->name() << path;
    if (path.size() < 1)
      continue;
    // First ensure the top-level menu is present (create it if needed).
    QMenu *menu(NULL);
    foreach (QAction *topMenu, menuBar()->actions()) {
      if (topMenu->text() == path.at(0)) {
        menu = topMenu->menu();
        break;
      }
    }
    if (!menu)
      menu = menuBar()->addMenu(path.at(0));

    // Build up submenus if necessary.
    QMenu *nextMenu(NULL);
    for (int i = 1; i < path.size(); ++i) {
      if (nextMenu) {
        menu = nextMenu;
        nextMenu = NULL;
      }
      const QString menuText = path[i];
      foreach (QAction *menuAction, menu->actions()) {
        if (menuAction->text() == menuText) {
          nextMenu = menuAction->menu();
          break;
        }
      }
      if (!nextMenu)
        nextMenu = menu->addMenu(path.at(i));
      menu = nextMenu;
      nextMenu = NULL;
    }
    // Now we actually add the action we got (it should have set the text etc).
    menu->addAction(action);
  }

}

void MainWindow::buildElements()
{
  m_ui->elementComboBox->clear();
  m_elementLookup.clear();

  // Add common elements to the top.
  addElement(1); // Hydrogen
  addElement(5); // Boron
  addElement(6); // Carbon
  addElement(7); // Nitrogen
  addElement(8); // Oxygen
  addElement(9); // Fluorine
  addElement(15); // Phosphorus
  addElement(16); // Sulfur
  addElement(17); // Chlorine
  addElement(35); // Bromine

  m_ui->elementComboBox->insertSeparator(m_ui->elementComboBox->count());

  // And the rest...
  for (unsigned char i = 1; i < Core::Elements::elementCount(); ++i)
    addElement(i);
}

void MainWindow::addElement(unsigned char atomicNum)
{
  m_ui->elementComboBox->addItem(QString("%1 (%2)")
                                 .arg(Core::Elements::name(atomicNum))
                                 .arg(QString::number(atomicNum)));
  m_elementLookup.push_back(atomicNum);
}

} // End of Avogadro namespace
