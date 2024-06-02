/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#ifndef AVOGADRO_RENDERINGDIALOG_H
#define AVOGADRO_RENDERINGDIALOG_H

#include <QtWidgets/QDialog>

#include <avogadro/rendering/solidpipeline.h>

namespace Ui {
class RenderingDialog;
}

namespace Avogadro {

using Rendering::SolidPipeline;

class RenderingDialog : public QDialog
{
  Q_OBJECT

public:
  RenderingDialog(QWidget *parent, SolidPipeline &pipeline);
  ~RenderingDialog() override;

  bool aoEnabled();
  float aoStrength();
  bool fogEnabled();
  bool edEnabled();

protected slots:
  void aoEnableCheckBoxChanged(int state);
  void fogEnableCheckBoxChanged(int state);
  void saveButtonClicked();
  void closeButtonClicked();

private:
  Ui::RenderingDialog *m_ui;
  SolidPipeline &m_solidPipeline;
};

} // End namespace Avogadro

#endif // AVOGADRO_RENDERINGDIALOG_H
