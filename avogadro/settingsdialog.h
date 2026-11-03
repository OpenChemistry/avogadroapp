/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#include <QtWidgets/QDialog>

namespace Ui {
class SettingsDialog;
}

namespace Avogadro {

class SettingsDialog : public QDialog
{
  Q_OBJECT

public:
  SettingsDialog(QWidget* Parent);
  ~SettingsDialog() override;

private:
  Ui::SettingsDialog* m_ui;
};

} // End namespace Avogadro
