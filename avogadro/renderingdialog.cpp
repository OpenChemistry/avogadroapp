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
  m_ui->fogEnableCheckBox->setCheckState(pipeline.getFogEnabled()? Qt::Checked : Qt::Unchecked);  
  m_ui->aoStrengthDoubleSpinBox->setMinimum(0.0);
  m_ui->aoStrengthDoubleSpinBox->setValue(pipeline.getAoStrength());
  m_ui->aoStrengthDoubleSpinBox->setMaximum(2.0);
  m_ui->aoStrengthDoubleSpinBox->setDecimals(1);
  m_ui->aoStrengthDoubleSpinBox->setSingleStep(0.1);
  m_ui->fogStrengthDoubleSpinBox->setMinimum(0.0);
  m_ui->fogStrengthDoubleSpinBox->setValue(pipeline.getFogStrength());
  m_ui->fogStrengthDoubleSpinBox->setMaximum(2.0);
  m_ui->fogStrengthDoubleSpinBox->setDecimals(1);
  m_ui->fogStrengthDoubleSpinBox->setSingleStep(0.1);
  m_ui->edEnableCheckBox->setCheckState(pipeline.getEdEnabled()? Qt::Checked : Qt::Unchecked);
  
  connect(m_ui->aoEnableCheckBox, SIGNAL(stateChanged(int)),
          SLOT(aoEnableCheckBoxChanged(int)));
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

bool RenderingDialog::fogEnabled()
{
  return m_ui->fogEnableCheckBox->checkState() == Qt::Checked;
}

float RenderingDialog::aoStrength()
{
  return m_ui->aoStrengthDoubleSpinBox->value();
}

float RenderingDialog::fogStrength()
{
  return m_ui->fogStrengthDoubleSpinBox->value();
}

bool RenderingDialog::edEnabled()
{
  return m_ui->edEnableCheckBox->checkState() == Qt::Checked;
}

// TODO: will correct it.

void RenderingDialog::fogEnableCheckBoxChanged(int state)
{
  if (state == Qt::Unchecked)
    m_ui->fogStrengthDoubleSpinBox->setEnabled(false);
  else
    m_ui->fogStrengthDoubleSpinBox->setEnabled(true);
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
  m_solidPipeline.setAoStrength(aoStrength());
  m_solidPipeline.setFogStrength(fogStrength());
  m_solidPipeline.setFogEnabled(fogEnabled());
  m_solidPipeline.setEdEnabled(edEnabled());
  this->accept();
}

void RenderingDialog::closeButtonClicked()
{
  this->reject();
}

} /* namespace Avogadro */
