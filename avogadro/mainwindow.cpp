/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#include "mainwindow.h"

#include "aboutdialog.h"
#include "avogadroappconfig.h"
#include "backgroundfileformat.h"
#include "menubuilder.h"
#include "tooltipfilter.h"
#include "ui_mainwindow.h"
#include "viewfactory.h"

#include <avogadro/core/elements.h>
#include <avogadro/io/cjsonformat.h>
#include <avogadro/io/cmlformat.h>
#include <avogadro/io/fileformat.h>
#include <avogadro/io/fileformatmanager.h>
#include <avogadro/qtgui/customelementdialog.h>
#include <avogadro/qtgui/extensionplugin.h>
#include <avogadro/qtgui/fileformatdialog.h>
#include <avogadro/qtgui/layermodel.h>
#include <avogadro/qtgui/molecule.h>
#include <avogadro/qtgui/moleculemodel.h>
#include <avogadro/qtgui/multiviewwidget.h>
#include <avogadro/qtgui/periodictableview.h>
#include <avogadro/qtgui/rwmolecule.h>
#include <avogadro/qtgui/sceneplugin.h>
#include <avogadro/qtgui/scenepluginmodel.h>
#include <avogadro/qtgui/toolplugin.h>
#include <avogadro/qtopengl/activeobjects.h>
#include <avogadro/qtopengl/glwidget.h>
#include <avogadro/qtplugins/pluginmanager.h>
#include <avogadro/rendering/glrenderer.h>
#include <avogadro/rendering/scene.h>

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QMimeData>
#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>
#include <QtCore/QString>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtGui/QCloseEvent>
#include <QtGui/QKeySequence>
#include <QtWidgets/QActionGroup>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QToolBar>

#include <QtWidgets/QColorDialog>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QProgressDialog>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeView>

#include <QToolButton>
#include <QtGui/QOpenGLFramebufferObject>

#ifdef QTTESTING
#include <QXmlStreamReader>
#include <pqEventObserver.h>
#include <pqEventSource.h>
#include <pqTestUtility.h>
#endif

#ifdef Avogadro_ENABLE_RPC
#include <molequeue/client/client.h>
#endif // Avogadro_ENABLE_RPC

#ifdef AVO_USE_VTK
#include <avogadro/vtk/vtkglwidget.h>
#endif

#ifdef Q_OS_MAC
#include "qcocoamessagebox.h"
#define MESSAGEBOX QCocoaMessageBox
#else
#define MESSAGEBOX QMessageBox
#endif

namespace Avogadro {

#ifdef QTTESTING
class XMLEventObserver : public pqEventObserver
{
  QXmlStreamWriter* XMLStream;
  QString XMLString;

public:
  XMLEventObserver(QObject* p)
    : pqEventObserver(p)
  {
    this->XMLStream = nullptr;
  }
  ~XMLEventObserver() { delete this->XMLStream; }

protected:
  virtual void setStream(QTextStream* stream)
  {
    if (this->XMLStream) {
      this->XMLStream->writeEndElement();
      this->XMLStream->writeEndDocument();
      delete this->XMLStream;
      this->XMLStream = nullptr;
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
  QXmlStreamReader* XMLStream;

public:
  XMLEventSource(QObject* p)
    : Superclass(p)
  {
    this->XMLStream = nullptr;
  }
  ~XMLEventSource() { delete this->XMLStream; }

protected:
  virtual void setContent(const QString& xmlfilename)
  {
    delete this->XMLStream;
    this->XMLStream = nullptr;

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

using Io::FileFormat;
using Io::FileFormatManager;
using QtGui::CustomElementDialog;
using QtGui::ExtensionPlugin;
using QtGui::ExtensionPluginFactory;
using QtGui::FileFormatDialog;
using QtGui::Molecule;
using QtGui::MultiViewWidget;
using QtGui::RWMolecule;
using QtGui::ScenePlugin;
using QtGui::ScenePluginFactory;
using QtGui::ScenePluginModel;
using QtGui::ToolPlugin;
using QtGui::ToolPluginFactory;
using QtOpenGL::ActiveObjects;
using QtOpenGL::GLWidget;
using QtPlugins::PluginManager;
using std::string;
using std::vector;
#ifdef AVO_USE_VTK
using VTK::vtkGLWidget;
#endif

MainWindow::MainWindow(const QStringList& fileNames, bool disableSettings)
  : m_molecule(nullptr)
  , m_rwMolecule(nullptr)
  , m_moleculeModel(nullptr)
  , m_layerModel(nullptr)
  , m_activeScenePlugin(nullptr)
  , m_queuedFilesStarted(false)
  , m_menuBuilder(new MenuBuilder)
  , m_fileReadThread(nullptr)
  , m_fileWriteThread(nullptr)
  , m_threadedReader(nullptr)
  , m_threadedWriter(nullptr)
  , m_progressDialog(nullptr)
  , m_fileReadMolecule(nullptr)
  , m_fileToolBar(new QToolBar(this))
  , m_toolToolBar(new QToolBar(this))
  , m_moleculeDirty(false)
  , m_undo(nullptr)
  , m_redo(nullptr)
  , m_viewFactory(new ViewFactory)
  , m_ui(new Ui::MainWindow)
{
  m_ui->setupUi(this);

  // If disable settings, ensure we create a cleared QSettings object.
  if (disableSettings) {
    QSettings settings;
    settings.clear();
    settings.sync();
  }
  // The default settings will be used if everything was cleared.
  readSettings();

  // Now load the plugins.
  PluginManager* plugin = PluginManager::instance();
  plugin->load();

  // Call this a second time, not needed but ensures plugins only load once.
  plugin->load();

  QList<ExtensionPluginFactory*> extensions =
    plugin->pluginFactories<ExtensionPluginFactory>();
  qDebug() << "Extension plugins dynamically found…" << extensions.size();
  foreach (ExtensionPluginFactory* factory, extensions) {
    ExtensionPlugin* extension = factory->createInstance();
    if (extension) {
      extension->setParent(this);
      connect(this, &MainWindow::moleculeChanged, extension,
              &QtGui::ExtensionPlugin::setMolecule);
      connect(extension, &QtGui::ExtensionPlugin::moleculeReady, this,
              &MainWindow::moleculeReady);
      connect(extension, &QtGui::ExtensionPlugin::fileFormatsReady, this,
              &MainWindow::fileFormatsReady);
      connect(extension, &QtGui::ExtensionPlugin::requestActiveTool, this,
              &MainWindow::setActiveTool);
      connect(extension, &QtGui::ExtensionPlugin::requestActiveDisplayTypes,
              this, &MainWindow::setActiveDisplayTypes);
      buildMenu(extension);
      m_extensions.append(extension);
    }
  }

  // Now set up the interface.
  setupInterface();

  // Build up the standard menus, incorporate dynamic menus.
  buildMenu();
  updateRecentFiles();

  // Try to open the file(s) passed in.
  if (!fileNames.isEmpty()) {
    m_queuedFiles = fileNames;
    // Give the plugins 5 seconds before timing out queued files.
    QTimer::singleShot(5000, this, &MainWindow::clearQueuedFiles);
  } else {
    newMolecule();
  }

#ifdef Avogadro_ENABLE_RPC
  // Wait a few seconds to attempt registering with MoleQueue.
  QTimer::singleShot(3000, this, &MainWindow::registerMoleQueue);
#endif // Avogadro_ENABLE_RPC

  statusBar()->showMessage(tr("Ready…"), 2000);

  updateWindowTitle();
}

MainWindow::~MainWindow()
{
  writeSettings();
  delete m_molecule;
  delete m_menuBuilder;
  delete m_viewFactory;
  delete m_ui;
}

void MainWindow::setupInterface()
{
  // We take care of setting up the main interface here, along with any custom
  // pieces that might be added for saved settings etc.
  setAcceptDrops(true); // allow drag-and-drop of files
  QSettings settings;

  m_multiViewWidget = new QtGui::MultiViewWidget(this);
  m_multiViewWidget->setFactory(m_viewFactory);
  setCentralWidget(m_multiViewWidget);
  GLWidget* glWidget = new GLWidget(this);

  // set the background color (alpha channel default should be opaque)
  QColor color =
    settings.value("backgroundColor", QColor(0, 0, 0, 255)).value<QColor>();
  Vector4ub cColor;
  cColor[0] = static_cast<unsigned char>(color.red());
  cColor[1] = static_cast<unsigned char>(color.green());
  cColor[2] = static_cast<unsigned char>(color.blue());
  cColor[3] = static_cast<unsigned char>(color.alpha());

  Avogadro::Rendering::Scene* scene = &glWidget->renderer().scene();
  scene->setBackgroundColor(cColor);

  m_multiViewWidget->addWidget(glWidget);
  ActiveObjects::instance().setActiveGLWidget(glWidget);

  // Our tool dock.
  m_toolDock = new QDockWidget(tr("Tool"), this);
  addDockWidget(Qt::LeftDockWidgetArea, m_toolDock);

  // Our scene/view dock.
  m_sceneDock = new QDockWidget(tr("Display Types"), this);
  m_sceneDock->setFeatures(m_sceneDock->features() &
                           ~QDockWidget::AllDockWidgetFeatures);

  m_sceneTreeView = new QTreeView(m_sceneDock);
  m_sceneTreeView->setIndentation(0);
  m_sceneTreeView->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_sceneTreeView->setSelectionMode(QAbstractItemView::SingleSelection);
  m_sceneDock->setWidget(m_sceneTreeView);
  addDockWidget(Qt::LeftDockWidgetArea, m_sceneDock);
  // Our view dock.
  m_viewDock = new QDockWidget(tr("View Configuration"), this);
  addDockWidget(Qt::LeftDockWidgetArea, m_viewDock);
  m_viewDock->setFeatures(m_viewDock->features() &
                          ~QDockWidget::AllDockWidgetFeatures);

  // Our molecule dock.
  QDockWidget* moleculeDock = new QDockWidget(tr("Molecules"), this);
  m_moleculeTreeView = new QTreeView(moleculeDock);
  m_moleculeTreeView->setIndentation(0);
  m_moleculeTreeView->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_moleculeTreeView->setSelectionMode(QAbstractItemView::SingleSelection);
  moleculeDock->setWidget(m_moleculeTreeView);
  addDockWidget(Qt::LeftDockWidgetArea, moleculeDock);

  // Our molecule dock.
  QDockWidget* layerDock = new QDockWidget(tr("Layers"), this);
  m_layerTreeView = new QTreeView(layerDock);
  m_layerTreeView->setIndentation(0);
  m_layerTreeView->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_layerTreeView->setSelectionMode(QAbstractItemView::SingleSelection);
  layerDock->setWidget(m_layerTreeView);
  addDockWidget(Qt::LeftDockWidgetArea, layerDock);

  tabifyDockWidget(moleculeDock, layerDock);

  // Switch to our fallback icons if there are no platform-specific icons.
  if (!QIcon::hasThemeIcon("document-new"))
    QIcon::setThemeName("fallback");

  QIcon icon(":/icons/avogadro.png");
  setWindowIcon(icon);

#ifndef Q_OS_MAC
  m_fileToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  m_fileToolBar->setWindowTitle(tr("File"));
  addToolBar(m_fileToolBar);
#else
  m_fileToolBar->hide();
#endif
  m_toolToolBar->setWindowTitle(tr("Tools"));
  addToolBar(m_toolToolBar);

  // Create the scene plugin model
  m_sceneTreeView->setAlternatingRowColors(true);
  m_sceneTreeView->header()->stretchLastSection();
  m_sceneTreeView->header()->setVisible(false);
  connect(m_sceneTreeView, &QAbstractItemView::activated, this,
          &MainWindow::sceneItemActivated);
  connect(m_sceneTreeView, &QAbstractItemView::clicked, this,
          &MainWindow::sceneItemActivated);

  // Create the molecule model
  m_moleculeModel = new QtGui::MoleculeModel(this);
  m_moleculeTreeView->setModel(m_moleculeModel);
  m_moleculeTreeView->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_moleculeTreeView->setAlternatingRowColors(true);
  m_moleculeTreeView->header()->setStretchLastSection(false);
  m_moleculeTreeView->header()->setVisible(false);
  m_moleculeTreeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_moleculeTreeView->header()->setSectionResizeMode(1, QHeaderView::Fixed);
  m_moleculeTreeView->header()->resizeSection(1, 30);
  connect(m_moleculeTreeView, &QAbstractItemView::activated, this,
          &MainWindow::moleculeActivated);
  connect(m_moleculeTreeView, &QAbstractItemView::clicked, this,
          &MainWindow::moleculeActivated);

  m_layerModel = new QtGui::LayerModel(this);
  m_layerTreeView->setModel(m_layerModel);
  m_layerTreeView->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_layerTreeView->setAlternatingRowColors(true);
  m_layerTreeView->header()->setStretchLastSection(false);
  m_layerTreeView->header()->setVisible(false);
  m_layerTreeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  for (int i = 1; i < m_layerModel->columnCount(QModelIndex()); ++i) {
    m_layerTreeView->header()->setSectionResizeMode(i, QHeaderView::Fixed);
    m_layerTreeView->header()->resizeSection(i, 25);
  }
  connect(m_layerTreeView, &QAbstractItemView::activated, this,
          &MainWindow::layerActivated);
  connect(m_layerTreeView, &QAbstractItemView::clicked, this,
          &MainWindow::layerActivated);
  connect(m_sceneTreeView, &QAbstractItemView::clicked, m_layerModel,
          &QtGui::LayerModel::updateRows);

  viewActivated(glWidget);
  buildTools();
  // Connect to the invalid context signal, check whether GL is initialized.
  // connect(m_glWidget, SIGNAL(rendererInvalid()), SLOT(rendererInvalid()));
  connect(m_multiViewWidget, &QtGui::MultiViewWidget::activeWidgetChanged, this,
          &MainWindow::viewActivated);
}

void MainWindow::closeEvent(QCloseEvent* e)
{
  writeSettings();
  if (!saveFileIfNeeded()) {
    e->ignore();
    return;
  }
  QMainWindow::closeEvent(e);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
  if (event->mimeData()->hasUrls())
    event->acceptProposedAction();
  else
    event->ignore();
}

void MainWindow::dropEvent(QDropEvent* event)
{
  if (event->mimeData()->hasUrls()) {
    // TODO: check for ZIP, TAR, PY scripts (plugins)
    foreach (const QUrl& url, event->mimeData()->urls()) {
      if (url.isLocalFile()) {
        QString fileName = url.toLocalFile();
        QFileInfo info(fileName);
        QString extension = info.completeSuffix(); // e.g. .tar.gz or .pdb.gz

        if (extension == "py")
          addScript(fileName);
        else
          openFile(fileName);
      }
    }
    event->acceptProposedAction();
  } else
    event->ignore();
}

void MainWindow::moleculeReady(int)
{
  ExtensionPlugin* extension = qobject_cast<ExtensionPlugin*>(sender());
  if (extension) {
    Molecule* mol = new Molecule(this);
    if (extension->readMolecule(*mol))
      setMolecule(mol);
  }
}

void MainWindow::newMolecule()
{
  setMolecule(new Molecule(this));
}

template<class T, class M>
void setWidgetMolecule(T* glWidget, M* mol)
{
  glWidget->setMolecule(mol);
  glWidget->updateScene();
  glWidget->resetCamera();
}

void setDefaultViews(MultiViewWidget* viewWidget)
{
  QSettings settings;
  // save the enabled scene / render plugins
  if (GLWidget* glWidget =
        qobject_cast<GLWidget*>(viewWidget->activeWidget())) {

    const ScenePluginModel* sceneModel = &glWidget->sceneModel();
    for (auto plugin : sceneModel->scenePlugins()) {
      QString settingsKey("MainWindow/" + plugin->objectName());
      bool enabled = settings.value(settingsKey, plugin->isEnabled()).toBool();
      plugin->setEnabled(enabled);
    }
  }
}
void MainWindow::setMolecule(Molecule* mol)
{
  if (!mol)
    return;

  // Set the new molecule, ensure both molecules are in the model.
  if (m_molecule && !m_moleculeModel->molecules().contains(m_molecule)) {
    m_moleculeModel->addItem(m_molecule);
  }
  if (!m_moleculeModel->molecules().contains(mol)) {
    m_moleculeModel->addItem(mol);
  }

  Molecule* oldMolecule(m_molecule);
  m_molecule = mol;

  // If the molecule is empty, make the editor active. Otherwise, use the
  // navigator tool.
  if (m_molecule) {
    QString targetToolName =
      m_molecule->atomCount() > 0 ? "Navigator" : "Editor";
    setActiveTool(targetToolName);
    connect(m_molecule, &QtGui::Molecule::changed, this,
            &MainWindow::markMoleculeDirty);
  }

  emit moleculeChanged(m_molecule);
  markMoleculeClean();
  updateWindowTitle();
  m_moleculeModel->setActiveMolecule(m_molecule);
  m_layerModel->addMolecule(m_molecule);
  // only set the default values if there is nothing setup
  // 1 layer (layerCount) and 1º layer + add items
  if (m_layerModel->layerCount() == 1 && m_layerModel->items() == 2) {
    setDefaultViews(m_multiViewWidget);
    refreshDisplayTypes();
  }

  ActiveObjects::instance().setActiveMolecule(m_molecule);

  if (oldMolecule)
    oldMolecule->disconnect(this);

  // Check if the molecule needs to update the current one.
  QWidget* w = m_multiViewWidget->activeWidget();
  if (GLWidget* glWidget = qobject_cast<QtOpenGL::GLWidget*>(w)) {
    setWidgetMolecule(glWidget, mol);
  }
#ifdef AVO_USE_VTK
  else if (vtkGLWidget* vtkWidget = qobject_cast<vtkGLWidget*>(w)) {
    setWidgetMolecule(vtkWidget, mol);
  }
#endif
}

void MainWindow::markMoleculeDirty()
{
  activeMoleculeEdited();
  if (!m_moleculeDirty) {
    m_moleculeDirty = true;
    updateWindowTitle();
  }
}

void MainWindow::refreshDisplayTypes()
{
  m_layerModel->updateRows();
  // force m_sceneTreeView update (update doesn't update the checkbox)
  m_sceneTreeView->setFocus();
  if (m_activeScenePlugin != nullptr) {
    m_viewDock->setWidget(m_activeScenePlugin->setupWidget());
  }
  // reset focus to m_layerTreeView
  m_layerTreeView->setFocus();
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

  setWindowTitle(tr("%1%2 - Avogadro %3",
                    "window title: %1 = file name, %2 = • for "
                    "modified file, %3 = Avogadro version")
                   .arg(QFileInfo(fileName).fileName())
                   .arg(m_moleculeDirty ? "•" : "")
                   .arg(AvogadroApp_VERSION));
}

#ifdef QTTESTING
void MainWindow::playTest(const QString& fileName, bool exit)
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
  settings.setValue("perspective", m_viewPerspective->isChecked());
  settings.endGroup();
  settings.setValue("recentFiles", m_recentFiles);

  // save the enabled scene / render plugins
  if (GLWidget* glWidget =
        qobject_cast<GLWidget*>(m_multiViewWidget->activeWidget())) {

    const ScenePluginModel* sceneModel = &glWidget->sceneModel();
    for (auto plugin : sceneModel->scenePlugins()) {
      QString settingsKey("MainWindow/" + plugin->objectName());
      settings.setValue(settingsKey, plugin->isEnabled());
    }
  }
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

  QString fileName =
    QFileDialog::getOpenFileName(this, tr("Open chemical file"), dir, filter);

  if (fileName.isEmpty()) // user cancel
    return;

  QFileInfo info(fileName);
  dir = info.absoluteDir().absolutePath();
  settings.setValue("MainWindow/lastOpenDir", dir);

  // Create one of our readers to read the file:
  QString extension = info.suffix().toLower();
  FileFormat* reader = nullptr;
  if (extension == "cml")
    reader = new Io::CmlFormat;
  else if (extension == "cjson")
    reader = new Io::CjsonFormat;

  if (!openFile(fileName, reader)) {
    MESSAGEBOX::information(this, tr("Cannot open file"),
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
    QtGui::FileFormatDialog::fileToRead(this, tr("Open Molecule"), dir);

  if (reply.first == nullptr) // user cancel
    return;

  dir = QFileInfo(reply.second).absoluteDir().absolutePath();
  settings.setValue("MainWindow/lastOpenDir", dir);

  if (!openFile(reply.second, reply.first->newInstance())) {
    MESSAGEBOX::information(
      this, tr("Cannot open file"),
      tr("Can't open supplied file %1").arg(reply.second));
  }
}

bool MainWindow::addScript(const QString& filePath)
{
  if (filePath.isEmpty()) {
    return false;
  }

  // Ask the user what type of script this is
  // TODO: add some sort of warning?
  QStringList types;
  types << tr("Commands") << tr("Input Generators") << tr("File Formats");

  bool ok;
  QString item =
    QInputDialog::getItem(this, tr("Install Plugin Script"), tr("Script Type:"),
                          types, 0, false, &ok);

  if (!ok || item.isEmpty())
    return false;

  QString typePath;

  int index = types.indexOf(item);
  // don't translate these
  switch (index) {
    case 0: // commands
      typePath = "commands";
      break;
    case 1:
      typePath = "inputGenerators";
      break;
    case 2:
      typePath = "formatScripts";
      break;
    default:
      typePath = "other";
  }

  QStringList stdPaths =
    QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation);

  QFileInfo info(filePath);

  QString destinationPath(stdPaths[0] + '/' + typePath + '/' + info.fileName());
  qDebug() << " copying " << filePath << " to " << destinationPath;
  QFile::remove(destinationPath); // silently fail if there's nothing to remove
  QFile::copy(filePath, destinationPath);

  // TODO: Ask that type of plugin script to reload?

  return true;
}

bool MainWindow::openFile(const QString& fileName, Io::FileFormat* reader)
{
  if (fileName.isEmpty()) {
    delete reader;
    return false;
  }

  if (reader == nullptr) {
    const Io::FileFormat* format = QtGui::FileFormatDialog::findFileFormat(
      this, tr("Select file reader"), fileName,
      FileFormat::File | FileFormat::Read, "Avogadro:");
    if (format)
      reader = format->newInstance();
  }
  if (!reader)
    return false;

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
  m_fileReadMolecule->setData("fileName", fileName.toLocal8Bit().data());
  m_threadedReader->moveToThread(m_fileReadThread);
  m_threadedReader->setMolecule(m_fileReadMolecule);
  m_threadedReader->setFileName(fileName);

  // Setup a progress dialog in case file loading is slow
  m_progressDialog = new QProgressDialog(this);
  m_progressDialog->setRange(0, 0);
  m_progressDialog->setValue(0);
  m_progressDialog->setMinimumDuration(750);
  m_progressDialog->setWindowTitle(tr("Reading File"));
  m_progressDialog->setLabelText(
    tr("Opening file '%1'\nwith '%2'").arg(fileName).arg(ident));
  /// @todo Add API to abort file ops
  m_progressDialog->setCancelButton(nullptr);
  connect(m_fileReadThread, &QThread::started, m_threadedReader,
          &BackgroundFileFormat::read);
  connect(m_threadedReader, &BackgroundFileFormat::finished, m_fileReadThread,
          &QThread::quit);
  connect(m_threadedReader, &BackgroundFileFormat::finished, this,
          &MainWindow::backgroundReaderFinished);

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
  } else if (m_threadedReader->success()) {
    if (!fileName.isEmpty()) {
      m_fileReadMolecule->setData("fileName", fileName.toLocal8Bit().data());
      m_recentFiles.prepend(fileName);
      updateRecentFiles();
    } else {
      m_fileReadMolecule->setData("fileName", Core::Variant());
    }
    setMolecule(m_fileReadMolecule);
    statusBar()->showMessage(tr("Molecule loaded (%1 atoms, %2 bonds)")
                               .arg(m_molecule->atomCount())
                               .arg(m_molecule->bondCount()),
                             5000);
  } else {
    MESSAGEBOX::critical(this, tr("File error"),
                         tr("Error while reading file '%1':\n%2")
                           .arg(fileName)
                           .arg(m_threadedReader->error()));
    delete m_fileReadMolecule;
  }
  m_fileReadThread->deleteLater();
  m_fileReadThread = nullptr;
  m_threadedReader->deleteLater();
  m_threadedReader = nullptr;
  m_fileReadMolecule = nullptr;
  m_progressDialog->hide();
  m_progressDialog->deleteLater();
  m_progressDialog = nullptr;

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
      statusBar()->showMessage(
        tr("Saved file %1", "%1 = filename").arg(fileName));
      m_threadedWriter->molecule()->setData("fileName",
                                            fileName.toLocal8Bit().data());
      markMoleculeClean();
      updateWindowTitle();
      success = true;
    } else {
      MESSAGEBOX::critical(
        this, tr("Error saving file"),
        tr("Error while saving '%1':\n%2", "%1 = file name, %2 = error message")
          .arg(fileName)
          .arg(m_threadedWriter->error()));
    }
  }
  m_fileWriteThread->deleteLater();
  m_fileWriteThread = nullptr;
  m_threadedWriter->deleteLater();
  m_threadedWriter = nullptr;
  m_progressDialog->deleteLater();
  m_progressDialog = nullptr;
  return success;
}

void MainWindow::toolActivated()
{
  if (QAction* action = qobject_cast<QAction*>(sender())) {
    if (GLWidget* glWidget =
          qobject_cast<GLWidget*>(m_multiViewWidget->activeWidget())) {
      glWidget->setActiveTool(action->data().toString());
      if (glWidget->activeTool()) {
        m_toolDock->setWidget(glWidget->activeTool()->toolWidget());
        m_toolDock->setWindowTitle(action->text());

        // uncheck the toolbar
        foreach (QAction* barAction, m_toolToolBar->actions()) {
          if (action->data().toString() != barAction->data().toString())
            barAction->setChecked(false);
        }
      }
    }
  }
}

void MainWindow::viewConfigActivated() {}

void MainWindow::rendererInvalid()
{
  GLWidget* widget = qobject_cast<GLWidget*>(sender());
  MESSAGEBOX::warning(this, tr("Error: Failed to initialize OpenGL context"),
                      tr("OpenGL 2.0 or greater required, exiting.\n\n%1")
                        .arg(widget ? widget->error() : tr("Unknown error")));
  // Process events, and then set a single shot timer. This is needed to ensure
  // the RPC server also exits cleanly.
  QApplication::processEvents();
  QTimer::singleShot(500, this, &QWidget::close);
}

void MainWindow::layerActivated(const QModelIndex& idx)
{
  m_layerModel->updateRows();
  if (idx.row() == m_layerModel->items() - 1) {
    m_layerModel->addLayer(m_molecule->undoMolecule());
  } else {
    bool updateGL = false;
    if (idx.column() == QtGui::LayerModel::ColumnType::Name) {
      m_layerModel->setActiveLayer(idx.row(), m_molecule->undoMolecule());
    } else if (idx.column() == QtGui::LayerModel::ColumnType::Remove) {
      if (m_layerModel->layerCount() > 1) {
        m_layerModel->removeItem(idx.row(), m_molecule->undoMolecule());
        updateGL = true;
      }
    } else if (idx.column() == QtGui::LayerModel::ColumnType::Lock) {
      m_layerModel->flipLocked(idx.row());
    } else if (idx.column() == QtGui::LayerModel::ColumnType::Visible) {
      m_layerModel->flipVisible(idx.row());
      updateGL = true;
    } else if (idx.column() == QtGui::LayerModel::ColumnType::Menu) {
      m_layerModel->setActiveLayer(idx.row(), m_molecule->undoMolecule());
      if (m_sceneDock->isHidden()) {
        m_sceneDock->show();
        m_viewDock->show();
        resizeDocks({ m_sceneDock, m_viewDock }, { 250, 50 }, Qt::Vertical);
      } else {
        m_sceneDock->hide();
        m_viewDock->hide();
      }
    }

    if (updateGL) {
      QWidget* w = m_multiViewWidget->activeWidget();
      if (GLWidget* glWidget = qobject_cast<QtOpenGL::GLWidget*>(w)) {
        glWidget->updateScene();
      }
#ifdef AVO_USE_VTK
      else if (vtkGLWidget* vtkWidget = qobject_cast<vtkGLWidget*>(w)) {
        vtkWidget->updateScene();
      }
#endif
    }
  }
  refreshDisplayTypes();
}

void MainWindow::moleculeActivated(const QModelIndex& idx)
{
  QObject* obj = static_cast<QObject*>(idx.internalPointer());
  if (Molecule* mol = qobject_cast<Molecule*>(obj)) {
    if (idx.column() == 0)
      setMolecule(mol);

    // Deleting a molecule, we must also create a new one if it is the last.
    if (idx.column() == 1) {
      if (m_molecule == mol) {
        QList<Molecule*> molecules = m_moleculeModel->molecules();
        int molIdx = molecules.indexOf(mol);
        if (molIdx > 0)
          setMolecule(molecules[molIdx - 1]);
        else if (molIdx == 0 && molecules.size() > 1) {
          setMolecule(molecules[1]);
        } else {
          newMolecule();
        }
      }
      m_moleculeModel->removeItem(mol);
    }
  }
}

void MainWindow::sceneItemActivated(const QModelIndex& idx)
{
  if (!idx.isValid())
    return;
  QObject* obj = static_cast<QObject*>(idx.internalPointer());
  if (ScenePlugin* scene = qobject_cast<ScenePlugin*>(obj)) {
    m_viewDock->setWidget(scene->setupWidget());
    m_activeScenePlugin = scene;
  }
}

bool populatePluginModel(ScenePluginModel& model, bool editOnly = false)
{
  if (!model.scenePlugins().empty())
    return false;

  QSettings settings;
  PluginManager* plugin = PluginManager::instance();
  QList<ScenePluginFactory*> scenePluginFactories =
    plugin->pluginFactories<ScenePluginFactory>();
  foreach (ScenePluginFactory* factory, scenePluginFactories) {
    ScenePlugin* scenePlugin = factory->createInstance();
    if (editOnly && scenePlugin) {
      if (scenePlugin->objectName() == "BallStick") {
        model.addItem(scenePlugin);
      } else {
        delete scenePlugin;
      }
    } else if (scenePlugin) {
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
  PluginManager* plugin = PluginManager::instance();
  QList<ToolPluginFactory*> toolPluginFactories =
    plugin->pluginFactories<ToolPluginFactory>();
  foreach (ToolPluginFactory* factory, toolPluginFactories) {
    ToolPlugin* tool = factory->createInstance();
    if (tool)
      glWidget->addTool(tool);
  }
  glWidget->setDefaultTool(QObject::tr("Navigate tool"));
  glWidget->setActiveTool(QObject::tr("Navigate tool"));
  return true;
}

void MainWindow::viewActivated(QWidget* widget)
{
  ActiveObjects::instance().setActiveWidget(widget);
  if (GLWidget* glWidget = qobject_cast<GLWidget*>(widget)) {
    bool firstRun = populatePluginModel(glWidget->sceneModel());
    m_sceneTreeView->setModel(&glWidget->sceneModel());
    populateTools(glWidget);

    foreach (ExtensionPlugin* extension, m_extensions) {
      extension->setScene(&glWidget->renderer().scene());
      extension->setCamera(&glWidget->renderer().camera());
      extension->setActiveWidget(glWidget);
    }

    if (firstRun) {
      setActiveTool("Navigator");
      glWidget->updateScene();
    } else {
      m_moleculeModel->setActiveMolecule(glWidget->molecule());
      m_layerModel->addMolecule(m_molecule);
      // Figure out the active tool - reflect this in the toolbar.
      ToolPlugin* tool = glWidget->activeTool();
      if (tool) {
        QString name = tool->objectName();
        foreach (QAction* action, m_toolToolBar->actions()) {
          action->setChecked(action->data().toString() == name);
        }
      }
    }
    if (m_molecule != glWidget->molecule() && glWidget->molecule()) {
      m_rwMolecule = 0;
      m_molecule = glWidget->molecule();
      emit moleculeChanged(m_molecule);
      m_moleculeModel->setActiveMolecule(m_molecule);
      m_layerModel->addMolecule(m_molecule);
    }
    ActiveObjects::instance().setActiveGLWidget(glWidget);
  }
#ifdef AVO_USE_VTK
  else if (vtkGLWidget* vtkWidget = qobject_cast<vtkGLWidget*>(widget)) {
    bool firstRun = populatePluginModel(vtkWidget->sceneModel());
    m_sceneTreeView->setModel(&vtkWidget->sceneModel());

    if (firstRun) {
      setActiveTool("Navigator");
      vtkWidget->updateScene();
    } else {
      m_moleculeModel->setActiveMolecule(vtkWidget->molecule());
      m_layerModel->addMolecule(m_molecule);
    }
    if (m_molecule != vtkWidget->molecule() && vtkWidget->molecule()) {
      m_rwMolecule = nullptr;
      m_molecule = vtkWidget->molecule();
      emit moleculeChanged(m_molecule);
      m_moleculeModel->setActiveMolecule(m_molecule);
      m_layerModel->addMolecule(m_molecule);
    }
  }
#endif
  updateWindowTitle();
  activeMoleculeEdited();
}

void MainWindow::exportGraphics()
{
  QOpenGLWidget* glWidget =
    qobject_cast<QOpenGLWidget*>(m_multiViewWidget->activeWidget());
  QStringList filters;
// Omit "common image formats" on Mac
#ifdef Q_OS_MAC
  filters
#else
  filters << tr("Common image formats") + " (*.png *.jpg *.jpeg)"
#endif
    << tr("All files") + " (* *.*)" << tr("BMP") + " (*.bmp)"
    << tr("PNG") + " (*.png)" << tr("JPEG") + " (*.jpg *.jpeg)";

  // Use QFileInfo to get the parts of the path we want
  QString baseFileName;
  if (m_molecule)
    baseFileName = m_molecule->data("fileName").toString().c_str();
  QFileInfo info(baseFileName);

  QString fileName = QFileDialog::getSaveFileName(
    this, tr("Export Bitmap Graphics"), "", "PNG (*.png)");

  if (fileName.isEmpty())
    return;
  if (QFileInfo(fileName).suffix().isEmpty())
    fileName += ".png";

  // render it (with alpha channel)
  Rendering::Scene* scene(nullptr);
  GLWidget* viewWidget(nullptr);
  if ((viewWidget =
         qobject_cast<GLWidget*>(m_multiViewWidget->activeWidget()))) {
    scene = &viewWidget->renderer().scene();
  }
  Vector4ub cColor = scene->backgroundColor();
  unsigned char alpha = cColor[3];
  cColor[3] = 0; // 100% transparent for export
  scene->setBackgroundColor(cColor);

  QImage exportImage;
  glWidget->raise();
  glWidget->repaint();
  if (QOpenGLFramebufferObject::hasOpenGLFramebufferObjects()) {
    exportImage = glWidget->grabFramebuffer();
  } else {
    QPixmap pixmap = QPixmap::grabWindow(glWidget->winId());
    exportImage = pixmap.toImage();
  }

  // Now we embed molecular information into the file, if possible
  if (m_molecule && m_molecule->atomCount() < 1000) {
    string tmpCml;
    bool ok =
      FileFormatManager::instance().writeString(*m_molecule, tmpCml, "cml");
    if (ok)
      exportImage.setText("CML", tmpCml.c_str());
  }

  if (!exportImage.save(fileName)) {
    MESSAGEBOX::warning(this, tr("Avogadro"),
                        tr("Cannot save file %1.").arg(fileName));
  }

  // set the GL widget back to the right background color (i.e., not 100%
  // transparent)
  cColor[3] = alpha; // previous color
  scene->setBackgroundColor(cColor);
  glWidget->repaint();
}

void MainWindow::reassignCustomElements()
{
  if (m_molecule && m_molecule->hasCustomElements())
    CustomElementDialog::resolve(this, *m_molecule);
}

void MainWindow::openRecentFile()
{
  if (!saveFileIfNeeded())
    return;

  QAction* action = qobject_cast<QAction*>(sender());
  if (action) {
    QString fileName = action->data().toString();

    const FileFormat* format =
      FileFormatDialog::findFileFormat(this, tr("Select file reader"), fileName,
                                       FileFormat::File | FileFormat::Read);

    if (!openFile(fileName, format ? format->newInstance() : nullptr)) {
      MESSAGEBOX::information(this, tr("Cannot open file"),
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
  foreach (const QString& file, m_recentFiles) {
    QFileInfo fileInfo(file);
    QAction* recentFile = m_actionRecentFiles[i++];
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
  QObject* molObj = m_moleculeModel->activeMolecule();

  if (!molObj)
    return false;

  Molecule* mol = qobject_cast<Molecule*>(molObj);
  if (!mol) {
    RWMolecule* rwMol = qobject_cast<RWMolecule*>(molObj);
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
  } else if (extension == QLatin1String("cjson")) {
    return saveFileAs(QString::fromStdString(fileName), new Io::CjsonFormat,
                      async);
  }

  // is the imported format writable?
  bool writable =
    !FileFormatManager::instance()
       .fileFormatsFromFileExtension(extension.toStdString(),
                                     FileFormat::File | FileFormat::Read)
       .empty();
  if (writable) {
    // Warn the user that the format may lose data.
    MESSAGEBOX box(this);
    box.setModal(true);
    box.setWindowTitle(tr("Avogadro"));
    box.setText(tr("This file was imported from a non-standard format which "
                   "may not be able to write all of the information in the "
                   "molecule.\n\nWould you like to export to the current "
                   "format, or save in a standard format?"));
    QPushButton* saveButton(box.addButton(QMessageBox::Save));
    QPushButton* cancelButton(box.addButton(QMessageBox::Cancel));
    QPushButton* exportButton(
      box.addButton(tr("Export"), QMessageBox::DestructiveRole));
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
  QString filter(QString("%1 (*.cjson);;%2 (*.cml)")
                   .arg(tr("Chemical JSON"))
                   .arg(tr("Chemical Markup Language")));

  QSettings settings;
  QString dir = settings.value("MainWindow/lastSaveDir").toString();

  QString fileName =
    QFileDialog::getSaveFileName(this, tr("Save chemical file"), dir, filter);

  if (fileName.isEmpty()) // user cancel
    return false;

  QFileInfo info(fileName);
  dir = info.absoluteDir().absolutePath();
  settings.setValue("MainWindow/lastSaveDir", dir);

  // Create one of our writers to save the file:
  QString extension = info.suffix().toLower();
  FileFormat* writer = nullptr;
  if (extension == "cjson" || extension.isEmpty())
    writer = new Io::CjsonFormat;
  else if (extension == "cml")
    writer = new Io::CmlFormat;
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

  if (reply.first == nullptr) // user cancel
    return false;

  dir = QFileInfo(reply.second).absoluteDir().absolutePath();
  settings.setValue("MainWindow/lastSaveDir", dir);

  return saveFileAs(reply.second, reply.first->newInstance(), async);
}

bool MainWindow::saveFileAs(const QString& fileName, Io::FileFormat* writer,
                            bool async)
{
  if (fileName.isEmpty() || writer == nullptr) {
    delete writer;
    return false;
  }

  QString ident = QString::fromStdString(writer->identifier());

  // Figure out what molecule willl be saved, perform conversion if necessary.
  QObject* molObj = m_moleculeModel->activeMolecule();

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

  Molecule* mol = qobject_cast<Molecule*>(molObj);
  if (!mol) {
    delete writer;
    return false;
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
  m_progressDialog->setWindowTitle(tr("Saving File in Progress…"));
  m_progressDialog->setLabelText(
    tr("Saving file “%1”\nwith “%2”", "%1 = file name, %2 = format")
      .arg(fileName)
      .arg(ident));
  /// @todo Add API to abort file ops
  m_progressDialog->setCancelButton(nullptr);
  connect(m_fileWriteThread, &QThread::started, m_threadedWriter,
          &BackgroundFileFormat::write);
  connect(m_threadedWriter, &BackgroundFileFormat::finished, m_fileWriteThread,
          &QThread::quit);

  // Start the file operation
  m_progressDialog->show();
  if (async) {
    connect(m_threadedWriter, &BackgroundFileFormat::finished, this,
            &MainWindow::backgroundWriterFinished);
    m_fileWriteThread->start();
    return true;
  } else {
    QTimer::singleShot(0, m_fileWriteThread, SLOT(start()));
    QEventLoop loop;
    connect(m_fileWriteThread, &QThread::finished, &loop, &QEventLoop::quit);
    loop.exec();
    return backgroundWriterFinished();
  }
}

void MainWindow::setActiveTool(QString toolName)
{
  if (GLWidget* glWidget =
        qobject_cast<GLWidget*>(m_multiViewWidget->activeWidget())) {
    foreach (ToolPlugin* toolPlugin, glWidget->tools()) {
      if (toolPlugin->objectName() == toolName) {
        toolPlugin->activateAction()->triggered();
        glWidget->setActiveTool(toolPlugin);

        // update the settings widget
        m_toolDock->setWidget(toolPlugin->toolWidget());
        m_toolDock->setWindowTitle(toolPlugin->activateAction()->text());
      }
    }
  }

  if (!toolName.isEmpty()) {
    // check view tools
    foreach (QAction* action, m_toolToolBar->actions()) {
      if (action->data().toString() == toolName)
        action->setChecked(true);
      else
        action->setChecked(false);
    }
  }
}

void MainWindow::setActiveDisplayTypes(QStringList displayTypes)
{
  ScenePluginModel* scenePluginModel(nullptr);
  GLWidget* glWidget(nullptr);
#ifdef AVO_USE_VTK
  VTK::vtkGLWidget* vtkWidget(nullptr);
#endif
  if ((glWidget = qobject_cast<GLWidget*>(m_multiViewWidget->activeWidget()))) {
    scenePluginModel = &glWidget->sceneModel();
  }
#ifdef AVO_USE_VTK
  else if ((vtkWidget = qobject_cast<VTK::vtkGLWidget*>(
              m_multiViewWidget->activeWidget()))) {
    scenePluginModel = &vtkWidget->sceneModel();
  }
#endif

  foreach (ScenePlugin* scene, scenePluginModel->scenePlugins())
    scene->setEnabled(false);
  foreach (ScenePlugin* scene, scenePluginModel->scenePlugins())
    foreach (const QString& name, displayTypes)
      if (scene->objectName() == name)
        scene->setEnabled(true);
  if (glWidget)
    glWidget->updateScene();
#ifdef AVO_USE_VTK
  else if (vtkWidget)
    vtkWidget->updateScene();
#endif
}

void MainWindow::undoEdit()
{
  if (m_molecule) {
    m_molecule->undoMolecule()->undoStack().undo();
    m_molecule->emitChanged(Molecule::Atoms | Molecule::Added);
    m_layerModel->updateRows();
    activeMoleculeEdited();
  }
}

void MainWindow::redoEdit()
{
  if (m_molecule) {
    m_molecule->undoMolecule()->undoStack().redo();
    m_molecule->emitChanged(Molecule::Atoms | Molecule::Added);
    m_layerModel->updateRows();
    activeMoleculeEdited();
  }
}

void MainWindow::activeMoleculeEdited()
{
  if (!m_undo || !m_redo)
    return;
  if (m_molecule) {
    if (m_molecule->undoMolecule()->undoStack().canUndo()) {
      m_undo->setEnabled(true);
      m_undo->setText(
        tr("&Undo %1").arg(m_molecule->undoMolecule()->undoStack().undoText()));
    } else {
      m_undo->setEnabled(false);
      m_undo->setText(tr("&Undo"));
    }
    if (m_molecule->undoMolecule()->undoStack().canRedo()) {
      m_redo->setEnabled(true);
      m_redo->setText(
        tr("&Redo %1").arg(m_molecule->undoMolecule()->undoStack().redoText()));
    } else {
      m_redo->setEnabled(false);
      m_redo->setText(tr("&Redo"));
    }
  } else {
    m_undo->setEnabled(false);
    m_redo->setEnabled(false);
  }
}

void MainWindow::setBackgroundColor()
{
  Rendering::Scene* scene(nullptr);
  GLWidget* glWidget(nullptr);
  if ((glWidget = qobject_cast<GLWidget*>(m_multiViewWidget->activeWidget())))
    scene = &glWidget->renderer().scene();
  if (scene) {
    Vector4ub cColor = scene->backgroundColor();
    QColor qtColor(cColor[0], cColor[1], cColor[2], cColor[3]);
    QColor color = QColorDialog::getColor(qtColor, this);
    if (color.isValid()) {
      cColor[0] = static_cast<unsigned char>(color.red());
      cColor[1] = static_cast<unsigned char>(color.green());
      cColor[2] = static_cast<unsigned char>(color.blue());
      cColor[3] = static_cast<unsigned char>(color.alpha());
      scene->setBackgroundColor(cColor);
      if (glWidget)
        glWidget->update();

      QSettings settings;
      settings.setValue("backgroundColor", color);
    }
  }
}

void MainWindow::setProjectionPerspective()
{
  Rendering::GLRenderer* renderer(nullptr);
  GLWidget* glWidget(nullptr);
  if ((glWidget = qobject_cast<GLWidget*>(m_multiViewWidget->activeWidget())))
    renderer = &glWidget->renderer();
  if (renderer) {
    renderer->camera().setProjectionType(Rendering::Perspective);
    if (glWidget)
      glWidget->update();
  }
}

void MainWindow::setProjectionOrthographic()
{
  Rendering::GLRenderer* renderer(nullptr);
  GLWidget* glWidget(nullptr);
  if ((glWidget = qobject_cast<GLWidget*>(m_multiViewWidget->activeWidget())))
    renderer = &glWidget->renderer();
  if (renderer) {
    renderer->camera().setProjectionType(Rendering::Orthographic);
    if (glWidget)
      glWidget->update();
  }
}

#ifdef QTTESTING
void MainWindow::record()
{
  QString fileName = QFileDialog::getSaveFileName(
    this, "Test file name", QString(), "XML Files (*.xml)");
  if (!fileName.isEmpty())
    m_testUtility->recordTests(fileName);
}

void MainWindow::play()
{
  QString fileName = QFileDialog::getOpenFileName(
    this, "Test file name", QString(), "XML Files (*.xml)");
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
  QAction* actionRecord = new QAction(this);
  actionRecord->setText(tr("Record test…"));
  m_menuBuilder->addAction(testingPath, actionRecord, 10);
  QAction* actionPlay = new QAction(this);
  actionPlay->setText(tr("Play test…"));
  m_menuBuilder->addAction(testingPath, actionPlay, 5);

  connect(actionRecord, SIGNAL(triggered()), SLOT(record()));
  connect(actionPlay, SIGNAL(triggered()), SLOT(play()));

  m_testUtility = new pqTestUtility(this);
  m_testUtility->addEventObserver("xml", new XMLEventObserver(this));
  m_testUtility->addEventSource("xml", new XMLEventSource(this));

  m_testExit = true;
#endif

  QStringList path;
  path << "&File";
  // New
  QAction* action = new QAction(tr("&New"), this);
  action->setShortcut(QKeySequence::New);
#ifndef Q_OS_MAC
  action->setIcon(QIcon::fromTheme("document-new"));
#endif
  m_menuBuilder->addAction(path, action, 999);
  m_fileToolBar->addAction(action);
  connect(action, &QAction::triggered, this, &MainWindow::newMolecule);
  // Open
  action = new QAction(tr("&Open…"), this);
  action->setShortcut(QKeySequence::Open);
#ifndef Q_OS_MAC
  action->setIcon(QIcon::fromTheme("document-open"));
#endif
  m_menuBuilder->addAction(path, action, 998);
  m_fileToolBar->addAction(action);
  connect(action, &QAction::triggered, this, &MainWindow::importFile);

  action = new QAction(tr("&Close"), this);
  action->setShortcut(QKeySequence::Close);
#ifndef Q_OS_MAC
  action->setIcon(QIcon::fromTheme("document-close"));
#endif
  m_menuBuilder->addAction(path, action, 981);
  m_fileToolBar->addAction(action);
  connect(action, &QAction::triggered, this, &QWidget::close);

  // Separator (after open recent)
  action = new QAction("", this);
  action->setSeparator(true);
  m_menuBuilder->addAction(path, action, 980);
  // Save
  action = new QAction(tr("&Save"), this);
  action->setShortcut(QKeySequence::Save);
#ifndef Q_OS_MAC
  action->setIcon(QIcon::fromTheme("document-save"));
#endif
  m_menuBuilder->addAction(path, action, 965);
  m_fileToolBar->addAction(action);
  connect(action, &QAction::triggered, this, &MainWindow::saveFile);
  // Save As
  action = new QAction(tr("Save &As…"), this);
  action->setShortcut(QKeySequence::SaveAs);
#ifndef Q_OS_MAC
  action->setIcon(QIcon::fromTheme("document-save-as"));
#endif
  m_menuBuilder->addAction(path, action, 960);
  m_fileToolBar->addAction(action);
  connect(action, SIGNAL(triggered()), SLOT(saveFileAs()));

  // Export
  QStringList exportPath = path;
  exportPath << tr("&Export");
  action = new QAction(tr("&Molecule…"), this);
  m_menuBuilder->addAction(exportPath, action, 110);
  m_fileToolBar->addAction(action);
#ifndef Q_OS_MAC
  action->setIcon(QIcon::fromTheme("document-export"));
#endif
  connect(action, &QAction::triggered, this, &MainWindow::exportFile);
  // Export graphics
  action = new QAction(tr("&Graphics…"), this);
  m_menuBuilder->addAction(exportPath, action, 100);
#ifndef Q_OS_MAC
  action->setIcon(QIcon::fromTheme("document-export"));
#endif
  connect(action, &QAction::triggered, this, &MainWindow::exportGraphics);

  // Quit
  action = new QAction(tr("&Quit"), this);
  action->setShortcut(QKeySequence::Quit);
#ifndef Q_OS_MAC
  action->setIcon(QIcon::fromTheme("application-exit"));
#endif
  m_menuBuilder->addAction(path, action, -200);
  connect(action, &QAction::triggered, this, &QWidget::close);

  // open recent 995 - 985
  // Populate the recent file actions list.
  path << "Open Recent";
  // TODO: Check if files exist and if we actually have 10 items
  for (int i = 0; i < 10; ++i) {
    action = new QAction(QString::number(i), this);
    m_actionRecentFiles.push_back(action);
#ifndef Q_OS_MAC
    action->setIcon(QIcon::fromTheme("document-open-recent"));
#endif
    action->setVisible(false);
    m_menuBuilder->addAction(path, action, 995 - i);
    connect(action, &QAction::triggered, this, &MainWindow::openRecentFile);
  }
  m_actionRecentFiles[0]->setText(tr("No recent files"));
  m_actionRecentFiles[0]->setVisible(true);
  m_actionRecentFiles[0]->setEnabled(false);

  // Undo/redo
  QStringList editPath;
  editPath << tr("&Edit");
  m_undo = new QAction(tr("&Undo"), this);
#ifndef Q_OS_MAC
  m_undo->setIcon(QIcon::fromTheme("edit-undo"));
#endif
  m_undo->setShortcut(QKeySequence::Undo);
  m_redo = new QAction(tr("&Redo"), this);
#ifndef Q_OS_MAC
  m_redo->setIcon(QIcon::fromTheme("edit-redo"));
#endif
  m_redo->setShortcut(QKeySequence::Redo);
  m_undo->setEnabled(false);
  m_redo->setEnabled(false);
  connect(m_undo, &QAction::triggered, this, &MainWindow::undoEdit);
  connect(m_redo, &QAction::triggered, this, &MainWindow::redoEdit);
  m_menuBuilder->addAction(editPath, m_undo, 1);
  m_menuBuilder->addAction(editPath, m_redo, 0);

  // View menu
  QStringList viewPath;
  viewPath << tr("&View");
  action = new QAction(tr("Set Background Color…"), this);
  m_menuBuilder->addAction(viewPath, action, 100);
  connect(action, &QAction::triggered, this, &MainWindow::setBackgroundColor);

  // set default projection
  QSettings settings;
  bool perspective = settings.value("MainWindow/perspective", true).toBool();

  viewPath << tr("Projection");
  m_viewPerspective = new QAction(tr("Perspective"), this);
  m_viewPerspective->setCheckable(true);
  m_viewPerspective->setChecked(perspective);
  m_menuBuilder->addAction(viewPath, m_viewPerspective, 10);
  connect(m_viewPerspective, &QAction::triggered, this,
          &MainWindow::setProjectionPerspective);

  m_viewOrthographic = new QAction(tr("Orthographic"), this);
  m_viewOrthographic->setCheckable(true);
  m_viewOrthographic->setChecked(!perspective);
  m_menuBuilder->addAction(viewPath, m_viewOrthographic, 10);
  connect(m_viewOrthographic, &QAction::triggered, this,
          &MainWindow::setProjectionOrthographic);

  connect(m_viewPerspective, &QAction::triggered, m_viewOrthographic,
          &QAction::toggle);
  connect(m_viewOrthographic, &QAction::triggered, m_viewPerspective,
          &QAction::toggle);

  if (perspective)
    setProjectionPerspective();
  else
    setProjectionOrthographic();

  // Periodic table.
  QStringList extensionsPath;
  extensionsPath << tr("&Extensions");
  action = new QAction("&Periodic Table…", this);
  m_menuBuilder->addAction(extensionsPath, action, 0);
  QtGui::PeriodicTableView* periodicTable = new QtGui::PeriodicTableView(this);
  connect(action, &QAction::triggered, periodicTable, &QWidget::show);

  QStringList helpPath;
  helpPath << tr("&Help");
  QAction* about = new QAction("&About", this);
#ifndef Q_OS_MAC
  about->setIcon(QIcon::fromTheme("help-about"));
#endif
  m_menuBuilder->addAction(helpPath, about, 20);
  connect(about, &QAction::triggered, this, &MainWindow::showAboutDialog);

  // Now actually add all menu entries.
  m_menuBuilder->buildMenuBar(menuBar());
}

void MainWindow::buildMenu(QtGui::ExtensionPlugin* extension)
{
  foreach (QAction* action, extension->actions())
    m_menuBuilder->addAction(extension->menuPath(action), action);
}

// TODO: this would be a lovely C++11 lambda
bool ToolSort(const ToolPlugin* a, const ToolPlugin* b)
{
  return a->priority() < b->priority();
}

void MainWindow::buildTools()
{
  PluginManager* plugin = PluginManager::instance();

  // Now the tool plugins need to be built/added.
  QList<ToolPluginFactory*> toolPluginFactories =
    plugin->pluginFactories<ToolPluginFactory>();
  foreach (ToolPluginFactory* factory, toolPluginFactories) {
    ToolPlugin* tool = factory->createInstance();
    tool->setParent(this);
    if (tool)
      m_tools << tool;
  }

  // sort them based on priority
  std::sort(m_tools.begin(), m_tools.end(), ToolSort);

  int index = 1;
  foreach (ToolPlugin* toolPlugin, m_tools) {
    // Add action to toolbar.
    toolPlugin->setParent(this);
    QAction* action = toolPlugin->activateAction();
    action->setParent(m_toolToolBar);
    action->setCheckable(true);
    if (index < 10)
      action->setShortcut(QKeySequence(QString("Ctrl+%1").arg(index)));
    action->setData(toolPlugin->objectName());
    m_toolToolBar->addAction(action);

    connect(action, &QAction::triggered, this, &MainWindow::toolActivated);
    ++index;
  }

  // add the fake tooltip method
  ToolTipFilter* filter = new ToolTipFilter(this);
  foreach (QToolButton* button, m_toolToolBar->findChildren<QToolButton*>()) {
    button->installEventFilter(filter);
  }
}

QString MainWindow::extensionToWildCard(const QString& extension)
{
  // This is a list of "extensions" returned by OB that are not actually
  // file extensions, but rather the full filename of the file. These
  // will be used as-is in the filter string, while others will be prepended
  // with "*.".
  static QStringList nonExtensions;
  if (nonExtensions.empty()) {
    nonExtensions << "POSCAR"  // VASP input geometry
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
  const std::vector<const Io::FileFormat*>& formats, bool addAllEntry)
{
  QString result;

  // Create a map that groups the file extensions by name:
  QMap<QString, QString> formatMap;
  for (vector<const FileFormat*>::const_iterator it = formats.begin(),
                                                 itEnd = formats.end();
       it != itEnd; ++it) {
    QString name(QString::fromStdString((*it)->name()));
    vector<string> exts = (*it)->fileExtensions();
    for (vector<string>::const_iterator eit = exts.begin(), eitEnd = exts.end();
         eit != eitEnd; ++eit) {
      QString ext(QString::fromStdString(*eit));
      if (!formatMap.values(name).contains(ext)) {
        formatMap.insertMulti(name, ext);
      }
    }
  }

  // This holds all known extensions:
  QStringList allExtensions;

  foreach (const QString& desc, formatMap.uniqueKeys()) {
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
    // We're using the property interface to QMessageBox, rather than
    // the static functions. This is more work, but gives us some nice
    // fine-grain control. This helps both on Windows and Mac
    // look more "native."
    QPointer<MESSAGEBOX> msgBox = new MESSAGEBOX(
      QMessageBox::Warning, tr("Avogadro"),
      tr("Do you want to save the changes to the document?"),
      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this);

    // On Mac, this will make a sheet relative to the window
    // Unfortunately, it also closes the window when the box disappears!
    // msgBox->setWindowModality(Qt::WindowModal);
    // second line of text
    msgBox->setInformativeText(
      tr("Your changes will be lost if you don't save them."));
    msgBox->setDefaultButton(QMessageBox::Save);
#ifdef Q_OS_MAC
    msgBox->setWindowModality(Qt::WindowModal);
#endif

    // OK, now add shortcuts for save and discard
    msgBox->button(QMessageBox::Save)
      ->setShortcut(QKeySequence(tr("Ctrl+S", "Save")));
    msgBox->button(QMessageBox::Discard)
      ->setShortcut(QKeySequence(tr("Ctrl+D", "Discard")));

    int response = msgBox->exec();

    switch (static_cast<QMessageBox::StandardButton>(response)) {
      case QMessageBox::Save:
        // Synchronous save -- needed so that we don't lose changes if the
        // writer
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
  FileFormatManager& ffm = FileFormatManager::instance();
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
  ExtensionPlugin* extension(qobject_cast<ExtensionPlugin*>(sender()));
  if (!extension)
    return;
  foreach (FileFormat* format, extension->fileFormats()) {
    if (!FileFormatManager::registerFormat(format)) {
      qWarning() << tr("Error while loading the “%1” file format.")
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
    const FileFormat* format = QtGui::FileFormatDialog::findFileFormat(
      this, tr("Select file format"), file, FileFormat::File | FileFormat::Read,
      "Avogadro:");

    if (!openFile(file, format ? format->newInstance() : nullptr)) {
      MESSAGEBOX::warning(this, tr("Cannot open file"),
                          tr("Avogadro cannot open"
                             " “%1”.")
                            .arg(file));
    }
  }
}

void MainWindow::clearQueuedFiles()
{
  if (!m_queuedFilesStarted && !m_queuedFiles.isEmpty()) {
    MESSAGEBOX::warning(this, tr("Cannot open files"),
                        tr("Avogadro cannot open"
                           " “%1”.")
                          .arg(m_queuedFiles.join("\n")));
    m_queuedFiles.clear();
  }
}

} // End of Avogadro namespace
