// SPDX-License-Identifier: BSD-3-Clause
#include "webcontroller.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QUndoStack>
#include <avogadro/core/atom.h>
#include <avogadro/core/bond.h>
#include <avogadro/core/layermanager.h>
#include <avogadro/qtgui/molecule.h>
#include <avogadro/qtgui/rwmolecule.h>
#include <avogadro/qtopengl/glwidget.h>
#include <avogadro/qtplugins/pluginmanager.h>
#include <gtest/gtest.h>

using namespace Avogadro;
class WebEditorTest : public testing::Test
{
protected:
  QtOpenGL::GLWidget view;
  Web::WebController editor{ view };
  QtGui::ToolPlugin* draw()
  {
    editor.setTool("draw");
    return view.activeTool();
  }
  void clickAtom()
  {
    // Configure the CPU camera directly: this test intentionally has no GL
    // context.
    view.renderer().camera().setViewport(640, 480);
    view.renderer().camera().calculatePerspective(40.0f, 0.1f, 100.0f);
    auto* tool = draw();
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(320, 240),
                      QPointF(320, 240), Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(320, 240),
                        QPointF(320, 240), Qt::LeftButton, Qt::NoButton,
                        Qt::NoModifier);
    tool->mousePressEvent(&press);
    tool->mouseReleaseEvent(&release);
  }
};

TEST_F(WebEditorTest, DefaultsAndWidgetIndependentOptions)
{
  auto* tool = draw();
  EXPECT_EQ(editor.state()["tool"].toString(), "draw");
  EXPECT_EQ(tool->property("drawOptions").toMap()["atomicNumber"].toInt(), 6);
  for (auto* widget : QApplication::allWidgets())
    EXPECT_STRNE(widget->metaObject()->className(),
                 "Avogadro::QtPlugins::EditorToolWidget");
  QSignalSpy changes(&editor, &Web::WebController::stateChanged);
  EXPECT_TRUE(editor.setDrawOptions({ { "atomicNumber", 8 },
                                      { "bondOrder", 2 },
                                      { "adjustHydrogens", false } }));
  EXPECT_FALSE(
    editor.setDrawOptions({ { "atomicNumber", 119 }, { "bondOrder", 1 } }));
  EXPECT_EQ(tool->property("drawOptions").toMap()["bondOrder"].toInt(), 2);
  EXPECT_FALSE(changes.isEmpty());
  EXPECT_FALSE(editor.setTool("unknown"));
  EXPECT_EQ(QtPlugins::PluginManager::instance()
              ->pluginFactory<QtGui::ToolPluginFactory>("missing-tool"),
            nullptr);
  EXPECT_EQ(editor.state()["tool"].toString(), "draw");
  auto* panel = tool->toolWidget();
  EXPECT_EQ(panel->findChild<QComboBox*>("bondOrder")->currentIndex(), 2);
  EXPECT_FALSE(panel->findChild<QCheckBox*>("adjustHydrogens")->isChecked());
  panel->findChild<QComboBox*>("bondOrder")->setCurrentIndex(3);
  EXPECT_EQ(tool->property("drawOptions").toMap()["bondOrder"].toInt(), 3);
  EXPECT_TRUE(editor.setDrawOptions({ { "bondOrder", 1 } }));
  EXPECT_EQ(panel->findChild<QComboBox*>("bondOrder")->currentIndex(), 1);
  delete panel; // The desktop may destroy the panel before the tool.
  EXPECT_EQ(
    tool->toolWidget()->findChild<QComboBox*>("bondOrder")->currentIndex(), 1);
}

TEST_F(WebEditorTest, DrawingHydrogensAndGroupedUndo)
{
  clickAtom();
  EXPECT_EQ(editor.molecule().atomCount(), 5); // existing carbon + H adjustment
  EXPECT_EQ(editor.molecule().bondCount(), 4);
  EXPECT_TRUE(editor.state()["modified"].toBool());
  editor.undo();
  EXPECT_EQ(editor.molecule().atomCount(), 0);
  EXPECT_FALSE(editor.state()["modified"].toBool());
  editor.redo();
  EXPECT_EQ(editor.molecule().atomCount(), 5);
  editor.newMolecule();
  editor.setDrawOptions(
    { { "atomicNumber", 8 }, { "adjustHydrogens", false } });
  clickAtom();
  EXPECT_EQ(editor.molecule().atomCount(), 1);
  EXPECT_EQ(editor.molecule().atom(0).atomicNumber(), 8);
}

TEST_F(WebEditorTest, RoundTripsAndFirstRecord)
{
  ASSERT_TRUE(
    editor.loadMolecule("2\nHydrogen\nH 0 0 0\nH 0 0 0.74\n", "xyz")["ok"]
      .toBool());
  for (const QString format : { "cjson", "mol", "sdf", "xyz" }) {
    auto result = editor.exportMolecule(format);
    ASSERT_TRUE(result["ok"].toBool())
      << result["error"].toString().toStdString();
    auto loaded = editor.loadMolecule(result["text"].toString(), format);
    ASSERT_TRUE(loaded["ok"].toBool())
      << loaded["error"].toString().toStdString();
    EXPECT_EQ(editor.molecule().atomCount(), 2);
    EXPECT_NEAR(editor.molecule().atom(1).position3d().z(), .74, .0001);
  }
  const auto sdf = editor.exportMolecule("sdf")["text"].toString();
  auto result = editor.loadMolecule(sdf + sdf, "sdf");
  EXPECT_TRUE(result["ok"].toBool());
  EXPECT_FALSE(result["notice"].toString().isEmpty());
  EXPECT_FALSE(
    editor.loadMolecule(sdf + sdf, "mol")["notice"].toString().isEmpty());
  result =
    editor.loadMolecule("1\nfirst\nO 0 0 0\n1\nsecond\nN 1 1 1\n", "xyz");
  ASSERT_TRUE(result["ok"].toBool());
  EXPECT_FALSE(result["notice"].toString().isEmpty());
  EXPECT_EQ(editor.molecule().atomCount(), 1);
  EXPECT_EQ(editor.molecule().atom(0).atomicNumber(), 8);
}

TEST_F(WebEditorTest, BondOrdersHydrogensAndUndoGrouping)
{
  for (int order : { 1, 2, 3 }) {
    editor.newMolecule();
    ASSERT_TRUE(editor.setDrawOptions({ { "atomicNumber", 6 },
                                        { "bondOrder", order },
                                        { "adjustHydrogens", true } }));
    // Configure the CPU camera directly: this test intentionally has no GL
    // context.
    view.renderer().camera().setViewport(640, 480);
    view.renderer().camera().calculatePerspective(40.0f, 0.1f, 100.0f);
    auto* tool = draw();
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(320, 240),
                      QPointF(320, 240), Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QMouseEvent move(QEvent::MouseMove, QPointF(400, 240), QPointF(400, 240),
                     Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(400, 240),
                        QPointF(400, 240), Qt::LeftButton, Qt::NoButton,
                        Qt::NoModifier);
    tool->mousePressEvent(&press);
    tool->mouseMoveEvent(&move);
    tool->mouseReleaseEvent(&release);
    EXPECT_EQ(editor.molecule().atomCount(), 10 - 2 * order);
    int carbonBonds = 0;
    for (Index i = 0; i < editor.molecule().bondCount(); ++i) {
      const auto bond = editor.molecule().bond(i);
      if (bond.atom1().atomicNumber() == 6 &&
          bond.atom2().atomicNumber() == 6) {
        ++carbonBonds;
        EXPECT_EQ(bond.order(), order);
      }
    }
    EXPECT_EQ(carbonBonds, 1);
    editor.undo();
    EXPECT_EQ(editor.molecule().atomCount(), 0);
    editor.redo();
    EXPECT_EQ(editor.molecule().atomCount(), 10 - 2 * order);
  }
}

TEST_F(WebEditorTest, FailedImportsPreserveDocumentAndUndo)
{
  editor.setDrawOptions({ { "adjustHydrogens", false } });
  clickAtom();
  const QJsonValue before = editor.exportMolecule("cjson")["text"];
  for (const QString format : { "cjson", "mol", "sdf", "xyz", "smi" }) {
    EXPECT_FALSE(editor.loadMolecule("not a molecule", format)["ok"].toBool());
    EXPECT_EQ(editor.exportMolecule("cjson")["text"], before);
    EXPECT_TRUE(editor.state()["canUndo"].toBool());
  }
  editor.undo();
  EXPECT_EQ(editor.molecule().atomCount(), 0);
}

TEST_F(WebEditorTest, DeleteAndSaveState)
{
  editor.setDrawOptions({ { "adjustHydrogens", false } });
  clickAtom();
  editor.markSaved();
  EXPECT_FALSE(editor.state()["modified"].toBool());
  editor.molecule().atom(0).setSelected(true);
  editor.deleteSelection();
  EXPECT_EQ(editor.molecule().atomCount(), 0);
  editor.undo();
  EXPECT_EQ(editor.molecule().atomCount(), 1);
  EXPECT_FALSE(editor.state()["modified"].toBool());
  editor.newMolecule();
  EXPECT_FALSE(editor.state()["canUndo"].toBool());
}

TEST_F(WebEditorTest, ReplacingDocumentReleasesLayerState)
{
  for (int i = 0; i < 20; ++i) {
    clickAtom();
    std::weak_ptr<Core::MoleculeInfo> layers =
      Core::LayerManager::getMoleculeInfo(&editor.molecule());
    editor.newMolecule();
    EXPECT_TRUE(layers.expired());
    EXPECT_EQ(editor.molecule().atomCount(), 0);
  }
}
int main(int argc, char** argv)
{
  QApplication app(argc, argv);
  app.setApplicationName("AvogadroWebTests");
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
