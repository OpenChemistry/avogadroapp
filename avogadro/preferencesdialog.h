/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#include <QtWidgets/QDialog>

namespace Ui {
class PreferencesDialog;
}

namespace Avogadro {

class PreferencesDialog : public QDialog
{
  Q_OBJECT

public:
  PreferencesDialog(QWidget* Parent);
  ~PreferencesDialog() override;

private:
  Ui::PreferencesDialog* m_ui;
};

} // End namespace Avogadro
