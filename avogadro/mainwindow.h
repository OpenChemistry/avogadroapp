#ifndef AVOGADRO_MAINWINDOW_H
#define AVOGADRO_MAINWINDOW_H

#include <QtGui/QMainWindow>
#include <QtCore/QString>

namespace Avogadro {

class MainWindow : public QMainWindow
{
Q_OBJECT
public:
  MainWindow(const QString &filename = QString());
  ~MainWindow();
};

} // End Avogadro namespace

#endif
