/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#include "preferencesdialog.h"
#include "ui_preferencesdialog.h"

namespace Avogadro {

PreferencesDialog::PreferencesDialog(QWidget* parent_)
  : QDialog(parent_), m_ui(new Ui::PreferencesDialog)
{
  m_ui->setupUi(this);
}

PreferencesDialog::~PreferencesDialog()
{
  delete m_ui;
}

} // End namespace Avogadro
