/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#ifndef AVOGADRO_VIEWFACTORY_H
#define AVOGADRO_VIEWFACTORY_H

#include <avogadro/qtgui/viewfactory.h>
#include <avogadro/qtopengl/glwidget.h>
#ifdef AVO_USE_VTK
#include <avogadro/vtk/vtkglwidget.h>
#endif

namespace Avogadro {

class ViewFactory : public QtGui::ViewFactory
{
public:
  ViewFactory();
  ~ViewFactory();

  QStringList views() const;
  QWidget* createView(const QString& view);
  void setGLWidget(QtOpenGL::GLWidget* glWidget) { m_glWidget = glWidget; }

protected:
  QtOpenGL::GLWidget* m_glWidget = nullptr;
};

} // End namespace Avogadro

#endif // AVOGADRO_AVOGADROVIEWFACTORY_H
