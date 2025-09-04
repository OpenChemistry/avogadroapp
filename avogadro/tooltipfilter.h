/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#include <QtCore/QObject>
#include <QEvent>

class ToolTipFilter : public QObject
{
Q_OBJECT

public:
ToolTipFilter(QObject *parent);

bool eventFilter(QObject *object, QEvent *event);
};