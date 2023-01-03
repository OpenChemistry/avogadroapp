/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#include "aboutdialog.h"
#include "avogadroappconfig.h"
#include "ui_aboutdialog.h"

#include <QSslSocket>

#include <avogadro/core/version.h>

namespace Avogadro {

AboutDialog::AboutDialog(QWidget* parent_)
  : QDialog(parent_)
  , m_ui(new Ui::AboutDialog)
{
  m_ui->setupUi(this);

  QString html("<html><head/><body><p>"
               "<span style=\" font-size:%1pt; font-weight:600;\">%2</span>"
               "</p></body></html>");

  // Add the labels
  m_ui->versionLabel->setText(html.arg("20").arg(tr("Version:")));
  m_ui->libsLabel->setText(html.arg("10").arg(tr("Avogadro Library Version:")));
  m_ui->qtVersionLabel->setText(html.arg("10").arg(tr("Qt Version:")));
  m_ui->sslVersionLabel->setText(html.arg("10").arg(tr("SSL Version:")));

  // Add the version numbers
  m_ui->version->setText(html.arg("20").arg(AvogadroApp_VERSION));
  m_ui->libsVersion->setText(html.arg("10").arg(version()));
  m_ui->qtVersion->setText(html.arg("10").arg(qVersion()));
  m_ui->sslVersion->setText(
    html.arg("10").arg(QSslSocket::sslLibraryVersionString()));

#ifdef TDX_INTEGRATION
  m_ui->tdxLabel->setText(QString::fromLocal8Bit("3D input device development tools and related technology are provided under license from 3Dconnexion. © 3Dconnexion 1992 - 2022. All rights reserved."));
  m_ui->tdxLabel->setWordWrap(true);
#else
  m_ui->tdxLabel->hide();
#endif

  // Add support for a 2x replacement (mainly Mac OS X retina at this point).
  if (window()->devicePixelRatio() == 2) {
    QPixmap pix(":/icons/Avogadro2_About@2x.png");
    pix.setDevicePixelRatio(2);
    m_ui->Image->setPixmap(pix);
  }
}

AboutDialog::~AboutDialog()
{
  delete m_ui;
}

} /* namespace Avogadro */
