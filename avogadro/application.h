/**********************************************************************
  This source file is part of the Avogadro project.

  Copyright (C) 2018 by Geoffrey R. Hutchison

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file is part of the Avogadro molecular editor project.
  For more information, see <http://avogadro.cc/>
 ***********************************************************************/

#ifndef AVOGADRO_APPLICATION_H
#define AVOGADRO_APPLICATION_H

#include <QtWidgets/QApplication>

namespace Avogadro {

class Application : public QApplication
{
  Q_OBJECT

public:
  Application(int& argc, char** argv);
  bool loadFile(const QString& fileName);

protected:
  bool event(QEvent* event);

private:
};

} // end namespace Avogadro
#endif
