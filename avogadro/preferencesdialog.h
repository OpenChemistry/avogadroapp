/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#ifndef AVOGADRO_PREFERENCESDIALOG_H
#define AVOGADRO_PREFERENCESDIALOG_H

#include <QtCore/QSettings>
#include <QtWidgets/QDialog>

namespace Ui {
class PreferencesDialog;
}

namespace Avogadro {

class PreferencesDialog : public QDialog
{
  Q_OBJECT

public:
  PreferencesDialog(QWidget* Parent, int initialPageIndex);
  ~PreferencesDialog() override;

protected slots:
  void changePage(int index);

private:
  Ui::PreferencesDialog* m_ui;
  QSettings m_settings;
  QStringList m_availableLocales;

  void setupAll();
  void updateAll();
  void applyAll();

  void setupGeneral();
  void updateGeneral();
  void applyGeneral();

  void updateControls();
  void applyControls();

  void updateView();
  void applyView();

  void updateRendering();
  void applyRendering();

  void updateColors();
  void applyColors();

  void updateAccessibility();
  void applyAccessibility();

  void updateBonding();
  void applyBonding();
};

} // End namespace Avogadro

#endif // AVOGADRO_PREFERENCESDIALOG_H
