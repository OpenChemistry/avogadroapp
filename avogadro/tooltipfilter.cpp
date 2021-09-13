/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#include "tooltipfilter.h"

#include <QtCore/QEnterEvent>
#include <QtCore/QEvent>
#include <QtWidgets/QToolTip>
#include <QtWidgets/QWidget>

ToolTipFilter::ToolTipFilter(QObject* parent)
  : QObject(parent)
{}

bool ToolTipFilter::eventFilter(QObject* object, QEvent* event)
{
  // Fire off a toolTip item for an enter event
  if (event->type() == QEvent::Enter) {
    QWidget* target = qobject_cast<QWidget*>(object);
    QEnterEvent* ee = dynamic_cast<QEnterEvent*>(event);
    if (target && ee) {
      QToolTip::showText(ee->globalPos(), target->toolTip(), target);
      return true;
    }
  }

  return false;
}
