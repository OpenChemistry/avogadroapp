// SPDX-License-Identifier: BSD-3-Clause
#ifndef AVOGADRO_WEB_CONTROLLER_H
#define AVOGADRO_WEB_CONTROLLER_H

#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <memory>

namespace Avogadro {
namespace QtGui {
class Molecule;
class ToolPlugin;
class LayerModel;
}
namespace QtOpenGL {
class GLWidget;
}
namespace Web {

// One document per application instance. The viewport must outlive this object.
class WebController : public QObject
{
  Q_OBJECT
public:
  explicit WebController(QtOpenGL::GLWidget& view, QObject* parent = nullptr);
  ~WebController() override;
  QJsonObject state() const;
  QtGui::Molecule& molecule() const;
  bool setTool(const QString& name);
  bool setDrawOptions(const QVariantMap& options);
  void newMolecule();
  QJsonObject loadMolecule(const QString& text, const QString& format);
  QJsonObject exportMolecule(const QString& format) const;
  void markSaved();
  void deleteSelection();
  void undo();
  void redo();
  void resetCamera();
signals:
  void stateChanged();

private:
  void replaceMolecule(std::unique_ptr<QtGui::Molecule> molecule);
  QtOpenGL::GLWidget& m_view;
  std::unique_ptr<QtGui::Molecule> m_molecule;
  std::unique_ptr<QtGui::LayerModel> m_layers;
  QMap<QString, QtGui::ToolPlugin*> m_tools;
  QString m_tool = QStringLiteral("draw");
};
} // namespace Web
} // namespace Avogadro
#endif
