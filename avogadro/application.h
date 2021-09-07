/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

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
