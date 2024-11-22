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

ViewFactory::ViewFactory() {}

ViewFactory::~ViewFactory() {}

QStringList ViewFactory::views() const
{
  QStringList views;
  views << QObject::tr("3D View");
#ifdef AVO_USE_VTK
  views << QObject::tr("VTK");
#endif
  return views;
}

QWidget* ViewFactory::createView(const QString& view)
{
  if (view == QObject::tr("3D View")) {
    // get the background color, etc.
    if (m_glWidget != nullptr) {
      auto newWidget = new QtOpenGL::GLWidget(m_glWidget);
      newWidget->setMolecule(m_glWidget->molecule());
      // set the background color, etc.
      auto bgColor = m_glWidget->renderer().scene().backgroundColor();
      newWidget->renderer().scene().setBackgroundColor(bgColor);
      return newWidget;
    } else
      return new QtOpenGL::GLWidget;
  }
#ifdef AVO_USE_VTK
  else if (view == QObject::tr("VTK"))
    return new VTK::vtkGLWidget;
#endif
  return nullptr;
}

} // End Avogadro namespace
