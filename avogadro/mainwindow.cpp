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

#include <avogadro/qtgui/molecule.h>
#include <avogadro/core/elements.h>
#include <avogadro/io/cmlformat.h>
#include <avogadro/io/cjsonformat.h>
#include <avogadro/qtopengl/editor.h>
#include <avogadro/qtopengl/glwidget.h>
#include <avogadro/qtplugins/pluginmanager.h>
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
#include <QtGui/QPushButton>

#ifdef QTTESTING
# include <pqTestUtility.h>
# include <pqEventObserver.h>
# include <pqEventSource.h>
# include <QXmlStreamReader>
#endif

namespace Avogadro {

#ifdef QTTESTING
class XMLEventObserver : public pqEventObserver
{
  QXmlStreamWriter* XMLStream;
  QString XMLString;

public:
  XMLEventObserver(QObject* p) : pqEventObserver(p)
  {
    this->XMLStream = NULL;
  }
  ~XMLEventObserver()
  {
    delete this->XMLStream;
  }

protected:
  virtual void setStream(QTextStream* stream)
  {
    if (this->XMLStream) {
      this->XMLStream->writeEndElement();
      this->XMLStream->writeEndDocument();
      delete this->XMLStream;
      this->XMLStream = NULL;
    }
    if (this->Stream)
      *this->Stream << this->XMLString;

    this->XMLString = QString();
    pqEventObserver::setStream(stream);
    if (this->Stream) {
      this->XMLStream = new QXmlStreamWriter(&this->XMLString);
      this->XMLStream->setAutoFormatting(true);
      this->XMLStream->writeStartDocument();
      this->XMLStream->writeStartElement("events");
    }
  }

  virtual void onRecordEvent(const QString& widget, const QString& command,
                             const QString& arguments)
  {
    if(this->XMLStream) {
      this->XMLStream->writeStartElement("event");
      this->XMLStream->writeAttribute("widget", widget);
      this->XMLStream->writeAttribute("command", command);
      this->XMLStream->writeAttribute("arguments", arguments);
      this->XMLStream->writeEndElement();
    }
  }
};

class XMLEventSource : public pqEventSource
{
  typedef pqEventSource Superclass;
  QXmlStreamReader *XMLStream;

public:
  XMLEventSource(QObject* p): Superclass(p) { this->XMLStream = NULL;}
  ~XMLEventSource() { delete this->XMLStream; }

protected:
  virtual void setContent(const QString& xmlfilename)
  {
    delete this->XMLStream;
    this->XMLStream = NULL;

    QFile xml(xmlfilename);
    if (!xml.open(QIODevice::ReadOnly)) {
      qDebug() << "Failed to load " << xmlfilename;
      return;
    }
    QByteArray data = xml.readAll();
    this->XMLStream = new QXmlStreamReader(data);
  }

  int getNextEvent(QString& widget, QString& command, QString& arguments)
  {
    if (this->XMLStream->atEnd())
      return DONE;
    while (!this->XMLStream->atEnd()) {
      QXmlStreamReader::TokenType token = this->XMLStream->readNext();
      if (token == QXmlStreamReader::StartElement) {
        if (this->XMLStream->name() == "event")
          break;
      }
    }
    if (this->XMLStream->atEnd())
      return DONE;

    widget = this->XMLStream->attributes().value("widget").toString();
    command = this->XMLStream->attributes().value("command").toString();
    arguments = this->XMLStream->attributes().value("arguments").toString();
    return SUCCESS;
  }
};
#endif

using QtGui::Molecule;

MainWindow::MainWindow(const QString &fileName, bool disableSettings)
  : m_ui(new Ui::MainWindow),
    m_molecule(0),
    m_scenePluginModel(0)
{
  m_ui->setupUi(this);

  QIcon icon(":/icons/avogadro.png");
  setWindowIcon(icon);

  // Create the scene plugin model
  m_scenePluginModel = new QtGui::ScenePluginModel(m_ui->scenePluginTreeView);
  m_ui->scenePluginTreeView->setModel(m_scenePluginModel);
  m_ui->scenePluginTreeView->setAlternatingRowColors(true);
  m_ui->scenePluginTreeView->header()->stretchLastSection();
  m_ui->scenePluginTreeView->header()->setVisible(false);
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

  // If disable settings, ensure we create a cleared QSettings object.
  if (disableSettings) {
    QSettings settings;
    settings.clear();
    settings.sync();
  }
  // The default settings will be used if everything was cleared.
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

  // Call this a second time, not needed but ensures plugins only load once.
  plugin->load();

  QList<QtGui::ExtensionPluginFactory *> extensions =
      plugin->pluginFactories<QtGui::ExtensionPluginFactory>();
  qDebug() << "Extension plugins dynamically found..." << extensions.size();
  foreach (QtGui::ExtensionPluginFactory *factory, extensions) {
    QtGui::ExtensionPlugin *extension = factory->createInstance();
    if (extension) {
      extension->setParent(this);
      qDebug() << "extension:" << extension->name() << extension->menuPath();
      connect(this, SIGNAL(moleculeChanged(QtGui::Molecule*)),
              extension, SLOT(setMolecule(QtGui::Molecule*)));
      connect(extension, SIGNAL(moleculeReady(int)), SLOT(moleculeReady(int)));
      buildMenu(extension);
    }
  }

  // try to open the file passed in. If opening fails, create a new molecule.
  openFile(fileName);
  if (!m_molecule)
    newMolecule();
  statusBar()->showMessage(tr("Ready..."), 2000);

#ifdef QTTESTING
  QMenu *menu = menuBar()->addMenu(tr("&Testing"));
  QAction *actionRecord = new QAction(this);
  actionRecord->setText(tr("Record test..."));
  menu->addAction(actionRecord);
  QAction *actionPlay = new QAction(this);
  actionPlay->setText(tr("Play test..."));
  menu->addAction(actionPlay);

  connect(actionRecord, SIGNAL(triggered()), SLOT(record()));
  connect(actionPlay, SIGNAL(triggered()), SLOT(play()));

  m_testUtility = new pqTestUtility(this);
  m_testUtility->addEventObserver("xml", new XMLEventObserver(this));
  m_testUtility->addEventSource("xml", new XMLEventSource(this));

  m_testExit = true;
#endif
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

void MainWindow::moleculeReady(int)
{
  QtGui::ExtensionPlugin *extension =
      qobject_cast<QtGui::ExtensionPlugin *>(sender());
  if (extension) {
    QtGui::Molecule *mol = new QtGui::Molecule(this);
    if (extension->readMolecule(*mol))
      setMolecule(mol);
  }
}

void MainWindow::newMolecule()
{
  setMolecule(new QtGui::Molecule);
}

void MainWindow::setMolecule(QtGui::Molecule *mol)
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

  connect(m_molecule, SIGNAL(changed(unsigned int)),
          SLOT(updateScenePlugins()));

  m_ui->glWidget->editor().setMolecule(mol);
  m_ui->glWidget->manipulator().setMolecule(mol);
  updateScenePlugins();
  m_ui->glWidget->resetCamera();
}

#ifdef QTTESTING
void MainWindow::playTest(const QString &fileName, bool exit)
{
  m_testFile = fileName;
  m_testExit = exit;

  QTimer::singleShot(200, this, SLOT(playTest()));
}
#endif

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
  resize(settings.value("size", QSize(800, 600)).toSize());
  move(settings.value("pos", QPoint(20, 20)).toPoint());
  settings.endGroup();
  m_recentFiles = settings.value("recentFiles", QStringList()).toStringList();
  updateRecentFiles();
}

void MainWindow::openFile()
{
  QString fileName = QFileDialog::getOpenFileName(this, tr("Open chemical file"),
    "", tr("Chemical files (*.cml *.cjson)"));
  openFile(fileName);
}

void MainWindow::openFile(const QString &fileName)
{
  if (fileName.isEmpty())
    return;

  QFileInfo info(fileName);
  Molecule *molecule_(NULL);
  bool success(false);
  if (info.suffix() == "cml") {
    Io::CmlFormat cml;
    molecule_ = new Molecule;
    success = cml.readFile(fileName.toStdString(), *molecule_);
  }
  else if (info.suffix() == "cjson") {
    Io::CjsonFormat cjson;
    molecule_ = new Molecule;
    success = cjson.readFile(fileName.toStdString(), *molecule_);
  }
  if (success) {
    m_recentFiles.prepend(fileName);
    updateRecentFiles();
    setMolecule(molecule_);
    statusBar()->showMessage(tr("Molecule loaded (%1 atoms, %2 bonds)")
                             .arg(molecule_->atomCount())
                             .arg(molecule_->bondCount()), 2500);
    setWindowTitle(tr("Avogadro - %1").arg(fileName));
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

#ifdef QTTESTING
void MainWindow::record()
{
  QString fileName = QFileDialog::getSaveFileName(this, "Test file name",
                                                  QString(), "XML Files (*.xml)");
  if (!fileName.isEmpty())
    m_testUtility->recordTests(fileName);
}

void MainWindow::play()
{
  QString fileName = QFileDialog::getOpenFileName(this, "Test file name",
                                                  QString(), "XML Files (*.xml)");
  if (!fileName.isEmpty())
    m_testUtility->playTests(fileName);
}

void MainWindow::playTest()
{
  if (!m_testFile.isEmpty()) {
    m_testUtility->playTests(m_testFile);
    if (m_testExit)
      close();
  }
}

void MainWindow::popup()
{
  QDialog dialog;
  QHBoxLayout* hbox = new QHBoxLayout(&dialog);
  QPushButton button("Click to Close", &dialog);
  hbox->addWidget(&button);
  QObject::connect(&button, SIGNAL(clicked()), &dialog, SLOT(accept()));
  dialog.exec();
}
#endif

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
  m_elementLookup.push_back(0); // for the separator

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
