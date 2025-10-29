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

  // description text
  // mostly to get picked up for the installer
  // and Linux .desktop file
  QString description(tr("Molecular editor and visualizer"));

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

  // check for light or dark mode
  const QPalette defaultPalette;
  // is the text lighter than the window color?
  bool darkMode = (defaultPalette.color(QPalette::WindowText).lightness() >
                   defaultPalette.color(QPalette::Window).lightness());
  QString theme = darkMode ? "dark" : "light";
  loadImage(theme);
}

void AboutDialog::loadImage(const QString& theme)
{
  QString pixels = window()->devicePixelRatio() == 2 ? "@2x" : "";

  QString path(":/icons/Avogadro2-about-" + theme + pixels + ".png");
  QPixmap pix(path);

  if (window()->devicePixelRatio() == 2)
    pix.setDevicePixelRatio(2);

  m_ui->Image->setPixmap(QPixmap(path));
}

void AboutDialog::changeEvent(QEvent* e)
{
  // it's supposed to be through a theme change
  // but on macOS, it seems to be triggered
  // by a palette change, so we handle both
  if (e->type() == QEvent::ApplicationPaletteChange ||
      e->type() == QEvent::PaletteChange || e->type() == QEvent::ThemeChange) {
    e->accept();

    const QPalette defaultPalette;
    // is the text lighter than the window color?
    bool darkMode = (defaultPalette.color(QPalette::WindowText).lightness() >
                     defaultPalette.color(QPalette::Window).lightness());

    QString theme = darkMode ? "dark" : "light";
    loadImage(theme);
  }
}

AboutDialog::~AboutDialog()
{
  delete m_ui;
}

} /* namespace Avogadro */
