/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#include "renderingdialog.h"
#include "ui_renderingdialog.h"

namespace Avogadro {

RenderingDialog::RenderingDialog(QWidget *parent_, SolidPipeline &pipeline)
  : QDialog(parent_), m_ui(new Ui::RenderingDialog), m_solidPipeline(pipeline)
{
  m_ui->setupUi(this);

  m_ui->aoEnableCheckBox->setCheckState(pipeline.getAoEnabled()? Qt::Checked : Qt::Unchecked);
  m_ui->dofEnableCheckBox->setCheckState(pipeline.getDofEnabled()? Qt::Checked : Qt::Unchecked);
  m_ui->fogEnableCheckBox->setCheckState(pipeline.getFogEnabled()? Qt::Checked : Qt::Unchecked);  
  m_ui->aoStrengthDoubleSpinBox->setMinimum(0.0);
  m_ui->aoStrengthDoubleSpinBox->setValue(pipeline.getAoStrength());
  m_ui->aoStrengthDoubleSpinBox->setMaximum(2.0);
  m_ui->aoStrengthDoubleSpinBox->setDecimals(1);
  m_ui->aoStrengthDoubleSpinBox->setSingleStep(0.1);
  m_ui->dofStrengthDoubleSpinBox->setMinimum(0.0);
  m_ui->dofStrengthDoubleSpinBox->setValue(pipeline.getDofStrength());
  m_ui->dofStrengthDoubleSpinBox->setMaximum(2.0);
  m_ui->dofStrengthDoubleSpinBox->setDecimals(1);
  m_ui->dofStrengthDoubleSpinBox->setSingleStep(0.1);
  m_ui->dofPositionDoubleSpinBox->setMinimum(0.0);
  m_ui->dofPositionDoubleSpinBox->setValue(pipeline.getDofPosition());
  // We can adjust the max and min value 
  // after testing with several molecules
  m_ui->dofPositionDoubleSpinBox->setMaximum(20.0);
  m_ui->dofPositionDoubleSpinBox->setDecimals(1);
  m_ui->dofPositionDoubleSpinBox->setSingleStep(0.1);
  m_ui->fogStrengthDoubleSpinBox->setMinimum(-20.0);
  m_ui->fogStrengthDoubleSpinBox->setValue(pipeline.getFogStrength());
  m_ui->fogStrengthDoubleSpinBox->setMaximum(20.0);
  m_ui->fogStrengthDoubleSpinBox->setDecimals(1);
  m_ui->fogStrengthDoubleSpinBox->setSingleStep(0.1);
  m_ui->fogPositionDoubleSpinBox->setMinimum(-20.0);
  m_ui->fogPositionDoubleSpinBox->setValue(pipeline.getFogPosition());
  m_ui->fogPositionDoubleSpinBox->setMaximum(20.0);
  m_ui->fogPositionDoubleSpinBox->setDecimals(1);
  m_ui->fogPositionDoubleSpinBox->setSingleStep(0.1);
  m_ui->edEnableCheckBox->setCheckState(pipeline.getEdEnabled()? Qt::Checked : Qt::Unchecked);
  
  connect(m_ui->aoEnableCheckBox, SIGNAL(stateChanged(int)),
          SLOT(aoEnableCheckBoxChanged(int)));
  connect(m_ui->dofEnableCheckBox, SIGNAL(stateChanged(int)),
          SLOT(dofEnableCheckBoxChanged(int)));
  connect(m_ui->fogEnableCheckBox, SIGNAL(stateChanged(int)),
          SLOT(fogEnableCheckBoxChanged(int)));
  connect(m_ui->saveButton, SIGNAL(clicked()),
          SLOT(saveButtonClicked()));
  connect(m_ui->closeButton, SIGNAL(clicked()),
          SLOT(closeButtonClicked()));
}

RenderingDialog::~RenderingDialog()
{
  delete m_ui;
}

bool RenderingDialog::aoEnabled()
{
  return m_ui->aoEnableCheckBox->checkState() == Qt::Checked;
}

bool RenderingDialog::dofEnabled()
{
  return m_ui->dofEnableCheckBox->checkState() == Qt::Checked;
}
bool RenderingDialog::fogEnabled()
{
  return m_ui->fogEnableCheckBox->checkState() == Qt::Checked;
}

float RenderingDialog::aoStrength()
{
  return m_ui->aoStrengthDoubleSpinBox->value();
}

float RenderingDialog::dofStrength()
{
  return m_ui->dofStrengthDoubleSpinBox->value();
}

float RenderingDialog::dofPosition()
{
  return m_ui->dofPositionDoubleSpinBox->value();
}
float RenderingDialog::fogStrength()
{
  return m_ui->fogStrengthDoubleSpinBox->value();
}

float RenderingDialog::fogPosition()
{
  return m_ui->fogPositionDoubleSpinBox->value();
}

bool RenderingDialog::edEnabled()
{
  return m_ui->edEnableCheckBox->checkState() == Qt::Checked;
}


void RenderingDialog::dofEnableCheckBoxChanged(int state)
{
  if (state == Qt::Unchecked){
    m_ui->dofStrengthDoubleSpinBox->setEnabled(false);
    m_ui->dofPositionDoubleSpinBox->setEnabled(false);
  }
  else{
    m_ui->dofStrengthDoubleSpinBox->setEnabled(true);
    m_ui->dofPositionDoubleSpinBox->setEnabled(true);
  }
}

void RenderingDialog::fogEnableCheckBoxChanged(int state)
{
  if (state == Qt::Unchecked){
    m_ui->fogPositionDoubleSpinBox->setEnabled(false);
    m_ui->fogStrengthDoubleSpinBox->setEnabled(false);
  }
  else{
    m_ui->fogStrengthDoubleSpinBox->setEnabled(true);
    m_ui->fogPositionDoubleSpinBox->setEnabled(true);
  }
}

void RenderingDialog::aoEnableCheckBoxChanged(int state)
{
  if (state == Qt::Unchecked)
    m_ui->aoStrengthDoubleSpinBox->setEnabled(false);
  else
    m_ui->aoStrengthDoubleSpinBox->setEnabled(true);
}

void RenderingDialog::saveButtonClicked()
{
  m_solidPipeline.setAoEnabled(aoEnabled());
  m_solidPipeline.setDofEnabled(dofEnabled());
  m_solidPipeline.setAoStrength(aoStrength());
  m_solidPipeline.setDofStrength(dofStrength());
  m_solidPipeline.setDofPosition(dofPosition());
  m_solidPipeline.setFogStrength(fogStrength());
  m_solidPipeline.setFogEnabled(fogEnabled());
  m_solidPipeline.setFogPosition(fogPosition());
  m_solidPipeline.setEdEnabled(edEnabled());
  this->accept();
}

void RenderingDialog::closeButtonClicked()
{
  this->reject();
}

} /* namespace Avogadro */
