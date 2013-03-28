/******************************************************************************

 This source file is part of the MoleQueue project.

 Copyright 2013 Kitware, Inc.

 This source code is released under the New BSD License, (the "License").

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

 ******************************************************************************/

#ifndef ABOUTDIALOG_H_
#define ABOUTDIALOG_H_

#include <QtGui/QDialog>

namespace Ui {
class AboutDialog;
}

namespace Avogadro
{

class AboutDialog : public QDialog
{
  Q_OBJECT
public:
  AboutDialog(QWidget* Parent);
  ~AboutDialog();

private:
  Ui::AboutDialog *m_ui;
};

} /* namespace Avogadro */

#endif /* ABOUTDIALOG_H_ */
