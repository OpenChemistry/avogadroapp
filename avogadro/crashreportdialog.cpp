/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#include "crashreportdialog.h"

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

namespace Avogadro {

CrashReportDialog::CrashReportDialog(bool firstRun, QWidget* parent)
  : QDialog(parent)
  , m_firstRun(firstRun)
  , m_sendReports(nullptr)
{
  setWindowTitle(tr("Crash Reporting"));
  setMinimumWidth(480);

  auto* layout = new QVBoxLayout(this);

  auto* intro = new QLabel(
    tr("This is a diagnostic build of Avogadro. It can send a report to the "
       "developers when Avogadro crashes, which helps us fix crashes we "
       "cannot reproduce ourselves."),
    this);
  intro->setWordWrap(true);
  layout->addWidget(intro);

  // Be straight about what a minidump is. It is a snapshot of the process,
  // not a curated set of fields, and it cannot be filtered before it is
  // written - so promising anonymity here would not be truthful.
  auto* detail = new QLabel(
    tr("A crash report contains a snapshot of Avogadro's memory at the moment "
       "it failed, along with your operating system, graphics driver and "
       "Avogadro version, and a log of recent activity. That memory snapshot "
       "makes the report useful, but it may incidentally include "
       "fragments of what you were working on, such as part of a file name or "
       "a molecule you had open."),
    this);
  detail->setWordWrap(true);
  layout->addWidget(detail);

  // Only worth saying on first run - in the settings dialog the user is
  // already looking at the place they would be sent to.
  if (m_firstRun) {
    auto* control = new QLabel(
      tr("Nothing is sent unless you agree. You can change this at any time "
         "under Help \342\206\222 Crash Reporting."),
      this);
    control->setWordWrap(true);
    layout->addWidget(control);
  }

  QDialogButtonBox* buttons = nullptr;
  if (m_firstRun) {
    // No checkbox and no default button: first run is a deliberate choice.
    buttons = new QDialogButtonBox(this);
    buttons->addButton(tr("Send Crash Reports"), QDialogButtonBox::AcceptRole);
    buttons->addButton(tr("Don't Send"), QDialogButtonBox::RejectRole);
  } else {
    m_sendReports =
      new QCheckBox(tr("Send crash reports to the Avogadro developers"), this);
    layout->addSpacing(6);
    layout->addWidget(m_sendReports);
    buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  }

  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

bool CrashReportDialog::consentGiven() const
{
  // On first run the answer is carried by which button was pressed, so the
  // caller reads it from the dialog result instead.
  return m_sendReports != nullptr && m_sendReports->isChecked();
}

void CrashReportDialog::setConsentGiven(bool consentGiven)
{
  if (m_sendReports != nullptr)
    m_sendReports->setChecked(consentGiven);
}

} // end namespace Avogadro
