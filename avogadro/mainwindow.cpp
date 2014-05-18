/******************************************************************************

  This source file is part of the Avogadro project.

  Copyright 2012-2014 Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/

#include "mainwindow.h"
#include "aboutdialog.h"
#include "menubuilder.h"
#include "backgroundfileformat.h"
#include "avogadroappconfig.h"
#include "viewfactory.h"

#include <avogadro/core/elements.h>
#include <avogadro/io/cjsonformat.h>
#include <avogadro/io/cmlformat.h>
#include <avogadro/io/fileformat.h>
#include <avogadro/io/fileformatmanager.h>
#include <avogadro/qtopengl/glwidget.h>
#include <avogadro/qtopengl/editglwidget.h>
#include <avogadro/qtplugins/pluginmanager.h>
#include <avogadro/qtgui/customelementdialog.h>
#include <avogadro/qtgui/fileformatdialog.h>
#include <avogadro/qtgui/sceneplugin.h>
#include <avogadro/qtgui/scenepluginmodel.h>
#include <avogadro/qtgui/toolplugin.h>
#include <avogadro/qtgui/extensionplugin.h>
#include <avogadro/qtgui/periodictableview.h>
#include <avogadro/qtgui/molecule.h>
#include <avogadro/qtgui/moleculemodel.h>
#include <avogadro/qtgui/multiviewwidget.h>
#include <avogadro/qtgui/rwmolecule.h>
#include <avogadro/rendering/glrenderer.h>
#include <avogadro/rendering/scene.h>

#include <QtWidgets/QApplication>
#include <QtCore/QString>
#include <QtCore/QDebug>
#include <QtCore/QFileInfo>
#include <QtCore/QSettings>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtWidgets/QActionGroup>
#include <QtGui/QCloseEvent>
#include <QtWidgets/QInputDialog>
#include <QtGui/QKeySequence>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QHBoxLayout>

#include <QtWidgets/QDockWidget>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QProgressDialog>
#include <QtWidgets/QPushButton>

#ifdef QTTESTING
# include <pqTestUtility.h>
# include <pqEventObserver.h>
# include <pqEventSource.h>
# include <QXmlStreamReader>
#endif

#ifdef Avogadro_ENABLE_RPC
# include <molequeue/client/client.h>
#endif // Avogadro_ENABLE_RPC

#include <avogadro/vtk/vtkglwidget.h>

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
    if (this->XMLStream) {
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

using std::string;
using std::vector;
using Io::FileFormat;
using Io::FileFormatManager;
using QtGui::CustomElementDialog;
using QtGui::FileFormatDialog;
using QtGui::Molecule;
using QtGui::RWMolecule;
using QtGui::ScenePlugin;
using QtGui::ScenePluginFactory;
using QtGui::ScenePluginModel;
using QtGui::ToolPlugin;
using QtGui::ToolPluginFactory;
using QtGui::ExtensionPlugin;
using QtGui::ExtensionPluginFactory;
using QtOpenGL::EditGLWidget;
using QtOpenGL::GLWidget;
using VTK::vtkGLWidget;
using QtPlugins::PluginManager;

MainWindow::MainWindow(const QStringList &fileNames, bool disableSettings)
  : m_molecule(NULL),
    m_rwMolecule(NULL),
    m_moleculeModel(NULL),
    m_queuedFilesStarted(false),
    m_menuBuilder(new MenuBuilder),
    m_fileReadThread(NULL),
    m_fileWriteThread(NULL),
    m_threadedReader(NULL),
    m_threadedWriter(NULL),
    m_progressDialog(NULL),
    m_fileReadMolecule(NULL),
    m_fileToolBar(new QToolBar(this)),
    m_editToolBar(new QToolBar(this)),
    m_toolToolBar(new QToolBar(this)),
    m_moleculeDirty(false),
    m_undo(NULL), m_redo(NULL),
    m_viewFactory(new ViewFactory)
{
  // If disable settings, ensure we create a cleared QSettings object.
  if (disableSettings) {
    QSettings settings;
    settings.clear();
    settings.sync();
  }
  // The default settings will be used if everything was cleared.
  readSettings();

  // Now load the plugins.
  PluginManager *plugin = PluginManager::instance();
  plugin->load();

  // Now set up the interface.
  setupInterface();

  // Call this a second time, not needed but ensures plugins only load once.
  plugin->load();

  QList<ExtensionPluginFactory *> extensions =
      plugin->pluginFactories<ExtensionPluginFactory>();
  qDebug() << "Extension plugins dynamically found..." << extensions.size();
  foreach (ExtensionPluginFactory *factory, extensions) {
    ExtensionPlugin *extension = factory->createInstance();
    if (extension) {
      extension->setParent(this);
      connect(this, SIGNAL(moleculeChanged(QtGui::Molecule*)),
              extension, SLOT(setMolecule(QtGui::Molecule*)));
      connect(extension, SIGNAL(moleculeReady(int)), SLOT(moleculeReady(int)));
      connect(extension, SIGNAL(fileFormatsReady()), SLOT(fileFormatsReady()));
      connect(extension, SIGNAL(requestActiveTool(QString)),
              SLOT(setActiveTool(QString)));
      connect(extension, SIGNAL(requestActiveDisplayTypes(QStringList)),
              SLOT(setActiveDisplayTypes(QStringList)));
      buildMenu(extension);
    }
  }

  // Build up the standard menus, incorporate dynamic menus.
  buildMenu();
  updateRecentFiles();

  // Try to open the file(s) passed in.
  if (!fileNames.isEmpty()) {
    m_queuedFiles = fileNames;
    // Give the plugins 5 seconds before timing out queued files.
    QTimer::singleShot(5000, this, SLOT(clearQueuedFiles()));
  }

#ifdef Avogadro_ENABLE_RPC
  // Wait a few seconds to attempt registering with MoleQueue.
  QTimer::singleShot(3000, this, SLOT(registerMoleQueue()));
#endif // Avogadro_ENABLE_RPC

  statusBar()->showMessage(tr("Ready..."), 2000);

  updateWindowTitle();
}

MainWindow::~MainWindow()
{
  writeSettings();
  delete m_molecule;
  delete m_menuBuilder;
  delete m_viewFactory;
}

void MainWindow::setupInterface()
{
  // We take care of setting up the main interface here, along with any custom
  // pieces that might be added for saved settings etc.
  m_multiViewWidget = new QtGui::MultiViewWidget(this);
  m_multiViewWidget->setFactory(m_viewFactory);
  setCentralWidget(m_multiViewWidget);
  GLWidget *glWidget = new GLWidget(this);
  m_multiViewWidget->addWidget(glWidget);

  // Our tool dock.
  m_toolDock = new QDockWidget(tr("Tool"), this);
  addDockWidget(Qt::LeftDockWidgetArea, m_toolDock);

  // Our scene/view dock.
  QDockWidget *sceneDock = new QDockWidget(tr("Display Types"), this);
  m_sceneTreeView = new QTreeView(sceneDock);
  sceneDock->setWidget(m_sceneTreeView);
  addDockWidget(Qt::LeftDockWidgetArea, sceneDock);

  // Our view dock.
  m_viewDock = new QDockWidget(tr("View Configuration"), this);
  addDockWidget(Qt::LeftDockWidgetArea, m_viewDock);

  // Our molecule dock.
  QDockWidget *moleculeDock = new QDockWidget(tr("Molecules"), this);
  m_moleculeTreeView = new QTreeView(moleculeDock);
  moleculeDock->setWidget(m_moleculeTreeView);
  addDockWidget(Qt::LeftDockWidgetArea, moleculeDock);

  // Switch to our fallback icons if there are no platform-specific icons.
  if (!QIcon::hasThemeIcon("document-new"))
    QIcon::setThemeName("fallback");

  QIcon icon(":/icons/avogadro.png");
  setWindowIcon(icon);

  m_fileToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  addToolBar(m_fileToolBar);
  addToolBar(m_editToolBar);
  addToolBar(m_toolToolBar);

  // Create the scene plugin model
  m_sceneTreeView->setAlternatingRowColors(true);
  m_sceneTreeView->header()->stretchLastSection();
  m_sceneTreeView->header()->setVisible(false);
  connect(m_sceneTreeView, SIGNAL(activated(QModelIndex)),
          SLOT(sceneItemActivated(QModelIndex)));

  // Create the molecule model
  m_moleculeModel = new QtGui::MoleculeModel(this);
  m_moleculeTreeView->setModel(m_moleculeModel);
  m_moleculeTreeView->setAlternatingRowColors(true);
  m_moleculeTreeView->header()->stretchLastSection();
  m_moleculeTreeView->header()->setVisible(false);
  connect(m_moleculeTreeView, SIGNAL(activated(QModelIndex)),
          SLOT(moleculeActivated(QModelIndex)));

  viewActivated(glWidget);
  buildTools();
  // Connect to the invalid context signal, check whether GL is initialized.
  //connect(m_glWidget, SIGNAL(rendererInvalid()), SLOT(rendererInvalid()));
  connect(m_multiViewWidget, SIGNAL(activeWidgetChanged(QWidget*)),
          SLOT(viewActivated(QWidget*)));
}

void MainWindow::closeEvent(QCloseEvent *e)
{
  if (!saveFileIfNeeded()) {
    e->ignore();
    return;
  }
  writeSettings();
  QMainWindow::closeEvent(e);
}

void MainWindow::moleculeReady(int)
{
  ExtensionPlugin *extension = qobject_cast<ExtensionPlugin *>(sender());
  if (extension) {
    Molecule *mol = new Molecule(this);
    if (extension->readMolecule(*mol))
      setMolecule(mol);
  }
}

void MainWindow::newMolecule()
{
  setMolecule(new Molecule(this));
}

template <class T, class M>
void setWidgetMolecule(T *glWidget, M *mol)
{
  glWidget->setMolecule(mol);
  glWidget->updateScene();
  glWidget->resetCamera();
}

void MainWindow::setMolecule(Molecule *mol)
{
  if (!mol)
    return;

  // Set the new molecule, ensure both molecules are in the model.
  if (m_molecule && !m_moleculeModel->molecules().contains(m_molecule))
    m_moleculeModel->addItem(m_molecule);
  if (!m_moleculeModel->molecules().contains(mol))
    m_moleculeModel->addItem(mol);

  Molecule *oldMolecule(m_molecule);
  m_molecule = mol;

  // If the molecule is empty, make the editor active. Otherwise, use the
  // navigator tool.
  if (m_molecule) {
    //QString targetToolName = m_molecule->atomCount() > 0 ? "Navigator"
    //                                                     : "Editor";
    //setActiveTool(targetToolName);
    connect(m_molecule, SIGNAL(changed(uint)), SLOT(markMoleculeDirty()));
  }

  emit moleculeChanged(m_molecule);
  markMoleculeClean();
  updateWindowTitle();
  m_moleculeModel->setActiveMolecule(m_molecule);

  if (oldMolecule)
    oldMolecule->disconnect(this);

  // Check if the molecule needs to update the current one.
  QWidget *w = m_multiViewWidget->activeWidget();
  if (GLWidget *glWidget = qobject_cast<QtOpenGL::GLWidget *>(w)) {
    setWidgetMolecule(glWidget, mol);
  }
  else if (EditGLWidget *editWidget = qobject_cast<EditGLWidget *>(w)) {
    RWMolecule *rwMol = new RWMolecule(*mol, this);
    qDebug() << "rwMol with" << rwMol->atomCount() << "atoms";
    m_moleculeModel->addItem(rwMol);
    m_moleculeModel->setActiveMolecule(rwMol);
    setWidgetMolecule(editWidget, rwMol);
  }
  else if (vtkGLWidget *vtkWidget = qobject_cast<vtkGLWidget *>(w)) {
    setWidgetMolecule(vtkWidget, mol);
  }
}

void MainWindow::setMolecule(RWMolecule *rwMol)
{
  if (!rwMol)
    return;

  // It will ensure the molecule is unique.
  m_moleculeModel->addItem(rwMol);

  //emit moleculeChanged(m_molecule);
  //markMoleculeClean();
  //updateWindowTitle();
  m_moleculeModel->setActiveMolecule(rwMol);

  // Check if the molecule needs to update the current one.
  QWidget *w = m_multiViewWidget->activeWidget();
  if (GLWidget *glWidget = qobject_cast<QtOpenGL::GLWidget *>(w)) {
    Molecule *mol = new Molecule(*rwMol, this);
    m_moleculeModel->addItem(mol);
    m_moleculeModel->setActiveMolecule(mol);
    setWidgetMolecule(glWidget, mol);
  }
  else if (EditGLWidget *editWidget = qobject_cast<EditGLWidget *>(w)) {
    setWidgetMolecule(editWidget, rwMol);
  }
  else if (vtkGLWidget *vtkWidget = qobject_cast<vtkGLWidget *>(w)) {
    Molecule *mol = new Molecule(*rwMol, this);
    m_moleculeModel->addItem(mol);
    m_moleculeModel->setActiveMolecule(mol);
    setWidgetMolecule(vtkWidget, mol);
  }
}

void MainWindow::markMoleculeDirty()
{
  if (!m_moleculeDirty) {
    m_moleculeDirty = true;
    updateWindowTitle();
  }
}

void MainWindow::markMoleculeClean()
{
  if (m_moleculeDirty) {
    m_moleculeDirty = false;
    updateWindowTitle();
  }
}

void MainWindow::updateWindowTitle()
{
  QString fileName = tr("Untitled");

  if (m_molecule && m_molecule->hasData("fileName"))
    fileName = QString::fromStdString(m_molecule->data("fileName").toString());

  setWindowTitle(tr("%1%2 - Avogadro %3")
                 .arg(QFileInfo(fileName).fileName())
                 .arg(m_moleculeDirty ? "*" : "")
                 .arg(AvogadroApp_VERSION));
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
}

void MainWindow::openFile()
{
  if (!saveFileIfNeeded())
    return;

  QString filter(QString("%1 (*.cml);;%2 (*.cjson)")
                 .arg(tr("Chemical Markup Language"))
                 .arg(tr("Chemical JSON")));

  QSettings settings;
  QString dir = settings.value("MainWindow/lastOpenDir").toString();

  QString fileName = QFileDialog::getOpenFileName(this,
                                                  tr("Open chemical file"),
                                                  dir, filter);

  if (fileName.isEmpty()) // user cancel
    return;

  QFileInfo info(fileName);
  dir = info.absoluteDir().absolutePath();
  settings.setValue("MainWindow/lastOpenDir", dir);

  // Create one of our readers to read the file:
  QString extension = info.suffix().toLower();
  FileFormat *reader = NULL;
  if (extension == "cml")
    reader = new Io::CmlFormat;
  else if (extension == "cjson")
    reader = new Io::CjsonFormat;

  if (!openFile(fileName, reader)) {
    QMessageBox::information(this, tr("Cannot open file"),
                             tr("Can't open supplied file %1").arg(fileName));
  }
}

void MainWindow::importFile()
{
  if (!saveFileIfNeeded())
    return;

  QSettings settings;
  QString dir = settings.value("MainWindow/lastOpenDir").toString();

  FileFormatDialog::FormatFilePair reply =
      QtGui::FileFormatDialog::fileToRead(this, tr("Import Molecule"), dir);

  if (reply.first == NULL) // user cancel
    return;

  dir = QFileInfo(reply.second).absoluteDir().absolutePath();
  settings.setValue("MainWindow/lastOpenDir", dir);

  if (!openFile(reply.second, reply.first->newInstance())) {
    QMessageBox::information(this, tr("Cannot open file"),
                             tr("Can't open supplied file %1")
                             .arg(reply.second));
  }
}

bool MainWindow::openFile(const QString &fileName, Io::FileFormat *reader)
{
  if (fileName.isEmpty() || reader == NULL) {
    delete reader;
    return false;
  }

  QString ident = QString::fromStdString(reader->identifier());

  // Prepare the background thread to read in the selected file.
  if (!m_fileReadThread)
    m_fileReadThread = new QThread(this);
  if (m_threadedReader)
    m_threadedReader->deleteLater();
  m_threadedReader = new BackgroundFileFormat(reader);
  if (m_fileReadMolecule)
    m_fileReadMolecule->deleteLater();
  m_fileReadMolecule = new Molecule(this);
  m_fileReadMolecule->setData("fileName", fileName.toStdString());
  m_threadedReader->moveToThread(m_fileReadThread);
  m_threadedReader->setMolecule(m_fileReadMolecule);
  m_threadedReader->setFileName(fileName);

  // Setup a progress dialog in case file loading is slow
  m_progressDialog = new QProgressDialog(this);
  m_progressDialog->setRange(0, 0);
  m_progressDialog->setValue(0);
  m_progressDialog->setMinimumDuration(750);
  m_progressDialog->setWindowTitle(tr("Reading File"));
  m_progressDialog->setLabelText(tr("Opening file '%1'\nwith '%2'")
                                   .arg(fileName).arg(ident));
  /// @todo Add API to abort file ops
  m_progressDialog->setCancelButton(NULL);
  connect(m_fileReadThread, SIGNAL(started()), m_threadedReader, SLOT(read()));
  connect(m_threadedReader, SIGNAL(finished()), m_fileReadThread, SLOT(quit()));
  connect(m_threadedReader, SIGNAL(finished()),
          SLOT(backgroundReaderFinished()));

  // Start the file operation
  m_fileReadThread->start();
  m_progressDialog->show();

  return true;
}

void MainWindow::backgroundReaderFinished()
{
  QString fileName = m_threadedReader->fileName();
  if (m_progressDialog->wasCanceled()) {
    delete m_fileReadMolecule;
  }
  else if (m_threadedReader->success()) {
    if (!fileName.isEmpty()) {
      m_fileReadMolecule->setData("fileName", fileName.toStdString());
      m_recentFiles.prepend(fileName);
      updateRecentFiles();
    }
    else {
      m_fileReadMolecule->setData("fileName", Core::Variant());
    }
    setMolecule(m_fileReadMolecule);
    statusBar()->showMessage(tr("Molecule loaded (%1 atoms, %2 bonds)")
                             .arg(m_molecule->atomCount())
                             .arg(m_molecule->bondCount()), 5000);
  }
  else {
    QMessageBox::critical(this, tr("File error"),
                          tr("Error while reading file '%1':\n%2")
                          .arg(fileName)
                          .arg(m_threadedReader->error()));
    delete m_fileReadMolecule;
  }
  m_fileReadThread->deleteLater();
  m_fileReadThread = NULL;
  m_threadedReader->deleteLater();
  m_threadedReader = NULL;
  m_fileReadMolecule = NULL;
  m_progressDialog->hide();
  m_progressDialog->deleteLater();
  m_progressDialog = NULL;

  reassignCustomElements();

  if (!m_queuedFiles.empty())
    readQueuedFiles();
}

bool MainWindow::backgroundWriterFinished()
{
  QString fileName = m_threadedWriter->fileName();
  bool success = false;
  if (!m_progressDialog->wasCanceled()) {
    if (m_threadedWriter->success()) {
      statusBar()->showMessage(tr("File written: %1")
                               .arg(fileName));
      m_threadedWriter->molecule()->setData("fileName", fileName.toStdString());
      markMoleculeClean();
      updateWindowTitle();
      success = true;
      }
    else {
      QMessageBox::critical(this, tr("File error"),
                            tr("Error while writing file '%1':\n%2")
                            .arg(fileName)
                            .arg(m_threadedWriter->error()));
      }
    }
  m_fileWriteThread->deleteLater();
  m_fileWriteThread = NULL;
  m_threadedWriter->deleteLater();
  m_threadedWriter = NULL;
  m_progressDialog->deleteLater();
  m_progressDialog = NULL;
  return success;
}

void MainWindow::toolActivated()
{
  if (QAction *action = qobject_cast<QAction*>(sender())) {
    if (GLWidget *glWidget =
        qobject_cast<GLWidget *>(m_multiViewWidget->activeWidget())) {
      glWidget->setActiveTool(action->data().toString());
      if (glWidget->activeTool()) {
        m_toolDock->setWidget(glWidget->activeTool()->toolWidget());
        m_toolDock->setWindowTitle(action->text());
      }
    }
    if (EditGLWidget *editWidget =
        qobject_cast<EditGLWidget *>(m_multiViewWidget->activeWidget())) {
      editWidget->setActiveTool(action->data().toString());
      if (editWidget->activeTool()) {
        m_toolDock->setWidget(editWidget->activeTool()->toolWidget());
        m_toolDock->setWindowTitle(action->text());
      }
    }
  }
}

void MainWindow::viewConfigActivated()
{

}

void MainWindow::rendererInvalid()
{
  GLWidget *widget = qobject_cast<GLWidget *>(sender());
  QMessageBox::warning(this, tr("Error: Failed to initialize OpenGL context"),
                       tr("OpenGL 2.0 or greater required, exiting.\n\n%1")
                       .arg(widget ? widget->error() : tr("Unknown error")));
  // Process events, and then set a single shot timer. This is needed to ensure
  // the RPC server also exits cleanly.
  QApplication::processEvents();
  QTimer::singleShot(500, this, SLOT(close()));
}

void MainWindow::moleculeActivated(const QModelIndex &idx)
{
  QObject *obj = static_cast<QObject *>(idx.internalPointer());
  if (Molecule *mol = qobject_cast<Molecule *>(obj))
    setMolecule(mol);
  else if (RWMolecule *rwMol = qobject_cast<RWMolecule *>(obj))
    setMolecule(rwMol);
}

void MainWindow::sceneItemActivated(const QModelIndex &idx)
{
  if (!idx.isValid())
    return;
  QObject *obj = static_cast<QObject *>(idx.internalPointer());
  if (ScenePlugin *scene = qobject_cast<ScenePlugin *>(obj))
    m_viewDock->setWidget(scene->setupWidget());
}

bool populatePluginModel(ScenePluginModel &model, bool editOnly = false)
{
  if (!model.scenePlugins().empty())
    return false;
  PluginManager *plugin = PluginManager::instance();
  QList<ScenePluginFactory *> scenePluginFactories =
      plugin->pluginFactories<ScenePluginFactory>();
  foreach (ScenePluginFactory *factory, scenePluginFactories) {
    ScenePlugin *scenePlugin = factory->createInstance();
    if (editOnly && scenePlugin) {
      if (scenePlugin->objectName() == "BallStick"
          || scenePlugin->objectName() == "OverlayAxes") {
        model.addItem(scenePlugin);
      }
      else {
        delete scenePlugin;
      }
    }
    else if (scenePlugin) {
      model.addItem(scenePlugin);
    }
  }
  return true;
}

template<class T>
bool populateTools(T* glWidget)
{
  if (!glWidget->tools().isEmpty())
    return false;
  PluginManager *plugin = PluginManager::instance();
  QList<ToolPluginFactory *> toolPluginFactories =
      plugin->pluginFactories<ToolPluginFactory>();
  foreach (ToolPluginFactory *factory, toolPluginFactories) {
    ToolPlugin *tool = factory->createInstance();
    if (tool)
      glWidget->addTool(tool);
  }
  glWidget->setDefaultTool(QObject::tr("Navigate tool"));
  glWidget->setActiveTool(QObject::tr("Navigate tool"));
  return true;
}

void MainWindow::viewActivated(QWidget *widget)
{

  if (GLWidget *glWidget = qobject_cast<GLWidget *>(widget)) {
    bool firstRun = populatePluginModel(glWidget->sceneModel());
    m_sceneTreeView->setModel(&glWidget->sceneModel());
    populateTools(glWidget);

    m_editToolBar->setDisabled(true);

    if (firstRun) {
      setActiveTool("Navigator");
      glWidget->updateScene();
    }
    else {
      m_moleculeModel->setActiveMolecule(glWidget->molecule());
      // Figure out the active tool - reflect this in the toolbar.
      ToolPlugin *tool = glWidget->activeTool();
      if (tool) {
        QString name = tool->objectName();
        foreach(QAction *action, m_toolToolBar->actions()) {
          if (action->data().toString() == name)
            action->setChecked(true);
          else
            action->setChecked(false);
        }
      }
    }
    if (m_molecule != glWidget->molecule() && glWidget->molecule()) {
      m_rwMolecule = 0;
      m_molecule = glWidget->molecule();
      emit moleculeChanged(m_molecule);
      m_moleculeModel->setActiveMolecule(m_molecule);
    }
  }
  else if (EditGLWidget *editWidget = qobject_cast<EditGLWidget *>(widget)) {
    bool firstRun = populatePluginModel(editWidget->sceneModel(), true);
    m_sceneTreeView->setModel(&editWidget->sceneModel());
    populateTools(editWidget);

    m_editToolBar->setDisabled(false);

    if (firstRun) {
      setActiveTool("Editor");
      RWMolecule *rwMol = new RWMolecule(this);
      m_moleculeModel->addItem(rwMol);
      m_moleculeModel->setActiveMolecule(rwMol);
      editWidget->setMolecule(rwMol);
      editWidget->updateScene();
      m_rwMolecule = rwMol;
      m_molecule = NULL;
      connect(&rwMol->undoStack(), SIGNAL(canRedoChanged(bool)),
              m_redo, SLOT(setEnabled(bool)));
      connect(&rwMol->undoStack(), SIGNAL(canUndoChanged(bool)),
              m_undo, SLOT(setEnabled(bool)));
    }
    else {
      m_moleculeModel->setActiveMolecule(editWidget->molecule());
      // Figure out the active tool - reflect this in the toolbar.
      ToolPlugin *tool = editWidget->activeTool();
      if (tool) {
        QString name = tool->objectName();
        foreach(QAction *action, m_toolToolBar->actions()) {
          if (action->data().toString() == name)
            action->setChecked(true);
          else
            action->setChecked(false);
        }
      }
    }
    if (m_rwMolecule != editWidget->molecule() && editWidget->molecule()) {
      m_molecule = 0;
      m_rwMolecule = editWidget->molecule();
      //emit moleculeChanged(m_rwMolecule);
      m_moleculeModel->setActiveMolecule(m_rwMolecule);
    }
  }
  else if (vtkGLWidget *vtkWidget = qobject_cast<vtkGLWidget*>(widget)) {
    bool firstRun = populatePluginModel(vtkWidget->sceneModel());
    m_sceneTreeView->setModel(&vtkWidget->sceneModel());

    m_editToolBar->setDisabled(true);

    if (firstRun) {
      setActiveTool("Navigator");
      vtkWidget->updateScene();
    }
    else {
      m_moleculeModel->setActiveMolecule(vtkWidget->molecule());
    }
    if (m_molecule != vtkWidget->molecule() && vtkWidget->molecule()) {
      m_rwMolecule = NULL;
      m_molecule = vtkWidget->molecule();
      emit moleculeChanged(m_molecule);
      m_moleculeModel->setActiveMolecule(m_molecule);
    }
  }
  updateWindowTitle();
  activeMoleculeEdited();
}

void MainWindow::reassignCustomElements()
{
  if (m_molecule && m_molecule->hasCustomElements())
    CustomElementDialog::resolve(this, *m_molecule);
}

void MainWindow::openRecentFile()
{
  QAction *action = qobject_cast<QAction *>(sender());
  if (action) {
    QString fileName = action->data().toString();

    const FileFormat *format = FileFormatDialog::findFileFormat(
          this, tr("Select file reader"), fileName,
          FileFormat::File | FileFormat::Read);

    if(!openFile(fileName, format ? format->newInstance() : NULL)) {
      QMessageBox::information(this, tr("Cannot open file"),
                               tr("Can't open supplied file %1").arg(fileName));
    }
  }
}

void MainWindow::updateRecentFiles()
{
  m_recentFiles.removeDuplicates();
  while (m_recentFiles.size() > 10)
    m_recentFiles.removeLast();

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

bool MainWindow::saveFile(bool async)
{
  QObject *molObj = m_moleculeModel->activeMolecule();

  if (!molObj)
    return false;

  Molecule *mol = qobject_cast<Molecule *>(molObj);
  if (!mol) {
    RWMolecule *rwMol = qobject_cast<RWMolecule *>(molObj);
    if (!rwMol)
      return false;
    return saveFileAs(async);
  }

  if (!mol->hasData("fileName"))
    return saveFileAs(async);

  string fileName = mol->data("fileName").toString();
  QString extension =
      QFileInfo(QString::fromStdString(fileName)).suffix().toLower();

  // Was the original format standard, or imported?
  if (extension == QLatin1String("cml")) {
    return saveFileAs(QString::fromStdString(fileName), new Io::CmlFormat,
                      async);
  }
  else if (extension == QLatin1String("cjson")) {
    return saveFileAs(QString::fromStdString(fileName), new Io::CjsonFormat,
                      async);
  }


  // is the imported format writable?
  bool writable = !FileFormatManager::instance().fileFormatsFromFileExtension(
        extension.toStdString(), FileFormat::File | FileFormat::Read).empty();
  if (writable) {
    // Warn the user that the format may lose data.
    QMessageBox box(this);
    box.setModal(true);
    box.setWindowTitle(tr("Avogadro"));
    box.setText(tr("This file was imported from a non-standard format which "
                   "may not be able to write all of the information in the "
                   "molecule.\n\nWould you like to export to the current "
                   "format, or save in a standard format?"));
    QPushButton *saveButton(box.addButton(QMessageBox::Save));
    QPushButton *cancelButton(box.addButton(QMessageBox::Cancel));
    QPushButton *exportButton(box.addButton(tr("Export"),
                                            QMessageBox::DestructiveRole));
    box.setDefaultButton(saveButton);
    box.exec();
    if (box.clickedButton() == saveButton)
      return saveFileAs(async);
    else if (box.clickedButton() == cancelButton)
      return false;
    else if (box.clickedButton() == exportButton)
      return exportFile(async);
  }

  // Otherwise, prompt for a new valid format.
  return saveFileAs(async);
}

bool MainWindow::saveFileAs(bool async)
{
  QString filter(QString("%1 (*.cml);;%2 (*.cjson)")
                 .arg(tr("Chemical Markup Language"))
                 .arg(tr("Chemical JSON")));

  QSettings settings;
  QString dir = settings.value("MainWindow/lastSaveDir").toString();

  QString fileName = QFileDialog::getSaveFileName(this,
                                                  tr("Save chemical file"),
                                                  dir, filter);

  if (fileName.isEmpty()) // user cancel
    return false;

  QFileInfo info(fileName);
  dir = info.absoluteDir().absolutePath();
  settings.setValue("MainWindow/lastSaveDir", dir);

  // Create one of our writers to save the file:
  QString extension = info.suffix().toLower();
  FileFormat *writer = NULL;
  if (extension == "cml" || extension.isEmpty())
    writer = new Io::CmlFormat;
  else if (extension == "cjson")
    writer = new Io::CjsonFormat;
  if (extension.isEmpty())
    fileName += ".cml";

  return saveFileAs(fileName, writer, async);
}

bool MainWindow::exportFile(bool async)
{
  QSettings settings;
  QString dir = settings.value("MainWindow/lastSaveDir").toString();

  FileFormatDialog::FormatFilePair reply =
      QtGui::FileFormatDialog::fileToWrite(this, tr("Export Molecule"), dir);

  if (reply.first == NULL) // user cancel
    return false;

  dir = QFileInfo(reply.second).absoluteDir().absolutePath();
  settings.setValue("MainWindow/lastSaveDir", dir);

  return saveFileAs(reply.second, reply.first->newInstance(), async);
}

bool MainWindow::saveFileAs(const QString &fileName, Io::FileFormat *writer,
                            bool async)
{
  if (fileName.isEmpty() || writer == NULL) {
    delete writer;
    return false;
  }

  QString ident = QString::fromStdString(writer->identifier());

  // Figure out what molecule willl be saved, perform conversion if necessary.
  QObject *molObj = m_moleculeModel->activeMolecule();

  if (!molObj) {
    delete writer;
    return false;
  }

  // Initialize out writer.
  if (!m_fileWriteThread)
    m_fileWriteThread = new QThread(this);
  if (m_threadedWriter)
    m_threadedWriter->deleteLater();
  m_threadedWriter = new BackgroundFileFormat(writer);

  Molecule *mol = qobject_cast<Molecule *>(molObj);
  if (!mol) {
    RWMolecule *rwMol = qobject_cast<RWMolecule *>(molObj);
    if (!rwMol) {
      delete writer;
      return false;
    }
    mol = new Molecule(*rwMol, m_threadedWriter);
  }

  // Prepare the background thread to write the selected file.
  m_threadedWriter->moveToThread(m_fileWriteThread);
  m_threadedWriter->setMolecule(mol);
  m_threadedWriter->setFileName(fileName);

  // Setup a progress dialog in case file loading is slow
  m_progressDialog = new QProgressDialog(this);
  m_progressDialog->setRange(0, 0);
  m_progressDialog->setValue(0);
  m_progressDialog->setMinimumDuration(750);
  m_progressDialog->setWindowTitle(tr("Writing File"));
  m_progressDialog->setLabelText(tr("Writing file '%1'\nwith '%2'")
                                   .arg(fileName).arg(ident));
  /// @todo Add API to abort file ops
  m_progressDialog->setCancelButton(NULL);
  connect(m_fileWriteThread, SIGNAL(started()),
          m_threadedWriter, SLOT(write()));
  connect(m_threadedWriter, SIGNAL(finished()),
          m_fileWriteThread, SLOT(quit()));

  // Start the file operation
  m_progressDialog->show();
  if (async) {
    connect(m_threadedWriter, SIGNAL(finished()),
            SLOT(backgroundWriterFinished()));
    m_fileWriteThread->start();
    return true;
  }
  else {
    QTimer::singleShot(0, m_fileWriteThread, SLOT(start()));
    QEventLoop loop;
    connect(m_fileWriteThread, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();
    return backgroundWriterFinished();
  }
}

void MainWindow::setActiveTool(QString toolName)
{
  if (GLWidget *glWidget =
      qobject_cast<GLWidget *>(m_multiViewWidget->activeWidget())) {
    foreach (ToolPlugin *toolPlugin, glWidget->tools()) {
      if (toolPlugin->objectName() == toolName) {
        toolPlugin->activateAction()->triggered();
        glWidget->setActiveTool(toolPlugin);
      }
    }
  }
  else if (EditGLWidget *editWidget =
      qobject_cast<EditGLWidget *>(m_multiViewWidget->activeWidget())) {
    foreach (ToolPlugin *toolPlugin, editWidget->tools()) {
      if (toolPlugin->objectName() == toolName) {
        toolPlugin->activateAction()->triggered();
        editWidget->setActiveTool(toolPlugin);
      }
    }
  }

  if (!toolName.isEmpty()) {
    foreach(QAction *action, m_toolToolBar->actions()) {
      if (action->data().toString() == toolName)
        action->setChecked(true);
      else
        action->setChecked(false);
    }
  }
}

void MainWindow::setActiveDisplayTypes(QStringList displayTypes)
{
  ScenePluginModel *scenePluginModel(NULL);
  GLWidget *glWidget(NULL);
  VTK::vtkGLWidget *vtkWidget(NULL);
  if ((glWidget = qobject_cast<GLWidget *>(m_multiViewWidget->activeWidget()))) {
    scenePluginModel = &glWidget->sceneModel();
  }
  else if ((vtkWidget =
           qobject_cast<VTK::vtkGLWidget *>(m_multiViewWidget->activeWidget()))) {
    scenePluginModel = &vtkWidget->sceneModel();
  }

  foreach (ScenePlugin *scene, scenePluginModel->scenePlugins())
    scene->setEnabled(false);
  foreach (ScenePlugin *scene, scenePluginModel->scenePlugins())
    foreach (const QString &name, displayTypes)
      if (scene->objectName() == name)
        scene->setEnabled(true);
  if (glWidget)
    glWidget->updateScene();
  else if (vtkWidget)
    vtkWidget->updateScene();
}

void MainWindow::undoEdit()
{
  if (m_rwMolecule) {
    m_rwMolecule->undoStack().undo();
    m_rwMolecule->emitChanged(Molecule::Atoms | Molecule::Added);
    activeMoleculeEdited();
  }
}

void MainWindow::redoEdit()
{
  if (m_rwMolecule) {
    m_rwMolecule->undoStack().redo();
    m_rwMolecule->emitChanged(Molecule::Atoms | Molecule::Added);
    activeMoleculeEdited();
  }
}

void MainWindow::activeMoleculeEdited()
{
  if (!m_undo || !m_redo)
    return;
  if (m_rwMolecule) {
    if (m_rwMolecule->undoStack().canUndo())
      m_undo->setEnabled(true);
    else
      m_undo->setEnabled(false);
    if (m_rwMolecule->undoStack().canRedo())
      m_redo->setEnabled(true);
    else
      m_redo->setEnabled(false);
  }
  else {
    m_undo->setEnabled(false);
    m_redo->setEnabled(false);
  }
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

void MainWindow::buildMenu()
{
#ifdef QTTESTING
  QStringList testingPath;
  testingPath << tr("&Testing");
  QAction *actionRecord = new QAction(this);
  actionRecord->setText(tr("Record test..."));
  m_menuBuilder->addAction(testingPath, actionRecord, 10);
  QAction *actionPlay = new QAction(this);
  actionPlay->setText(tr("Play test..."));
  m_menuBuilder->addAction(testingPath, actionPlay, 5);

  connect(actionRecord, SIGNAL(triggered()), SLOT(record()));
  connect(actionPlay, SIGNAL(triggered()), SLOT(play()));

  m_testUtility = new pqTestUtility(this);
  m_testUtility->addEventObserver("xml", new XMLEventObserver(this));
  m_testUtility->addEventSource("xml", new XMLEventSource(this));

  m_testExit = true;
#endif

  // Add the standard menu items:
  QStringList path;
  path << "&File";
  // New
  QAction *action = new QAction(tr("&New"), this);
  action->setShortcut(QKeySequence("Ctrl+N"));
  action->setIcon(QIcon::fromTheme("document-new"));
  m_menuBuilder->addAction(path, action, 999);
  m_fileToolBar->addAction(action);
  connect(action, SIGNAL(triggered()), SLOT(newMolecule()));
  // Open
  action = new QAction(tr("&Open"), this);
  action->setShortcut(QKeySequence("Ctrl+O"));
  action->setIcon(QIcon::fromTheme("document-open"));
  m_menuBuilder->addAction(path, action, 970);
  m_fileToolBar->addAction(action);
  connect(action, SIGNAL(triggered()), SLOT(openFile()));
  // Save
  action = new QAction(tr("&Save"), this);
  action->setShortcut(QKeySequence("Ctrl+S"));
  action->setIcon(QIcon::fromTheme("document-save"));
  m_menuBuilder->addAction(path, action, 965);
  connect(action, SIGNAL(triggered()), SLOT(saveFile()));
  // Save As
  action = new QAction(tr("Save &As"), this);
  action->setShortcut(QKeySequence("Ctrl+Shift+S"));
  action->setIcon(QIcon::fromTheme("document-save-as"));
  m_menuBuilder->addAction(path, action, 960);
  connect(action, SIGNAL(triggered()), SLOT(saveFileAs()));
  // Import
  action = new QAction(tr("&Import"), this);
  action->setShortcut(QKeySequence("Ctrl+Shift+O"));
  action->setIcon(QIcon::fromTheme("document-import"));
  m_menuBuilder->addAction(path, action, 950);
  m_fileToolBar->addAction(action);
  connect(action, SIGNAL(triggered()), SLOT(importFile()));
  // Export
  action = new QAction(tr("&Export"), this);
  m_menuBuilder->addAction(path, action, 940);
  action->setIcon(QIcon::fromTheme("document-export"));
  connect(action, SIGNAL(triggered()), SLOT(exportFile()));
  // Quit
  action = new QAction(tr("&Quit"), this);
  action->setShortcut(QKeySequence("Ctrl+Q"));
  action->setIcon(QIcon::fromTheme("application-exit"));
  m_menuBuilder->addAction(path, action, -200);
  connect(action, SIGNAL(triggered()), this, SLOT(close()));

  // Undo/redo
  QStringList editPath;
  editPath << tr("&Edit");
  m_undo = new QAction(tr("&Undo"), this);
  m_undo->setIcon(QIcon::fromTheme("edit-undo"));
  m_undo->setShortcut(QKeySequence("Ctrl+Z"));
  m_redo = new QAction(tr("&Redo"), this);
  m_redo->setIcon(QIcon::fromTheme("edit-redo"));
  m_redo->setShortcut(QKeySequence("Ctrl+Shift+Z"));
  m_undo->setEnabled(false);
  m_redo->setEnabled(false);
  connect(m_undo, SIGNAL(triggered()), SLOT(undoEdit()));
  connect(m_redo, SIGNAL(triggered()), SLOT(redoEdit()));
  m_menuBuilder->addAction(editPath, m_undo, 1);
  m_menuBuilder->addAction(editPath, m_redo, 0);

  // Periodic table.
  QStringList extensionsPath;
  extensionsPath << tr("&Extensions");
  action = new QAction("&Periodic Table", this);
  m_menuBuilder->addAction(extensionsPath, action, 0);
  QtGui::PeriodicTableView *periodicTable = new QtGui::PeriodicTableView(this);
  connect(action, SIGNAL(triggered()), periodicTable, SLOT(show()));

  QStringList helpPath;
  helpPath << tr("&Help");
  QAction *about = new QAction("&About", this);
  about->setIcon(QIcon::fromTheme("help-about"));
  m_menuBuilder->addAction(helpPath, about, 20);
  connect(about, SIGNAL(triggered()), SLOT(showAboutDialog()));

  // Populate the recent file actions list.
  path << "Recent Files";
  for (int i = 0; i < 10; ++i) {
    action = new QAction(QString::number(i), this);
    m_actionRecentFiles.push_back(action);
    action->setIcon(QIcon::fromTheme("document-open-recent"));
    action->setVisible(false);
    m_menuBuilder->addAction(path, action, 995 - i);
    connect(action, SIGNAL(triggered()), SLOT(openRecentFile()));
  }
  m_actionRecentFiles[0]->setText(tr("No recent files"));
  m_actionRecentFiles[0]->setVisible(true);
  m_actionRecentFiles[0]->setEnabled(false);

  // Now actually add all menu entries.
  m_menuBuilder->buildMenu(menuBar());
}

void MainWindow::buildMenu(QtGui::ExtensionPlugin *extension)
{
  foreach (QAction *action, extension->actions())
    m_menuBuilder->addAction(extension->menuPath(action), action);
}

void MainWindow::buildTools()
{
  PluginManager *plugin = PluginManager::instance();

  // Now the tool plugins need to be built/added.
  QList<ToolPluginFactory *> toolPluginFactories =
      plugin->pluginFactories<ToolPluginFactory>();
  foreach (ToolPluginFactory *factory, toolPluginFactories) {
    ToolPlugin *tool = factory->createInstance();
    if (tool)
      m_tools << tool;
  }

  QActionGroup *editActions = new QActionGroup(this);
  QActionGroup *toolActions = new QActionGroup(this);
  int index = 0;
  foreach (ToolPlugin *toolPlugin, m_tools) {
    // Add action to toolbar.
    toolPlugin->setParent(this);
    QAction *action = toolPlugin->activateAction();
    action->setParent(toolPlugin);
    action->setCheckable(true);
    if (index + 1 < 10)
      action->setShortcut(QKeySequence(QString("Ctrl+%1").arg(index + 1)));
    action->setData(toolPlugin->objectName());
    if (toolPlugin->objectName() == "Editor"
        || toolPlugin->objectName() == "Manipulator"
        || toolPlugin->objectName() == "BondCentric") {
      editActions->addAction(action);
    }
    else {
      qDebug() << toolPlugin->objectName() << "added";
      toolActions->addAction(action);
    }
    connect(action, SIGNAL(triggered()), SLOT(toolActivated()));
    ++index;
  }

  m_editToolBar->addActions(editActions->actions());
  m_toolToolBar->addActions(toolActions->actions());
}

QString MainWindow::extensionToWildCard(const QString &extension)
{
  // This is a list of "extensions" returned by OB that are not actually
  // file extensions, but rather the full filename of the file. These
  // will be used as-is in the filter string, while others will be prepended
  // with "*.".
  static QStringList nonExtensions;
  if (nonExtensions.empty()) {
    nonExtensions
        << "POSCAR"  // VASP input geometry
        << "CONTCAR" // VASP output geometry
        << "HISTORY" // DL-POLY history file
        << "CONFIG"  // DL-POLY config file
           ;
  }

  if (nonExtensions.contains(extension))
    return extension;
  return QString("*.%1").arg(extension);
}

QString MainWindow::generateFilterString(
    const std::vector<const Io::FileFormat *> &formats, bool addAllEntry)
{
  QString result;

  // Create a map that groups the file extensions by name:
  QMap<QString, QString> formatMap;
  for (vector<const FileFormat*>::const_iterator it = formats.begin(),
       itEnd = formats.end(); it != itEnd; ++it) {
    QString name(QString::fromStdString((*it)->name()));
    vector<string> exts = (*it)->fileExtensions();
    for (vector<string>::const_iterator eit = exts.begin(),
         eitEnd = exts.end(); eit != eitEnd; ++eit) {
      QString ext(QString::fromStdString(*eit));
      if (!formatMap.values(name).contains(ext)) {
        formatMap.insertMulti(name, ext);
      }
    }
  }

  // This holds all known extensions:
  QStringList allExtensions;

  foreach (const QString &desc, formatMap.uniqueKeys()) {
    QStringList extensions;
    foreach (QString extension, formatMap.values(desc))
      extensions << extensionToWildCard(extension);
    allExtensions << extensions;
    result += QString("%1 (%2);;").arg(desc, extensions.join(" "));
  }

  if (addAllEntry) {
    result.prepend(tr("All supported formats (%1);;All files (*);;")
                   .arg(allExtensions.join(" ")));
  }

  return result;
}

bool MainWindow::saveFileIfNeeded()
{
  if (m_moleculeDirty) {
    int response =
        QMessageBox::question(
          this, tr("Avogadro"),
          tr("Do you want to save the changes you made in the document?\n\n"
             "Your changes will be lost if you don't save them."),
          QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
          QMessageBox::Save);

    switch (static_cast<QMessageBox::StandardButton>(response)) {
    case QMessageBox::Save:
      // Synchronous save -- needed so that we don't lose changes if the writer
      // fails:
      return saveFile(/*async=*/false);
    case QMessageBox::Discard:
      markMoleculeClean();
      return true;
    default:
    case QMessageBox::Cancel:
      return false;
    }
  }

  return true;
}

void MainWindow::registerMoleQueue()
{
#ifdef Avogadro_ENABLE_RPC
  MoleQueue::Client client;
  if (!client.connectToServer() || !client.isConnected())
    return;

  // Get all extensions;
  typedef std::vector<std::string> StringList;
  FileFormatManager &ffm = FileFormatManager::instance();
  StringList exts = ffm.fileExtensions(FileFormat::Read | FileFormat::File);

  // Create patterns list
  QList<QRegExp> patterns;
  for (StringList::const_iterator it = exts.begin(), itEnd = exts.end();
       it != itEnd; ++it) {
    patterns << QRegExp(extensionToWildCard(QString::fromStdString(*it)),
                        Qt::CaseInsensitive, QRegExp::Wildcard);
  }

  // Register the executable:
  client.registerOpenWith("Avogadro2 (new)", qApp->applicationFilePath(),
                          patterns);

  client.registerOpenWith("Avogadro2 (running)", "avogadro", "openFile",
                          patterns);
#endif // Avogadro_ENABLE_RPC
}

void MainWindow::showAboutDialog()
{
  AboutDialog about(this);
  about.exec();
}

void MainWindow::fileFormatsReady()
{
  ExtensionPlugin *extension(qobject_cast<ExtensionPlugin *>(sender()));
  if (!extension)
    return;
  foreach (FileFormat *format, extension->fileFormats()) {
    if (!FileFormatManager::registerFormat(format)) {
      qWarning() << tr("Error while loading FileFormat with id '%1'.")
                    .arg(QString::fromStdString(format->identifier()));
      // Need to delete the format if the manager didn't take ownership:
      delete format;
    }
  }
  readQueuedFiles();
}

void MainWindow::readQueuedFiles()
{
  m_queuedFilesStarted = true;
  if (!m_queuedFiles.empty()) {
    QString file = m_queuedFiles.first();
    m_queuedFiles.removeFirst();
    const FileFormat *format = QtGui::FileFormatDialog::findFileFormat(
          this, tr("Select file reader"), file,
          FileFormat::File | FileFormat::Read, "Avogadro:");

    if (!openFile(file, format ? format->newInstance() : NULL)) {
      QMessageBox::warning(this, tr("Cannot open file"),
                           tr("Avogadro timed out and doesn't know how to open"
                              " '%1'.").arg(file));
    }
  }
}

void MainWindow::clearQueuedFiles()
{
  if (!m_queuedFilesStarted && !m_queuedFiles.isEmpty()) {
    QMessageBox::warning(this, tr("Cannot open files"),
                         tr("Avogadro timed out and cannot open"
                            " '%1'.").arg(m_queuedFiles.join("\n")));
    m_queuedFiles.clear();
  }
}

} // End of Avogadro namespace
