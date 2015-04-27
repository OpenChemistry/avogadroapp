/******************************************************************************

 This source file is part of the Avogadro project.

 Copyright 2013 Kitware, Inc.

 This source code is released under the New BSD License, (the "License").

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

 ******************************************************************************/

#include "aboutdialog.h"
#include "ui_aboutdialog.h"
#include "avogadroappconfig.h"

#include <avogadro/core/version.h>

namespace Avogadro {

AboutDialog::AboutDialog(QWidget* parent_)
  : QDialog(parent_), m_ui(new Ui::AboutDialog)
{
  m_ui->setupUi(this);

  QString html("<html><head/><body><p>" \
               "<span style=\" font-size:%1pt; font-weight:600;\">%2</span>" \
               "</p></body></html>");

  m_ui->version->setText(html.arg("20").arg(AvogadroApp_VERSION));
  m_ui->libsVersion->setText(html.arg("10").arg(version()));
  m_ui->qtVersion->setText(html.arg("10").arg(qVersion()));

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
