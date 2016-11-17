#ifndef AVOGADRO_PQRWIDGET_H
#define AVOGADRO_PQRWIDGET_H

#include <QtWidgets/QDialog>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QTableWidgetItem>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QGraphicsRectItem>
#include <QFile>
#include <QDir>
#include "ImportPQR.h"

namespace Ui {
class PQRWidget;
}

namespace Avogadro {

class PQRWidget : public QDialog
{
    Q_OBJECT

public:
    PQRWidget(QWidget *parent = 0);
    ~PQRWidget();

private slots:
  void searchAction();
	void molSelected(int, int);
	void downloadMol();

private:
    Ui::PQRWidget *ui;
    ImportPQR *request;

};
}
#endif // MAINWINDOW_H
