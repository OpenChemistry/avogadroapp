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
#include "aboutdialog.h"

#include <avogadro/qtgui/molecule.h>
#include <avogadro/core/elements.h>
#include <avogadro/io/fileformat.h>
#include <avogadro/io/fileformatmanager.h>
#include <avogadro/qtopengl/glwidget.h>
#include <avogadro/qtplugins/pluginmanager.h>
#include <avogadro/qtgui/sceneplugin.h>
#include <avogadro/qtgui/scenepluginmodel.h>
#include <avogadro/qtgui/toolplugin.h>
#include <avogadro/qtgui/extensionplugin.h>
#include <avogadro/qtgui/periodictableview.h>
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
#include <QtGui/QMessageBox>
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

  // Connect the menu actions
  connect(m_ui->actionNewMolecule, SIGNAL(triggered()), SLOT(newMolecule()));
  connect(m_ui->actionOpen, SIGNAL(triggered()), SLOT(openFile()));
  connect(m_ui->actionSaveAs, SIGNAL(triggered()), SLOT(saveFile()));
  connect(m_ui->actionExport, SIGNAL(triggered()), SLOT(exportFile()));
  connect(m_ui->actionQuit, SIGNAL(triggered()), qApp, SLOT(quit()));

  // If disable settings, ensure we create a cleared QSettings object.
  if (disableSettings) {
    QSettings settings;
    settings.clear();
    settings.sync();
  }
  // The default settings will be used if everything was cleared.
  readSettings();

  QtPlugins::PluginManager *plugin = QtPlugins::PluginManager::instance();
  plugin->load();

  QList<QtGui::ToolPluginFactory *> toolPluginFactories =
      plugin->pluginFactories<QtGui::ToolPluginFactory>();
  QList<QtGui::ToolPlugin*> toolPlugins;
  foreach (QtGui::ToolPluginFactory *factory, toolPluginFactories) {
    QtGui::ToolPlugin *tool = factory->createInstance();
    if (tool)
      toolPlugins << tool;
  }
  buildTools(toolPlugins);

  QList<QtGui::ScenePluginFactory *> scenePluginFactories =
      plugin->pluginFactories<QtGui::ScenePluginFactory>();
  foreach (QtGui::ScenePluginFactory *factory, scenePluginFactories) {
    QtGui::ScenePlugin *scenePlugin = factory->createInstance();
    if (scenePlugin) {
      scenePlugin->setParent(this);
      m_scenePluginModel->addItem(scenePlugin);
    }
  }

  QMenu *menuTop = menuBar()->addMenu(tr("&Extensions"));
  QAction *showPeriodicTable = new QAction("&Periodic Table", this);
  menuTop->addAction(showPeriodicTable);
  QtGui::PeriodicTableView *periodicTable = new QtGui::PeriodicTableView(this);
  connect(showPeriodicTable, SIGNAL(triggered()), periodicTable, SLOT(show()));

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
      foreach (Io::FileFormat *format, extension->fileFormats()) {
        qDebug() << "Registering " << format->identifier().c_str();
        if (!Io::FileFormatManager::registerFormat(format)) {
          qWarning() << tr("Error while loading FileFormat with id '%1'.")
                        .arg(QString::fromStdString(format->identifier()));
          // Need to delete the format if the manager didn't take ownership:
          delete format;
        }
      }
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

  QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
  QAction *about = new QAction("&About", this);
  helpMenu->addAction(about);
  connect(about, SIGNAL(triggered()), SLOT(about()));
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

  // If the molecule is empty, make the editor active. Otherwise, use the
  // navigator tool.
  if (m_molecule) {
    int index = m_molecule->atomCount() > 0
        ? m_ui->toolComboBox->findText("Navigate")
        : m_ui->toolComboBox->findText("Draw");
    if (index >= 0)
      m_ui->toolComboBox->setCurrentIndex(index);
  }

  emit moleculeChanged(m_molecule);

  connect(m_molecule, SIGNAL(changed(unsigned int)),
          SLOT(updateScenePlugins()));

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
  std::vector<const Io::FileFormat *> formats =
      Io::FileFormatManager::instance().fileFormats(Io::FileFormat::Read
                                                    | Io::FileFormat::File);

  QString filter(generateFilterString(formats));

  QSettings settings;
  QString dir = settings.value("MainWindow/lastOpenDir").toString();

  QString fileName = QFileDialog::getOpenFileName(this,
                                                  tr("Open chemical file"),
                                                  dir, filter);

  dir = QFileInfo(fileName).absoluteDir().absolutePath();
  settings.setValue("MainWindow/lastOpenDir", dir);

  openFile(fileName);
}

void MainWindow::openFile(const QString &fileName)
{
  if (fileName.isEmpty())
    return;

  Molecule *molecule_(new Molecule());
  if (Io::FileFormatManager::instance().readFile(*molecule_,
                                                 fileName.toStdString())) {
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

void MainWindow::saveFile()
{
  std::vector<const Io::FileFormat *> formats =
      Io::FileFormatManager::instance().fileFormats(Io::FileFormat::Write
                                                    | Io::FileFormat::File);

  QString filter(QString("%1 (*.cml);;%2 (*.cjson)")
                 .arg(tr("Chemical Markup Language"))
                 .arg(tr("Chemical JSON")));

  QSettings settings;
  QString dir = settings.value("MainWindow/lastSaveDir").toString();

  QString fileName = QFileDialog::getSaveFileName(this,
                                                  tr("Save chemical file"),
                                                  "", filter);

  dir = QFileInfo(fileName).absoluteDir().absolutePath();
  settings.setValue("MainWindow/lastSaveDir", dir);

  saveFile(fileName);
}

void MainWindow::exportFile()
{
  std::vector<const Io::FileFormat *> formats =
      Io::FileFormatManager::instance().fileFormats(Io::FileFormat::Write
                                                    | Io::FileFormat::File);

  QString filter(generateFilterString(formats, false));

  QSettings settings;
  QString dir = settings.value("MainWindow/lastSaveDir").toString();

  QString fileName = QFileDialog::getSaveFileName(this,
                                                  tr("Export chemical file"),
                                                  "", filter);

  dir = QFileInfo(fileName).absoluteDir().absolutePath();
  settings.setValue("MainWindow/lastSaveDir", dir);

  saveFile(fileName);
}

void MainWindow::saveFile(const QString &fileName)
{
  if (fileName.isEmpty() || !m_molecule)
    return;

  if (Io::FileFormatManager::instance().writeFile(*m_molecule,
                                                  fileName.toStdString())) {
    m_recentFiles.prepend(fileName);
    updateRecentFiles();
    statusBar()->showMessage(tr("Molecule saved (%1)").arg(fileName));
    setWindowTitle(tr("Avogadro - %1").arg(fileName));
  }
  else {
    QMessageBox::critical(this, tr("Error saving file"),
                          tr("The file could not be saved."),
                          QMessageBox::Ok, QMessageBox::Ok);
  }
}

void MainWindow::updateScenePlugins()
{
  Rendering::Scene &scene = m_ui->glWidget->renderer().scene();

  // Build up the scene with the scene plugins, creating the appropriate nodes.
  Rendering::GroupNode &node = scene.rootNode();
  node.clear();

  if (m_molecule) {
    Rendering::GroupNode *moleculeNode = new Rendering::GroupNode(&node);

    foreach (QtGui::ScenePlugin *scenePlugin,
             m_scenePluginModel->activeScenePlugins()) {
      Rendering::GroupNode *engineNode = new Rendering::GroupNode(moleculeNode);
      scenePlugin->process(*m_molecule, *engineNode);
    }
  }
  m_ui->glWidget->update();
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

void MainWindow::buildTools(QList<QtGui::ToolPlugin *> toolList)
{
  foreach (QtGui::ToolPlugin *toolPlugin, toolList) {
    // Setup tool:
    toolPlugin->setMolecule(m_molecule);

    // Connect tool:
    connect(this, SIGNAL(moleculeChanged(QtGui::Molecule*)),
            toolPlugin, SLOT(setMolecule(QtGui::Molecule*)));

    // Setup tool selection
    QAction *toolAction = toolPlugin->activateAction();
    m_ui->toolComboBox->addItem(toolAction->text());

    // Setup tool widget
    QWidget *toolWidget = toolPlugin->toolWidget();
    if (!toolWidget)
      toolWidget = new QWidget();
    m_ui->toolWidgetStack->addWidget(toolWidget);
  }

  connect(m_ui->toolComboBox, SIGNAL(currentIndexChanged(int)),
          m_ui->toolWidgetStack, SLOT(setCurrentIndex(int)));
  connect(m_ui->toolComboBox, SIGNAL(currentIndexChanged(QString)),
          m_ui->glWidget, SLOT(setActiveTool(QString)));

  /// @todo Where to put these? For now just throw them into the glwidget, but
  /// we should have a better place for them (maybe a singleton ToolWrangler?)
  m_ui->glWidget->setTools(toolList);
  m_ui->glWidget->setDefaultTool(tr("Navigate tool"));
  if (!toolList.isEmpty())
    m_ui->glWidget->setActiveTool(toolList.first());
}

QString MainWindow::generateFilterString(
    const std::vector<const Io::FileFormat *> &formats, bool addAllEntry)
{
  QString result;

  // Create a map that groups the file extensions by name:
  QMap<QString, QString> formatMap;
  for (std::vector<const Io::FileFormat*>::const_iterator it = formats.begin(),
       itEnd = formats.end(); it != itEnd; ++it) {
    QString name(QString::fromStdString((*it)->name()));
    std::vector<std::string> exts = (*it)->fileExtensions();
    for (std::vector<std::string>::const_iterator eit = exts.begin(),
         eitEnd = exts.end(); eit != eitEnd; ++eit) {
      QString ext(QString::fromStdString(*eit));
      if (!formatMap.values(name).contains(ext)) {
        formatMap.insertMulti(name, ext);
      }
    }
  }

  // This is a list of "extensions" returned by OB that are not actually
  // file extensions, but rather the full filename of the file. These
  // will be used as-is in the filter string, while others will be prepended
  // with "*.".
  QStringList nonExtensions;
  nonExtensions
      << "POSCAR"  // VASP input geometry
      << "CONTCAR" // VASP output geometry
      << "HISTORY" // DL-POLY history file
      << "CONFIG"  // DL-POLY config file
         ;

  // This holds all known extensions:
  QStringList allExtensions;

  foreach (const QString &desc, formatMap.uniqueKeys()) {
    QStringList extensions;
    foreach (QString extension, formatMap.values(desc)) {
      if (!nonExtensions.contains(extension))
        extension.prepend("*.");
      extensions << extension;
    }
    allExtensions << extensions;
    result += QString("%1 (%2);;").arg(desc, extensions.join(" "));
  }

  if (addAllEntry) {
    result.prepend(tr("All supported formats (%1);;All files (*);;")
                   .arg(allExtensions.join(" ")));
  }

  return result;
}

void MainWindow::about()
{
  AboutDialog about(this);
  about.exec();
}

} // End of Avogadro namespace
