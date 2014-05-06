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

#include "viewfactory.h"

#include <avogadro/qtopengl/glwidget.h>
#include <avogadro/qtopengl/editglwidget.h>
#include <avogadro/vtk/vtkglwidget.h>

namespace Avogadro {

ViewFactory::ViewFactory()
{
}

ViewFactory::~ViewFactory()
{
}

QStringList ViewFactory::views() const
{
  return QStringList() << "3D View" << "3D Editor" << "VTK";
}

QWidget * ViewFactory::createView(const QString &view)
{
  if (view == "3D View")
    return new QtOpenGL::GLWidget;
  else if (view == "3D Editor")
    return new QtOpenGL::EditGLWidget;
  else if (view == "VTK")
    return new VTK::vtkGLWidget;
  return NULL;
}

} // End Avogadro namespace
