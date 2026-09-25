// SPDX-License-Identifier: BSD-3-Clause
#include "webcontroller.h"

#include <QJsonArray>
#include <QUndoStack>
#include <avogadro/core/atom.h>
#include <avogadro/core/layermanager.h>
#include <avogadro/io/fileformatmanager.h>
#include <avogadro/qtgui/layermodel.h>
#include <avogadro/qtgui/molecule.h>
#include <avogadro/qtgui/rwmolecule.h>
#include <avogadro/qtgui/sceneplugin.h>
#include <avogadro/qtopengl/glwidget.h>
#include <avogadro/qtplugins/pluginmanager.h>
#include <sstream>
#include <stdexcept>

namespace Avogadro::Web {
namespace {
// Core currently leaves document layer registrations alive on destruction.
// The page repeatedly replaces documents, so release only our owned document's
// registry entry and settings after discarding its undo commands.
class DocumentMolecule final
  : public QtGui::Molecule
  , private Core::LayerManager
{
public:
  ~DocumentMolecule() override
  {
    undoMolecule()->undoStack().clear();
    auto info = getMoleculeInfo(this);
    for (auto& settings : info->settings)
      for (auto* value : settings.second)
        delete value;
    info->settings.clear();
    deleteMolecule(this);
    if (m_activeMolecule == this)
      m_activeMolecule = nullptr;
  }
};

QJsonObject failure(const QString& error)
{
  return { { "ok", false }, { "error", error } };
}
bool supported(const QString& format)
{
  return QStringList{ "cjson", "mol", "sdf", "xyz" }.contains(format);
}
// XYZ's native reader also reads trajectory frames. Limit its input to the
// first record, without changing the reader's chemistry or perception rules.
QString firstRecord(const QString& text, const QString& format, bool& multiple)
{
  if (format == "sdf" || format == "mol") {
    const auto lines = text.split('\n');
    for (int i = 0; i < lines.size(); ++i) {
      if (lines[i].trimmed() == "$$$$") {
        multiple = !lines.mid(i + 1).join('\n').trimmed().isEmpty();
        return lines.mid(0, i + 1).join('\n') + '\n';
      }
    }
  } else if (format == "xyz") {
    const auto lines = text.split('\n');
    bool ok = false;
    const auto count = lines.first().trimmed().toULongLong(&ok);
    if (ok && count <= static_cast<qulonglong>(lines.size()) &&
        count + 2 <= static_cast<qulonglong>(lines.size())) {
      const auto end = static_cast<int>(count + 2);
      multiple = !lines.mid(end).join('\n').trimmed().isEmpty();
      return lines.mid(0, end).join('\n') + '\n';
    }
  }
  return text;
}
}

WebController::WebController(QtOpenGL::GLWidget& view, QObject* parent)
  : QObject(parent)
  , m_view(view)
  , m_layers(std::make_unique<QtGui::LayerModel>())
{
  auto* plugins = QtPlugins::PluginManager::instance();
  plugins->load();
  const QMap<QString, QString> tools{ { "navigate", "Navigator" },
                                      { "draw", "Editor" },
                                      { "select", "Selection" } };
  for (auto it = tools.cbegin(); it != tools.cend(); ++it) {
    auto* factory =
      plugins->pluginFactory<QtGui::ToolPluginFactory>(it.value());
    if (!factory)
      throw std::runtime_error("A required web editor tool is missing");
    auto* tool = factory->createInstance(&m_view);
    m_tools.insert(it.key(), tool);
    m_view.addTool(tool);
  }
  connect(m_tools["draw"], SIGNAL(drawOptionsChanged()), this,
          SIGNAL(stateChanged()));
  auto* factory =
    plugins->pluginFactory<QtGui::ScenePluginFactory>("BallStick");
  if (!factory)
    throw std::runtime_error("The BallStick renderer is missing");
  m_view.sceneModel().addItem(factory->createInstance(&m_view));
  m_view.setDefaultTool(m_tools["navigate"]);
  newMolecule();
  setTool("draw");
}

WebController::~WebController()
{
  m_view.setMolecule(nullptr);
}

QtGui::Molecule& WebController::molecule() const
{
  return *m_molecule;
}

QJsonObject WebController::state() const
{
  const auto& stack = m_molecule->undoMolecule()->undoStack();
  return { { "tool", m_tool },
           { "drawOptions",
             QJsonObject::fromVariantMap(
               m_tools["draw"]->property("drawOptions").toMap()) },
           { "atomCount", static_cast<double>(m_molecule->atomCount()) },
           { "bondCount", static_cast<double>(m_molecule->bondCount()) },
           { "modified", !stack.isClean() },
           { "canUndo", stack.canUndo() },
           { "canRedo", stack.canRedo() } };
}

bool WebController::setTool(const QString& name)
{
  if (!m_tools.contains(name))
    return false;
  m_view.setActiveTool(m_tools[name]);
  m_tool = name;
  emit stateChanged();
  return true;
}

bool WebController::setDrawOptions(const QVariantMap& options)
{
  return m_tools["draw"]->handleCommand("setDrawOptions", options);
}

void WebController::replaceMolecule(std::unique_ptr<QtGui::Molecule> next)
{
  // QtGui::Molecule must be default-constructed before reading: that
  // constructor initializes its undo wrapper and stable atom/bond identifiers.
  auto previous = std::move(m_molecule);
  m_molecule = std::move(next);
  m_layers->addMolecule(m_molecule.get());
  m_view.setMolecule(m_molecule.get());
  for (auto* plugin : m_view.sceneModel().scenePlugins())
    plugin->setEnabled(true);
  auto& stack = m_molecule->undoMolecule()->undoStack();
  stack.clear();
  stack.setClean();
  connect(&stack, &QUndoStack::indexChanged, this,
          &WebController::stateChanged);
  connect(&stack, &QUndoStack::cleanChanged, this,
          &WebController::stateChanged);
  connect(m_molecule.get(), &QtGui::Molecule::changed, this,
          &WebController::stateChanged);
  m_view.updateScene();
  resetCamera();
  emit stateChanged();
}

void WebController::newMolecule()
{
  replaceMolecule(std::make_unique<DocumentMolecule>());
}

QJsonObject WebController::loadMolecule(const QString& text,
                                        const QString& formatName)
{
  const QString format = formatName.toLower();
  if (!supported(format))
    return failure("Supported formats: CJSON, MOL, SDF, XYZ.");
  bool multiple = false;
  const auto input = firstRecord(text, format, multiple).toStdString();
  auto next = std::make_unique<DocumentMolecule>();
  auto& formats = Io::FileFormatManager::instance();
  std::unique_ptr<Io::FileFormat> reader(formats.newFormatFromFileExtension(
    format.toStdString(), Io::FileFormat::Read));
  try {
    if (!reader || !reader->readString(input, *next))
      return failure(reader ? QString::fromStdString(reader->error())
                            : "Reader unavailable.");
  } catch (const std::exception& error) {
    return failure(QString::fromUtf8(error.what()));
  }
  replaceMolecule(std::move(next));
  return { { "ok", true },
           { "notice",
             multiple ? "Only the first molecule/frame was imported." : "" } };
}

QJsonObject WebController::exportMolecule(const QString& formatName) const
{
  const QString format = formatName.toLower();
  if (!supported(format))
    return failure("Supported formats: CJSON, MOL, SDF, XYZ.");
  auto& formats = Io::FileFormatManager::instance();
  std::unique_ptr<Io::FileFormat> writer(formats.newFormatFromFileExtension(
    format.toStdString(), Io::FileFormat::Write));
  std::string text;
  try {
    if (!writer || !writer->writeString(text, *m_molecule))
      return failure(writer ? QString::fromStdString(writer->error())
                            : "Writer unavailable.");
  } catch (const std::exception& error) {
    return failure(QString::fromUtf8(error.what()));
  }
  return { { "ok", true }, { "text", QString::fromStdString(text) } };
}

void WebController::markSaved()
{
  m_molecule->undoMolecule()->undoStack().setClean();
}
void WebController::undo()
{
  m_molecule->undoMolecule()->undoStack().undo();
}
void WebController::redo()
{
  m_molecule->undoMolecule()->undoStack().redo();
}
void WebController::resetCamera()
{
  m_view.resetCamera();
  m_view.requestUpdate();
}

void WebController::deleteSelection()
{
  auto* rw = m_molecule->undoMolecule();
  auto& stack = rw->undoStack();
  bool any = false;
  for (Index i = 0; i < m_molecule->atomCount(); ++i)
    any |= m_molecule->atom(i).selected();
  if (!any)
    return;
  stack.beginMacro(tr("Delete selected atoms"));
  for (Index i = m_molecule->atomCount(); i > 0; --i)
    if (m_molecule->atom(i - 1).selected())
      rw->removeAtom(i - 1);
  stack.endMacro();
  rw->emitChanged(QtGui::Molecule::Atoms | QtGui::Molecule::Bonds |
                  QtGui::Molecule::Removed);
}
} // namespace Avogadro::Web
