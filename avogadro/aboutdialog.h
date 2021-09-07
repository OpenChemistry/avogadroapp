/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#ifndef AVOGADRO_ABOUTDIALOG_H
#define AVOGADRO_ABOUTDIALOG_H

#include <QtWidgets/QDialog>

namespace Ui {
class AboutDialog;
}

namespace Avogadro {

class AboutDialog : public QDialog
{
  Q_OBJECT
public:
  AboutDialog(QWidget* Parent);
  ~AboutDialog();

private:
  Ui::AboutDialog* m_ui;
};

} // End Avogadro namespace

#endif // AVOGADRO_ABOUTDIALOG_H
