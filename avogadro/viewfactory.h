/******************************************************************************

  This source file is part of the Avogadro project.

  Copyright 2014 Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/

#ifndef AVOGADRO_VIEWFACTORY_H
#define AVOGADRO_VIEWFACTORY_H

#include <avogadro/qtgui/viewfactory.h>

namespace Avogadro {

class ViewFactory : public QtGui::ViewFactory
{
public:
  ViewFactory();
  ~ViewFactory();

  QStringList views() const;
  QWidget* createView(const QString& view);
};

} // End namespace Avogadro

#endif // AVOGADRO_AVOGADROVIEWFACTORY_H
