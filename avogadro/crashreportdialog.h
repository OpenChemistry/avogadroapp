/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#ifndef AVOGADRO_CRASHREPORTDIALOG_H
#define AVOGADRO_CRASHREPORTDIALOG_H

#include <QtWidgets/QDialog>

class QCheckBox;

namespace Avogadro {

/**
 * @class CrashReportDialog crashreportdialog.h
 * @brief Asks the user whether crash reports may be sent.
 *
 * Used in two modes. On first run it presents an explicit choice, with no
 * default and no way to dismiss it without deciding. Afterwards it is reached
 * from Help and shows a checkbox reflecting the current setting.
 *
 * This is only built when the application is configured with USE_SENTRY.
 */
class CrashReportDialog : public QDialog
{
  Q_OBJECT

public:
  explicit CrashReportDialog(bool firstRun, QWidget* parent = nullptr);

  /** @return true if the user wants crash reports sent */
  bool consentGiven() const;

  /** Set the state shown when the dialog opens. Ignored on first run. */
  void setConsentGiven(bool consentGiven);

private:
  bool m_firstRun;
  QCheckBox* m_sendReports;
};

} // end namespace Avogadro

#endif // AVOGADRO_CRASHREPORTDIALOG_H
