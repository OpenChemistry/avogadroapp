// SPDX-License-Identifier: BSD-3-Clause
#include "webcontroller.h"
#include <QApplication>
#include <QJsonDocument>
#include <QSurfaceFormat>
#include <QTimer>
#include <avogadro/qtopengl/glwidget.h>
#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>

namespace {
Avogadro::Web::WebController* controller = nullptr;
std::string json(const QJsonObject& object)
{
  return QJsonDocument(object).toJson(QJsonDocument::Compact).toStdString();
}
std::string state()
{
  return json(controller->state());
}
bool setTool(std::string name)
{
  return controller->setTool(QString::fromStdString(name));
}
bool setDrawOptions(std::string options)
{
  const auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(options));
  return doc.isObject() &&
         controller->setDrawOptions(doc.object().toVariantMap());
}
std::string load(std::string text, std::string format)
{
  return json(controller->loadMolecule(QString::fromStdString(text),
                                       QString::fromStdString(format)));
}
std::string save(std::string format)
{
  return json(controller->exportMolecule(QString::fromStdString(format)));
}
void newMolecule()
{
  controller->newMolecule();
}
void deleteSelection()
{
  controller->deleteSelection();
}
void undo()
{
  controller->undo();
}
void redo()
{
  controller->redo();
}
void resetCamera()
{
  controller->resetCamera();
}
void markSaved()
{
  controller->markSaved();
}
}
EMSCRIPTEN_BINDINGS(avogadro_web)
{
  emscripten::function("editorState", &state);
  emscripten::function("setTool", &setTool);
  emscripten::function("setDrawOptions", &setDrawOptions);
  emscripten::function("loadMolecule", &load);
  emscripten::function("exportMolecule", &save);
  emscripten::function("newMolecule", &newMolecule);
  emscripten::function("deleteSelection", &deleteSelection);
  emscripten::function("undo", &undo);
  emscripten::function("redo", &redo);
  emscripten::function("resetCamera", &resetCamera);
  emscripten::function("markSaved", &markSaved);
}
#endif

int main(int argc, char** argv)
{
  QSurfaceFormat format;
#ifdef __EMSCRIPTEN__
  format.setRenderableType(QSurfaceFormat::OpenGLES);
  format.setVersion(3, 0);
#else
  format.setVersion(4, 0);
  format.setProfile(QSurfaceFormat::CoreProfile);
#endif
  format.setDepthBufferSize(24);
  format.setStencilBufferSize(8);
  QSurfaceFormat::setDefaultFormat(format);
  QApplication app(argc, argv);
  app.setApplicationName("Avogadro Web");
  app.setOrganizationName("OpenChemistry");
  Avogadro::QtOpenGL::GLWidget view;
  Avogadro::Web::WebController editor(view);
#ifdef __EMSCRIPTEN__
  controller = &editor;
  // Qt stays on the browser main thread; callbacks run after each completed
  // event, avoiding re-entry into a partially updated molecule or undo stack.
  QObject::connect(
    &editor, &Avogadro::Web::WebController::stateChanged, &editor,
    []() {
      const auto callback =
        emscripten::val::module_property("onEditorStateChanged");
      if (callback.typeOf().as<std::string>() == "function")
        callback(state());
    },
    Qt::QueuedConnection);
  view.setWindowFlags(Qt::FramelessWindowHint);
  view.showFullScreen();
#else
  view.resize(960, 640);
  view.show();
  if (app.arguments().contains("--smoke-test"))
    QTimer::singleShot(2000, &app, [&]() { app.exit(view.isValid() ? 0 : 1); });
#endif
  const int result = app.exec();
#ifdef __EMSCRIPTEN__
  controller = nullptr;
#endif
  return result;
}
