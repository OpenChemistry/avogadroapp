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

#include <avogadro/core/molecule.h>
#include <avogadro/core/elements.h>
#include <avogadro/io/cmlformat.h>
#include <avogadro/qtopengl/glwidget.h>
#include <avogadro/rendering/glrenderer.h>
#include <avogadro/rendering/scene.h>

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

namespace Avogadro {

MainWindow::MainWindow(const QString &fileName) : m_molecule(0)
{
  m_glWidget = new QtOpenGL::GLWidget(this);
  setCentralWidget(m_glWidget);
  setWindowTitle(tr("Avogadro"));

  // Create the menus.
  QMenu *file = menuBar()->addMenu(tr("&File"));
  QAction *open = file->addAction(tr("&Open"));
  connect(open, SIGNAL(triggered()), SLOT(openFile()));
  QAction *quit = file->addAction(tr("&Quit"));
  connect(quit, SIGNAL(triggered()), SLOT(close()));

  // Create the toolbars
  QToolBar *fileToolBar = addToolBar(tr("File"));
  fileToolBar->addAction(open);

  readSettings();

  openFile(fileName);
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

void MainWindow::setMolecule(Core::Molecule *mol)
{
  if (m_molecule)
    delete m_molecule;
  m_molecule = mol;
  Rendering::Scene &scene = m_glWidget->renderer().scene();
  scene.clear();

  for (size_t i = 0; i < mol->atomCount(); ++i) {
    Core::Atom atom = mol->atom(i);
    unsigned char atomicNumber = atom.atomicNumber();
    const unsigned char *c = Core::Elements::color(atomicNumber);
    Vector3ub color(c[0], c[1], c[2]);
    scene.addSphere(atom.position3d().cast<float>(), color,
                    static_cast<float>(0.2f * Core::Elements::radiusVDW(atomicNumber)));
  }
  m_glWidget->resetCamera();
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
  cml.readFile(fileName.toStdString());
  if (cml.molecule()) {
    m_recentFiles.prepend(fileName);
    updateRecentFiles();
    setMolecule(cml.molecule());
    statusBar()->showMessage(tr("Molecule loaded (%1 atoms, %2 bonds)")
                             .arg(cml.molecule()->atomCount())
                             .arg(cml.molecule()->bondCount()), 2500);
  }
  else {
    statusBar()->showMessage(tr("Failed to read %1").arg(fileName), 2500);
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
    QMenu *fileMenu(NULL);
    foreach (QAction *menu, menuBar()->actions())
      if (menu->text() == tr("&File"))
        fileMenu = menu->menu();
    if (!fileMenu)
      return;
    // We want to go after Open in the file menu.
    bool next(false);
    QAction *before(NULL);
    foreach (QAction *menu, fileMenu->actions()) {
      if (menu->text() == "&Open")
        next = true;
      else if (next)
        before = menu;
    }
    QMenu *recentMenu = new QMenu(tr("&Recent files"));
    fileMenu->insertMenu(before, recentMenu);
    for (int i = 0; i < 10; ++i) {
      m_actionRecentFiles.push_back(recentMenu->addAction(""));
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

} // End of Avogadro namespace
