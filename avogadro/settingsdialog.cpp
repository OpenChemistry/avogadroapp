/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#include "settingsdialog.h"
#include "ui_settingsdialog.h"

namespace Avogadro {

SettingsDialog::SettingsDialog(QWidget* parent_)
  : QDialog(parent_)
  , m_ui(new Ui::SettingsDialog)
{
  m_ui->setupUi(this);
}

SettingsDialog::~SettingsDialog()
{
  delete m_ui;
}

} // End namespace Avogadro
