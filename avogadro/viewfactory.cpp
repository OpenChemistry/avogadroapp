/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#include "viewfactory.h"

#include <avogadro/qtopengl/glwidget.h>
#ifdef AVO_USE_VTK
#include <avogadro/vtk/vtkglwidget.h>
#endif

namespace Avogadro {

ViewFactory::ViewFactory()
{
}

ViewFactory::~ViewFactory()
{
}

QStringList ViewFactory::views() const
{
  QStringList views;
  views << tr("3D View");
#ifdef AVO_USE_VTK
  views << tr("VTK");
#endif
  return views;
}

QWidget* ViewFactory::createView(const QString& view)
{
  if (view == tr("3D View"))
    return new QtOpenGL::GLWidget;
#ifdef AVO_USE_VTK
  else if (view == tr("VTK"))
    return new VTK::vtkGLWidget;
#endif
  return nullptr;
}

} // End Avogadro namespace
