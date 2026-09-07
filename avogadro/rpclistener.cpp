/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#include "rpclistener.h"
#include "avogadroappconfig.h"
#include "mainwindow.h"

#include "rpc/connection.h"
#include "rpc/jsonrpc.h"
#include "rpc/jsonrpcclient.h"
#include "rpc/localsocketconnectionlistener.h"

#include <avogadro/core/basisset.h>
#include <avogadro/core/version.h>
#include <avogadro/io/fileformatmanager.h>
#include <avogadro/qtgui/molecule.h>
#include <avogadro/qtgui/sceneplugin.h>
#include <avogadro/qtgui/scenepluginmodel.h>
#include <avogadro/qtopengl/glwidget.h>

#include <QtCore/QBuffer>
#include <QtCore/QByteArray>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonValue>
#include <QtCore/QSize>
#include <QtCore/QTimer>
#include <QtCore/QVariantMap>
#include <QtGui/QImage>
#include <QtWidgets/QApplication>
#include <QtWidgets/QInputDialog>

#include <algorithm>

namespace Avogadro {

using Core::BasisSet;
using Io::FileFormatManager;
using QtGui::Molecule;
using QtGui::ScenePlugin;
using QtOpenGL::GLWidget;
using std::string;

namespace {
/// How long to wait for a command before giving up on it, in seconds. Most
/// commands are fast; a script that expects a slow one passes "timeout".
constexpr int defaultCommandTimeout = 60;

/// The request could not be carried out.
constexpr int errorRequestFailed = -1;
/// A command that was started failed, or timed out.
constexpr int errorCommandFailed = -2;
/// The plugin is already running a command.
constexpr int errorPluginBusy = -3;
/// No tool or extension claims this method.
constexpr int errorMethodNotFound = -32601;

/// The RPC protocol version reported by "version". Bumped whenever the wire
/// contract changes; "1" was the immediate-reply era and is never reported
/// by a build that has this command.
constexpr int rpcProtocolVersion = 2;

/// A single row of the "listCommands" table for a built-in method.
struct BuiltinCommand
{
  const char* name;
  const char* description;
};

/// Every method this listener answers itself, i.e. everything handled below
/// in messageReceived() plus "internalPing" (answered earlier, in JsonRpc)
/// and "kill". Kept sorted by name for readability; listCommands() sorts its
/// output anyway.
const BuiltinCommand builtinCommands[] = {
  { "exportFile",
    "Write the active molecule to a file, guessing the format from the "
    "extension." },
  { "getMolecule", "Return the active molecule serialized as a string." },
  { "internalPing", "Check whether the server is responsive." },
  { "kill", "Shut down Avogadro. Only enabled when started with "
            "--testing." },
  { "listCommands", "List every command the server understands." },
  { "listDisplayTypes",
    "List the scene display types available for the active view." },
  { "loadMolecule", "Read molecule data from a string and make it the "
                    "active molecule." },
  { "moleculeInfo", "Report summary statistics about the active molecule." },
  { "openFile", "Read a file from disk and make it the active molecule." },
  { "renderImage", "Render the current view to a PNG, inline or to a "
                   "file. Omit width/height for the native framebuffer "
                   "resolution, or supply both together to fit that "
                   "render onto a canvas of exactly that size." },
  { "saveGraphic", "Render the current view and save it as an image at "
                   "the window's current size." },
  { "setProjection",
    "Switch between perspective and orthographic projection." },
  { "setRenderTypes", "Enable or disable scene display types by name." },
  { "version",
    "Report Avogadro application, library, Qt and protocol versions." },
};
} // namespace

RpcListener::RpcListener(const QString& connectionName, QObject* parent_)
  : QObject(parent_)
  , m_pingClient(nullptr)
{
  m_rpc = new RPC::JsonRpc(this);

  m_connectionListener =
    new RPC::LocalSocketConnectionListener(this, connectionName);

  connect(m_connectionListener, &RPC::ConnectionListener::connectionError, this,
          &RpcListener::connectionError);

  m_rpc->addConnectionListener(m_connectionListener);

  connect(m_rpc, &RPC::JsonRpc::messageReceived, this,
          &RpcListener::messageReceived);

  // Find the main window.
  m_window = nullptr;
  foreach (QWidget* widget, QApplication::topLevelWidgets())
    if ((m_window = qobject_cast<MainWindow*>(widget)))
      break;

  if (m_window) {
    connect(this, &RpcListener::callSetMolecule, m_window,
            &MainWindow::setMolecule);
    connect(m_window, &MainWindow::commandCompleted, this,
            &RpcListener::resolvePending);
  }

  // Do not leave a script waiting on a reply that will never come.
  connect(qApp, &QCoreApplication::aboutToQuit, this,
          [this]() { failAllPending(tr("Avogadro is shutting down.")); });
}

RpcListener::~RpcListener()
{
  m_rpc->removeConnectionListener(m_connectionListener);
  m_connectionListener->stop();
}

void RpcListener::start()
{
  m_connectionListener->start();
}

void RpcListener::connectionError(RPC::ConnectionListener::Error error,
                                  const QString& message)
{
  qDebug() << "Error starting RPC server:" << message;
  if (error == RPC::ConnectionListener::AddressInUseError) {
    // Try to ping the existing server to see if it is alive:
    if (!m_pingClient)
      m_pingClient = new RPC::JsonRpcClient(this);
    bool result(
      m_pingClient->connectToServer(m_connectionListener->connectionString()));

    if (result) {
      QJsonObject request(m_pingClient->emptyRequest());
      request["method"] = QLatin1String("internalPing");
      connect(m_pingClient, &RPC::JsonRpcClient::resultReceived, this,
              &RpcListener::receivePingResponse);
      result = m_pingClient->sendRequest(request);
    }

    // If any of the above failed, trigger a failure now:
    if (!result)
      receivePingResponse();
    else // Otherwise wait 200 ms
      QTimer::singleShot(200, this, [this]() { receivePingResponse(); });
  }
}

void RpcListener::receivePingResponse(const QJsonObject& response)
{
  // Disconnect and remove the ping client the first time this is called:
  if (m_pingClient) {
    m_pingClient->deleteLater();
    m_pingClient = nullptr;
  } else {
    // In case the single shot timeout is triggered after the slot is called
    // directly or in response to m_pingClient's signal.
    return;
  }

  bool pingSuccessful = response.value("result").toString() == QString("pong");
  if (pingSuccessful) {
    qDebug() << "Other server is alive. Not starting new instance.";
  } else {
    qDebug() << "Starting new server.";
    m_connectionListener->stop(true);
    m_connectionListener->start();
  }
}

void RpcListener::messageReceived(const RPC::Message& message)
{
  QString method = message.method();
  QJsonObject params = message.params().toObject();

  // "wait" and "timeout" are reserved: they configure how this request is
  // answered, and are removed so that no plugin ever sees them as an option.
  const bool wait = params.take("wait").toBool(false);
  const QJsonValue timeoutValue = params.take("timeout");
  int timeoutSeconds = defaultCommandTimeout;
  if (timeoutValue.isDouble()) {
    timeoutSeconds = static_cast<int>(timeoutValue.toDouble());
    if (timeoutSeconds < 1)
      timeoutSeconds = 1;
  }

  // check for quit message first, since it doesn't require a window
  if (method == "kill") {
    // Only allow avogadro to be killed through RPC if it was started with the
    // '--testing' flag.
    if (qApp->arguments().contains("--testing")) {
      RPC::Message response = message.generateResponse();
      response.setResult(true);
      response.send();

      qApp->quit();
    } else {
      // send error response
      RPC::Message errorMessage = message.generateErrorResponse();
      errorMessage.setErrorCode(errorRequestFailed);
      errorMessage.setErrorMessage(
        "Ignoring kill command. Start with '--testing' to enable.");
      errorMessage.send();
    }
    // Either way the request has been answered. Without this the message
    // falls through to the handler chain below and a second reply is sent.
    return;
  }

  // Also answered before the window check, next to "kill": a client needs
  // to be able to negotiate compatibility while Avogadro is still starting.
  if (method == "version") {
    QVariantMap result;
    result["avogadroApp"] = QLatin1String(AvogadroApp_VERSION);
    result["avogadroLibs"] = QLatin1String(Avogadro::version());
    result["qt"] = QLatin1String(qVersion());
#if defined(Q_OS_MAC)
    result["platform"] = QStringLiteral("macos");
#elif defined(Q_OS_WIN)
    result["platform"] = QStringLiteral("windows");
#elif defined(Q_OS_LINUX)
    result["platform"] = QStringLiteral("linux");
#elif defined(Q_OS_BSD4)
    result["platform"] = QStringLiteral("bsd");
#else
    result["platform"] = QStringLiteral("unknown");
#endif
    result["rpcProtocol"] = rpcProtocolVersion;

    RPC::Message response = message.generateResponse();
    response.setResult(QJsonObject::fromVariantMap(result));
    response.send();
    return;
  }

  // check if there's an active window
  if (m_window == nullptr) {
    // send error response
    RPC::Message errorMessage = message.generateErrorResponse();
    errorMessage.setErrorCode(errorRequestFailed);
    errorMessage.setErrorMessage("No Active Avogadro Window");
    errorMessage.send();
    return;
  }

  // okay, window is open
  if (method == "openFile") {
    // Read the supplied file.
    string fileName = params["fileName"].toString().toStdString();
    auto* molecule = new Molecule(this);
    bool success = FileFormatManager::instance().readFile(*molecule, fileName);
    if (success) {
      emit callSetMolecule(molecule);

      // set response
      RPC::Message response = message.generateResponse();
      response.setResult(true);
      response.send();
    } else {
      delete molecule;

      // send error response
      RPC::Message errorMessage = message.generateErrorResponse();
      errorMessage.setErrorCode(errorRequestFailed);
      errorMessage.setErrorMessage(
        QString("Failed to read file: %1")
          .arg(QString::fromStdString(FileFormatManager::instance().error())));
      errorMessage.send();
    }
  } else if (method == "saveGraphic") {
    // Read the supplied file.
    QString fileName = params["fileName"].toString();

    // save the image
    m_window->exportGraphics(fileName);

    // set response
    RPC::Message response = message.generateResponse();
    response.setResult(true);
    response.send();
  } else if (method == "exportFile") {
    // Save to the supplied file name
    QString filename = params["fileName"].toString();

    if (wait) {
      // Hold the reply until the background write finishes, the same way a
      // waited plugin command does.
      const quint64 token = ++m_nextToken;
      bool started = m_window->exportFile(filename, true, token);
      if (started) {
        holdReply(message, token, timeoutSeconds);
      } else {
        sendError(message, errorRequestFailed,
                  QString("Could not start exporting to %1.").arg(filename));
      }
    } else {
      bool result = m_window->exportFile(filename);

      // set response
      RPC::Message response = message.generateResponse();
      response.setResult(result);
      response.send();
    }
  } else if (method == "loadMolecule") {
    // get molecule data and format
    string content = params["content"].toString().toStdString();
    string format = params["format"].toString().toStdString();

    // read molecule data
    auto* molecule = new Molecule(this);
    bool success =
      FileFormatManager::instance().readString(*molecule, content, format);
    if (success) {
      emit callSetMolecule(molecule);

      // send response
      RPC::Message response = message.generateResponse();
      response.setResult(true);
      response.send();
    } else {
      delete molecule;

      // send error response
      RPC::Message errorMessage = message.generateErrorResponse();
      errorMessage.setErrorCode(errorRequestFailed);
      errorMessage.setErrorMessage(
        QString("Failed to read Chemical JSON: %1")
          .arg(QString::fromStdString(FileFormatManager::instance().error())));
      errorMessage.send();
    }
  } else if (method == "listCommands") {
    // Builtins come from the static table above; tool and extension
    // commands come from the main window's command maps. Read-backs answer
    // their payload directly as "result", not wrapped in the usual
    // "status"/"data" envelope.
    QVariantList commands;
    for (const auto& builtin : builtinCommands) {
      QVariantMap entry;
      entry["name"] = QLatin1String(builtin.name);
      entry["description"] = QLatin1String(builtin.description);
      entry["kind"] = QStringLiteral("builtin");
      entry["plugin"] = QString();
      entry["async"] = false;
      entry["schema"] = QVariantMap();
      commands.append(entry);
    }
    const QVariantList pluginCommands = m_window->pluginCommands();
    for (const QVariant& item : pluginCommands) {
      QVariantMap entry = item.toMap();
      entry["async"] = false;
      entry["schema"] = QVariantMap();
      commands.append(entry);
    }
    std::sort(commands.begin(), commands.end(),
              [](const QVariant& a, const QVariant& b) {
                return a.toMap().value("name").toString() <
                       b.toMap().value("name").toString();
              });

    RPC::Message response = message.generateResponse();
    response.setResult(QJsonArray::fromVariantList(commands));
    response.send();
  } else if (method == "moleculeInfo") {
    QVariantMap info;
    auto* mol = m_window->molecule();

    info["atomCount"] =
      mol != nullptr ? static_cast<qulonglong>(mol->atomCount()) : 0;
    info["bondCount"] =
      mol != nullptr ? static_cast<qulonglong>(mol->bondCount()) : 0;
    info["formula"] =
      mol != nullptr ? QString::fromStdString(mol->formula()) : QString();
    info["mass"] = mol != nullptr ? mol->mass() : 0.0;
    info["totalCharge"] =
      mol != nullptr ? static_cast<int>(mol->totalCharge()) : 0;
    info["spinMultiplicity"] =
      mol != nullptr ? static_cast<int>(mol->totalSpinMultiplicity()) : 0;
    info["coordinateSetCount"] =
      mol != nullptr ? static_cast<qulonglong>(mol->coordinate3dCount()) : 0;

    size_t selectedAtomCount = 0;
    if (mol != nullptr) {
      for (Index i = 0; i < mol->atomCount(); ++i) {
        if (mol->atomSelected(i))
          ++selectedAtomCount;
      }
    }
    info["selectedAtomCount"] = static_cast<qulonglong>(selectedAtomCount);

    const Index residueCount = mol != nullptr ? mol->residueCount() : 0;
    info["residueCount"] = static_cast<qulonglong>(residueCount);
    info["hasResidues"] = residueCount > 0;
    info["hasUnitCell"] = mol != nullptr && mol->unitCell() != nullptr;
    info["hasCustomElements"] = mol != nullptr && mol->hasCustomElements();

    const BasisSet* basis = mol != nullptr ? mol->basisSet() : nullptr;
    info["hasBasisSet"] = basis != nullptr;
    info["orbitalCount"] =
      basis != nullptr ? static_cast<int>(basis->molecularOrbitalCount()) : 0;
    info["homoIndex"] = basis != nullptr ? static_cast<int>(basis->homo()) : -1;

    info["cubeCount"] =
      mol != nullptr ? static_cast<qulonglong>(mol->cubeCount()) : 0;
    info["vibrationCount"] =
      mol != nullptr
        ? static_cast<qulonglong>(mol->vibrationFrequencies().size())
        : 0;
    info["fileName"] =
      mol != nullptr ? QString::fromStdString(mol->data("fileName").toString())
                     : QString();

    RPC::Message response = message.generateResponse();
    response.setResult(QJsonObject::fromVariantMap(info));
    response.send();
  } else if (method == "getMolecule") {
    QString format = params.contains("format") ? params["format"].toString()
                                               : QStringLiteral("cjson");
    if (format.isEmpty())
      format = QStringLiteral("cjson");

    auto* mol = m_window->molecule();
    if (mol == nullptr) {
      sendError(message, errorRequestFailed, tr("No molecule is open."));
    } else {
      string content;
      bool ok = FileFormatManager::instance().writeString(*mol, content,
                                                          format.toStdString());
      if (ok) {
        QVariantMap result;
        result["format"] = format;
        result["content"] = QString::fromStdString(content);

        RPC::Message response = message.generateResponse();
        response.setResult(QJsonObject::fromVariantMap(result));
        response.send();
      } else {
        sendError(message, errorRequestFailed,
                  QString("Failed to write molecule: %1")
                    .arg(QString::fromStdString(
                      FileFormatManager::instance().error())));
      }
    }
  } else if (method == "renderImage") {
    // Width and height must be given together, or not at all: with only
    // one of the two, there is no native size available yet (the grab
    // hasn't happened) to derive the other from without rendering twice, so
    // rather than guess an aspect ratio we reject the request outright.
    const bool haveWidth = params.contains("width");
    const bool haveHeight = params.contains("height");
    if (haveWidth != haveHeight) {
      sendError(message, errorRequestFailed,
                tr("renderImage requires both width and height, or "
                   "neither."));
      return;
    }

    QSize requestedSize;
    if (haveWidth && haveHeight) {
      int width = qBound(1, params["width"].toInt(), 8192);
      int height = qBound(1, params["height"].toInt(), 8192);
      requestedSize = QSize(width, height);
    }
    // else: leave requestedSize null, so renderToImage() returns the
    // native framebuffer grab untouched, at the maximum quality available.

    const bool transparentBackground =
      params["transparentBackground"].toBool(false);
    QString fileName = params["fileName"].toString();

    QSize nativeSize;
    QImage image = m_window->renderToImage(requestedSize, transparentBackground,
                                           &nativeSize);

    if (!fileName.isEmpty()) {
      if (QFileInfo(fileName).suffix().isEmpty())
        fileName += ".png";
      if (image.save(fileName, "PNG")) {
        QVariantMap result;
        result["width"] = image.width();
        result["height"] = image.height();
        result["nativeWidth"] = nativeSize.width();
        result["nativeHeight"] = nativeSize.height();
        result["fileName"] = fileName;

        RPC::Message response = message.generateResponse();
        response.setResult(QJsonObject::fromVariantMap(result));
        response.send();
      } else {
        sendError(message, errorRequestFailed,
                  QString("Could not write image to %1.").arg(fileName));
      }
    } else {
      QByteArray bytes;
      QBuffer buffer(&bytes);
      buffer.open(QIODevice::WriteOnly);
      image.save(&buffer, "PNG");

      QVariantMap result;
      result["width"] = image.width();
      result["height"] = image.height();
      result["nativeWidth"] = nativeSize.width();
      result["nativeHeight"] = nativeSize.height();
      result["format"] = QStringLiteral("png");
      result["data"] = QString::fromLatin1(bytes.toBase64());

      RPC::Message response = message.generateResponse();
      response.setResult(QJsonObject::fromVariantMap(result));
      response.send();
    }
  } else if (method == "listDisplayTypes") {
    QVariantList types;
    if (GLWidget* glWidget = m_window->activeGLWidget()) {
      const QList<ScenePlugin*> plugins = glWidget->sceneModel().scenePlugins();
      for (ScenePlugin* plugin : plugins) {
        if (plugin == nullptr)
          continue;
        QVariantMap entry;
        entry["name"] = plugin->objectName();
        entry["displayName"] = plugin->name();
        entry["enabled"] = plugin->isEnabled();
        entry["applicable"] = plugin->isApplicable();
        entry["hasSettings"] = plugin->hasSetupWidget();
        types.append(entry);
      }
    }

    RPC::Message response = message.generateResponse();
    response.setResult(QJsonArray::fromVariantList(types));
    response.send();
  } else { // ask the main window to handle the message
    QVariantMap options = params.toVariantMap();
    // Only a request that asked to wait needs a token to report back with.
    const quint64 token = wait ? ++m_nextToken : 0;
    QString resultMessage;
    QVariantMap resultData;

    MainWindow::CommandStatus status = m_window->handleCommand(
      method, options, token, &resultMessage, &resultData);

    switch (status) {
      case MainWindow::CommandStatus::Started:
        if (wait) {
          // Hold the reply until the plugin reports back.
          holdReply(message, token, timeoutSeconds);
        } else {
          // The caller did not ask to wait, so answer as we always have.
          RPC::Message response = message.generateResponse();
          response.setResult(true);
          response.send();
        }
        break;

      case MainWindow::CommandStatus::Finished:
        if (wait) {
          sendSuccess(message, resultMessage, resultData);
        } else {
          RPC::Message response = message.generateResponse();
          response.setResult(true);
          response.send();
        }
        break;

      case MainWindow::CommandStatus::Failed:
        sendError(message, errorCommandFailed,
                  resultMessage.isEmpty() ? tr("The command failed.")
                                          : resultMessage);
        break;

      case MainWindow::CommandStatus::Busy:
        sendError(message, errorPluginBusy,
                  tr("That plugin is already running a command."));
        break;

      case MainWindow::CommandStatus::NotHandled: {
        // send error response
        RPC::Message errorMessage = message.generateErrorResponse();
        errorMessage.setErrorCode(errorMethodNotFound);
        errorMessage.setErrorMessage("Method not found");
        QJsonObject errorDataObject;
        errorDataObject.insert("request", message.toJsonObject());
        errorMessage.setErrorData(errorDataObject);
        errorMessage.send();
        break;
      }
    }
  }
}

void RpcListener::holdReply(const RPC::Message& message, quint64 token,
                            int timeoutSeconds)
{
  PendingCommand pending;
  pending.request = message;
  pending.connection = message.connection();
  pending.timer = new QTimer(this);
  pending.timer->setSingleShot(true);
  pending.timer->setInterval(timeoutSeconds * 1000);
  connect(pending.timer, &QTimer::timeout, this, [this, token]() {
    // The caller has given up. Release the plugin as well, so that a command
    // which never reports back does not leave it busy for the whole session.
    if (m_window != nullptr)
      m_window->abandonCommand(token);
    resolvePending(token, false, tr("The command timed out."), QVariantMap());
  });

  m_pending.insert(token, pending);
  pending.timer->start();
}

void RpcListener::resolvePending(quint64 token, bool success,
                                 const QString& message,
                                 const QVariantMap& result)
{
  auto match = m_pending.find(token);
  if (match == m_pending.end())
    return; // already answered, or the caller never asked to wait

  const PendingCommand pending = match.value();
  m_pending.erase(match);

  if (pending.timer != nullptr)
    pending.timer->deleteLater();

  // The client may have gone away while the command was running. The request
  // holds a bare pointer to the connection, so check before answering.
  if (pending.connection.isNull())
    return;

  if (success)
    sendSuccess(pending.request, message, result);
  else
    sendError(pending.request, errorCommandFailed,
              message.isEmpty() ? tr("The command failed.") : message);
}

void RpcListener::failAllPending(const QString& reason)
{
  const QList<quint64> tokens = m_pending.keys();
  foreach (quint64 token, tokens)
    resolvePending(token, false, reason, QVariantMap());
}

void RpcListener::sendSuccess(const RPC::Message& request,
                              const QString& message, const QVariantMap& result)
{
  QJsonObject payload;
  payload.insert("status", QStringLiteral("finished"));
  if (!message.isEmpty())
    payload.insert("message", message);
  if (!result.isEmpty())
    payload.insert("data", QJsonObject::fromVariantMap(result));

  RPC::Message response = request.generateResponse();
  response.setResult(payload);
  response.send();
}

void RpcListener::sendError(const RPC::Message& request, int code,
                            const QString& message)
{
  RPC::Message errorMessage = request.generateErrorResponse();
  errorMessage.setErrorCode(code);
  errorMessage.setErrorMessage(message);
  errorMessage.send();
}

} // End of Avogadro namespace
