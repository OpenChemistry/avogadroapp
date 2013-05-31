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
#include "menubuilder.h"
#include "backgroundfileformat.h"

#include <avogadro/qtgui/molecule.h>
#include <avogadro/core/elements.h>
#include <avogadro/io/cjsonformat.h>
#include <avogadro/io/cmlformat.h>
#include <avogadro/io/fileformat.h>
#include <avogadro/io/fileformatmanager.h>
#include <avogadro/qtopengl/glwidget.h>
#include <avogadro/qtplugins/pluginmanager.h>
#include <avogadro/qtgui/sceneplugin.h>
#include <avogadro/qtgui/scenepluginmodel.h>
#include <avogadro/qtgui/toolplugin.h>
#include <avogadro/qtgui/extensionplugin.h>
#include <avogadro/qtgui/periodictableview.h>
#include <avogadro/qtgui/utilities.h>
#include <avogadro/rendering/glrenderer.h>
#include <avogadro/rendering/scene.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>
#include <QtCore/QDebug>
#include <QtCore/QFileInfo>
#include <QtCore/QSettings>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtGui/QActionGroup>
#include <QtGui/QCloseEvent>
#include <QtGui/QInputDialog>
#include <QtGui/QKeySequence>
#include <QtGui/QMenu>
#include <QtGui/QMenuBar>
#include <QtGui/QMessageBox>
#include <QtGui/QFileDialog>
#include <QtGui/QStatusBar>
#include <QtGui/QToolBar>

#include <QtGui/QDockWidget>
#include <QtGui/QTreeView>
#include <QtGui/QHeaderView>
#include <QtGui/QProgressDialog>
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
using QtGui::ScenePluginModel;
using QtGui::ToolPlugin;
using QtGui::ExtensionPlugin;

MainWindow::MainWindow(const QString &fileName, bool disableSettings)
  : m_ui(new Ui::MainWindow),
    m_molecule(0),
    m_menuBuilder(new MenuBuilder),
    m_fileReadThread(NULL),
    m_threadedReader(NULL),
    m_fileReadProgress(NULL),
    m_fileReadMolecule(NULL),
    m_fileToolBar(new QToolBar(this)),
    m_toolToolBar(new QToolBar(this))
{
  m_ui->setupUi(this);

  QIcon icon(":/icons/avogadro.png");
  setWindowIcon(icon);

  m_fileToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  addToolBar(m_fileToolBar);
  addToolBar(m_toolToolBar);

  // Create the scene plugin model
  ScenePluginModel &scenePluginModel = m_ui->glWidget->sceneModel();
  m_ui->scenePluginTreeView->setModel(&scenePluginModel);
  m_ui->scenePluginTreeView->setAlternatingRowColors(true);
  m_ui->scenePluginTreeView->header()->stretchLastSection();
  m_ui->scenePluginTreeView->header()->setVisible(false);

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
    if (scenePlugin)
      scenePluginModel.addItem(scenePlugin);
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
      connect(this, SIGNAL(moleculeChanged(QtGui::Molecule*)),
              extension, SLOT(setMolecule(QtGui::Molecule*)));
      connect(extension, SIGNAL(moleculeReady(int)), SLOT(moleculeReady(int)));
      connect(extension, SIGNAL(fileFormatsReady()), SLOT(fileFormatsReady()));
      buildMenu(extension);
    }
  }

  // Build up the standard menus, incorporate dynamic menus.
  buildMenu();
  updateRecentFiles();

  // Try to open the file passed in. If opening fails, create a new molecule.
  if (!fileName.isEmpty()) {
    m_queuedFiles.append(fileName);
    // Give the plugins 5 seconds before timing out queued files.
    QTimer::singleShot(5000, this, SLOT(clearQueuedFiles()));
    readQueuedFiles();
  }
  if (!m_molecule)
    newMolecule();
  statusBar()->showMessage(tr("Ready..."), 2000);
}

MainWindow::~MainWindow()
{
  writeSettings();
  delete m_molecule;
  delete m_menuBuilder;
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
  m_ui->glWidget->clearScene();

  // Set molecule. Wait until after emitting MoleculeChanged to delete the
  // old one.
  QtGui::Molecule *oldMolecule(m_molecule);
  m_molecule = mol;

  // If the molecule is empty, make the editor active. Otherwise, use the
  // navigator tool.
  if (m_molecule) {
    QString targetToolName = m_molecule->atomCount() > 0 ? tr("Navigate tool")
                                                         : tr("Editor tool");
    foreach (ToolPlugin *toolPlugin, m_ui->glWidget->tools()) {
      if (toolPlugin->name() == targetToolName)
        toolPlugin->activateAction()->trigger();
    }
  }

  emit moleculeChanged(m_molecule);

  if (oldMolecule)
    oldMolecule->deleteLater();

  m_ui->glWidget->setMolecule(m_molecule);
  m_ui->glWidget->updateScene();
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
}

void MainWindow::openFile()
{
  QString filter(QString("%1 (*.cml);;%2 (*.cjson)")
                 .arg(tr("Chemical Markup Language"))
                 .arg(tr("Chemical JSON")));

  QSettings settings;
  QString dir = settings.value("MainWindow/lastOpenDir").toString();

  QString fileName = QFileDialog::getOpenFileName(this,
                                                  tr("Save chemical file"),
                                                  dir, filter);

  if (fileName.isEmpty()) // user cancel
    return;

  QFileInfo info(fileName);
  dir = info.absoluteDir().absolutePath();
  settings.setValue("MainWindow/lastOpenDir", dir);

  // Create one of our readers to read the file:
  QString extension = info.suffix().toLower();
  Io::FileFormat *reader = NULL;
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
  std::vector<const Io::FileFormat *> formats =
      Io::FileFormatManager::instance().fileFormats(Io::FileFormat::Read
                                                    | Io::FileFormat::File);

  QString filter(generateFilterString(formats));

  QSettings settings;
  QString dir = settings.value("MainWindow/lastOpenDir").toString();

  QString fileName = QFileDialog::getOpenFileName(this,
                                                  tr("Import chemical file"),
                                                  dir, filter);

  if (fileName.isEmpty()) // user cancel
    return;

  dir = QFileInfo(fileName).absoluteDir().absolutePath();
  settings.setValue("MainWindow/lastOpenDir", dir);

  if (!openFile(fileName)) {
    QMessageBox::information(this, tr("Cannot open file"),
                             tr("Can't open supplied file %1").arg(fileName));
  }
}

bool MainWindow::openFile(const QString &fileName, Io::FileFormat *reader)
{
  if (fileName.isEmpty())
    return false;

  // If a reader was not specified, look one up in the format manager.
  if (reader == NULL) {
    // Extract file extension
    QFileInfo info(fileName);
    QString ext = info.suffix();
    if (ext.isEmpty())
      ext = info.fileName();
    ext = ext.toLower();

    // Lookup readers that can handle this extension
    std::vector<const Io::FileFormat*> matches(
          Io::FileFormatManager::instance().fileFormatsFromFileExtension(
            ext.toStdString(), Io::FileFormat::Read | Io::FileFormat::File));

    if (matches.empty())
      return false;

    if (matches.size() == 1) {
      // One reader found -- use it.
      reader = matches.front()->newInstance();
    }
    else {
      // Multiple readers found. Ask the user which they'd like to use
      QStringList idents;
      for (std::vector<const Io::FileFormat*>::const_iterator
           it = matches.begin(), itEnd = matches.end(); it != itEnd; ++it) {
        idents << QString::fromStdString((*it)->identifier());
      }

      // See if they used one before:
      QString lastIdent = QSettings().value(
            QString("MainWindow/fileReader/%1/lastUsed").arg(ext)).toString();

      int lastIdentIndex = idents.indexOf(lastIdent);
      if (lastIdentIndex < 0)
        lastIdentIndex = 0;

      bool ok = false;
      QString item =
          QInputDialog::getItem(this, tr("Select reader"),
                                tr("Avogadro found multiple backends that can "
                                   "read this file. Which one should be used?"),
                                idents, lastIdentIndex, false, &ok);
      int index = idents.indexOf(item);

      if (!ok
          || index < 0
          || index + 1 > static_cast<int>(matches.size()))
        return false;

      reader = matches[index]->newInstance();

      // Store chosen reader for next time
      QSettings().setValue(QString("MainWindow/fileReader/%1/lastUsed")
                           .arg(ext), item);
    }
  }

  if (!reader) // newInstance failed?
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
  m_fileReadMolecule->setData("fileName", fileName.toStdString());
  m_threadedReader->moveToThread(m_fileReadThread);
  m_threadedReader->setMolecule(m_fileReadMolecule);
  m_threadedReader->setFileName(fileName);

  // Setup a progress dialog in case file loading is slow
  m_fileReadProgress = new QProgressDialog(this);
  m_fileReadProgress->setRange(0, 0);
  m_fileReadProgress->setValue(0);
  m_fileReadProgress->setMinimumDuration(750);
  m_fileReadProgress->setWindowTitle(tr("Reading File"));
  m_fileReadProgress->setLabelText(tr("Opening file '%1'\nwith '%2'")
                                   .arg(fileName).arg(ident));
  /// @todo Add API to abort file ops
  m_fileReadProgress->setCancelButton(NULL);
  connect(m_fileReadThread, SIGNAL(started()), m_threadedReader, SLOT(read()));
  connect(m_threadedReader, SIGNAL(finished()), m_fileReadThread, SLOT(quit()));
  connect(m_threadedReader, SIGNAL(finished()),
          SLOT(backgroundReaderFinished()));

  // Start the file operation
  m_fileReadThread->start();
  m_fileReadProgress->show();

  return true;
}

void MainWindow::backgroundReaderFinished()
{
  QString fileName = m_fileReadMolecule->data("fileName").toString().c_str();
  if (m_fileReadProgress->wasCanceled()) {
    delete m_fileReadMolecule;
  }
  else if (m_threadedReader->success()) {
    if (!fileName.isEmpty()) {
      m_recentFiles.prepend(fileName);
      updateRecentFiles();
    }
    setMolecule(m_fileReadMolecule);
    statusBar()->showMessage(tr("Molecule loaded (%1 atoms, %2 bonds)")
                             .arg(m_molecule->atomCount())
                             .arg(m_molecule->bondCount()), 2500);
    setWindowTitle(tr("Avogadro - %1").arg(fileName));
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
  m_fileReadProgress->deleteLater();
  m_fileReadProgress = NULL;
}

void MainWindow::toolActivated()
{
  if (QAction *action = qobject_cast<QAction*>(sender())) {
    if (ToolPlugin *toolPlugin = qobject_cast<ToolPlugin*>(action->parent())) {
      if (m_ui->glWidget->tools().contains(toolPlugin)) {
        bool ok;
        int index = action->data().toInt(&ok);
        if (ok && index < m_ui->toolWidgetStack->count()) {
          m_ui->glWidget->setActiveTool(toolPlugin);
          m_ui->toolWidgetStack->setCurrentIndex(index);
          m_ui->toolName->setText(action->text());
        }
      }
    }
  }
}

void MainWindow::openRecentFile()
{
  QAction *action = qobject_cast<QAction *>(sender());
  if (action) {
    QString fileName = action->data().toString();
    if(!openFile(fileName)) {
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

void MainWindow::saveFile()
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
    return;

  QFileInfo info(fileName);
  dir = info.absoluteDir().absolutePath();
  settings.setValue("MainWindow/lastSaveDir", dir);

  // Create one of our writers to save the file:
  QString extension = info.suffix().toLower();
  Io::FileFormat *writer = NULL;
  if (extension == "cml")
    writer = new Io::CmlFormat;
  else if (extension == "cjson")
    writer = new Io::CjsonFormat;

  saveFile(fileName, writer);
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
                                                  dir, filter);

  if (fileName.isEmpty()) // user cancel
    return;

  dir = QFileInfo(fileName).absoluteDir().absolutePath();
  settings.setValue("MainWindow/lastSaveDir", dir);

  saveFile(fileName);
}

void MainWindow::saveFile(const QString &fileName, Io::FileFormat *writer)
{
  if (fileName.isEmpty() || !m_molecule)
    return;

  // If a writer was not specified, look one up in the format manager.
  if (writer == NULL) {
    // Extract file extension
    QFileInfo info(fileName);
    QString ext = info.suffix();
    if (ext.isEmpty())
      ext = info.fileName();
    ext = ext.toLower();

    // Lookup writers that can handle this extension
    std::vector<const Io::FileFormat*> matches(
          Io::FileFormatManager::instance().fileFormatsFromFileExtension(
            ext.toStdString(), Io::FileFormat::Write | Io::FileFormat::File));

    if (matches.empty()) {
      QMessageBox::information(this, tr("Cannot save file"),
                               tr("Avogadro doesn't know how to write files of "
                                  "type '%1'.").arg(ext));
      return;
    }

    if (matches.size() == 1) {
      // One writer found -- use it.
      writer = matches.front()->newInstance();
    }
    else {
      // Multiple writers found. Ask the user which they'd like to use
      QStringList idents;
      for (std::vector<const Io::FileFormat*>::const_iterator
           it = matches.begin(), itEnd = matches.end(); it != itEnd; ++it) {
        idents << QString::fromStdString((*it)->identifier());
      }

      // See if they used one before:
      QString lastIdent = QSettings().value(
            QString("MainWindow/fileWriter/%1/lastUsed").arg(ext)).toString();

      int lastIdentIndex = idents.indexOf(lastIdent);
      if (lastIdentIndex < 0)
        lastIdentIndex = 0;

      bool ok = false;
      QString item =
          QInputDialog::getItem(this, tr("Select writer"),
                                tr("Avogadro found multiple backends that can "
                                   "save this file. Which one should be used?"),
                                idents, lastIdentIndex, false, &ok);
      int index = idents.indexOf(item);

      if (!ok
          || index < 0
          || index + 1 > static_cast<int>(matches.size()))
        return;

      writer = matches[index]->newInstance();

      // Store chosen writer for next time
      QSettings().setValue(QString("MainWindow/fileWriter/%1/lastUsed")
                           .arg(ext), item);
    }
  }

  if (!writer) // newInstance failed?
    return;

  QString ident = QString::fromStdString(writer->identifier());

  // Prepare the background thread
  QThread fileThread(this);
  BackgroundFileFormat threadedWriter(writer);
  threadedWriter.moveToThread(&fileThread);
  threadedWriter.setMolecule(m_molecule);
  threadedWriter.setFileName(fileName);

  // Setup a progress dialog in case file writing is slow
  QProgressDialog progDialog(this);
  progDialog.setRange(0, 0);
  progDialog.setValue(0);
  progDialog.setMinimumDuration(750);
  progDialog.setWindowTitle(tr("Writing File"));
  progDialog.setLabelText(tr("Writing file '%1'\nwith '%2'")
                          .arg(fileName).arg(ident));
  /// @todo Add API to abort file ops
  progDialog.setCancelButton(NULL);
  connect(&fileThread, SIGNAL(started()), &threadedWriter, SLOT(read()));
  connect(&threadedWriter, SIGNAL(finished()), &fileThread, SLOT(quit()));

  // Start the file operation
  fileThread.start();
  progDialog.show();
  while (fileThread.isRunning())
    qApp->processEvents(QEventLoop::AllEvents, 500);

  if (progDialog.wasCanceled())
    return;

  if (threadedWriter.success()) {
    m_recentFiles.prepend(fileName);
    updateRecentFiles();
    statusBar()->showMessage(tr("Molecule saved (%1)").arg(fileName));
    setWindowTitle(tr("Avogadro - %1").arg(fileName));
  }
  else {
    QMessageBox::critical(this, tr("Error saving file"),
                          tr("The file could not be saved.\n")
                          + threadedWriter.error(),
                          QMessageBox::Ok, QMessageBox::Ok);
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
  action->setIcon(QtGui::standardIcon("document-new"));
  m_menuBuilder->addAction(path, action, 999);
  m_fileToolBar->addAction(action);
  connect(action, SIGNAL(triggered()), SLOT(newMolecule()));
  // Open
  action = new QAction(tr("&Open"), this);
  action->setShortcut(QKeySequence("Ctrl+O"));
  action->setIcon(QtGui::standardIcon("document-open"));
  m_menuBuilder->addAction(path, action, 970);
  m_fileToolBar->addAction(action);
  connect(action, SIGNAL(triggered()), SLOT(openFile()));
  // Save As
  action = new QAction(tr("Save &As"), this);
  action->setShortcut(QKeySequence("Ctrl+Shift+S"));
  action->setIcon(QtGui::standardIcon("document-save-as"));
  m_menuBuilder->addAction(path, action, 960);
  connect(action, SIGNAL(triggered()), SLOT(saveFile()));
  // Import
  action = new QAction(tr("&Import"), this);
  action->setShortcut(QKeySequence("Ctrl+Shift+O"));
  action->setIcon(QtGui::standardIcon("document-import"));
  m_menuBuilder->addAction(path, action, 950);
  m_fileToolBar->addAction(action);
  connect(action, SIGNAL(triggered()), SLOT(importFile()));
  // Export
  action = new QAction(tr("&Export"), this);
  m_menuBuilder->addAction(path, action, 940);
  action->setIcon(QtGui::standardIcon("document-export"));
  connect(action, SIGNAL(triggered()), SLOT(exportFile()));
  // Quit
  action = new QAction(tr("&Quit"), this);
  action->setShortcut(QKeySequence("Ctrl+Q"));
  action->setIcon(QtGui::standardIcon("application-exit"));
  m_menuBuilder->addAction(path, action, -200);
  connect(action, SIGNAL(triggered()), qApp, SLOT(quit()));

  QStringList helpPath;
  helpPath << tr("&Help");
  QAction *about = new QAction("&About", this);
  about->setIcon(QtGui::standardIcon("help-about"));
  m_menuBuilder->addAction(helpPath, about, 20);
  connect(about, SIGNAL(triggered()), SLOT(showAboutDialog()));

  // Populate the recent file actions list.
  path << "Recent Files";
  for (int i = 0; i < 10; ++i) {
    action = new QAction(QString::number(i), this);
    m_actionRecentFiles.push_back(action);
    action->setIcon(QtGui::standardIcon("document-open-recent"));
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

void MainWindow::buildTools(QList<QtGui::ToolPlugin *> toolList)
{
  QActionGroup *toolActions = new QActionGroup(this);
  int index = 0;
  foreach (QtGui::ToolPlugin *toolPlugin, toolList) {
    // Add action to toolbar
    QAction *action = toolPlugin->activateAction();
    if (action->parent() != toolPlugin)
      action->setParent(toolPlugin);
    action->setCheckable(true);
    if (index + 1 < 10)
      action->setShortcut(QKeySequence(QString("Ctrl+%1").arg(index + 1)));
    action->setData(index);
    toolActions->addAction(action);
    connect(action, SIGNAL(triggered()), SLOT(toolActivated()));

    // Setup tool widget
    QWidget *toolWidget = toolPlugin->toolWidget();
    if (!toolWidget)
      toolWidget = new QWidget();
    m_ui->toolWidgetStack->addWidget(toolWidget);
    ++index;
  }

  m_toolToolBar->addActions(toolActions->actions());

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
  foreach (Io::FileFormat *format, extension->fileFormats()) {
    if (!Io::FileFormatManager::registerFormat(format)) {
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
  if (m_queuedFiles.size()) {
    // Currently only read one file, this should be extended to allow multiple
    // files once the interface supports that concept.
    if (openFile(m_queuedFiles.front()))
      m_queuedFiles.clear();
  }
}

void MainWindow::clearQueuedFiles()
{
  if (!m_queuedFiles.isEmpty()) {
    QMessageBox::warning(this, tr("Cannot open file"),
                         tr("Avogadro timed out and doesn't know how to open"
                            " '%1'.").arg(m_queuedFiles.front()));
    m_queuedFiles.clear();
  }
}

} // End of Avogadro namespace
