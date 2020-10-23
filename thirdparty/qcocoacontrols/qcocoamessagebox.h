#ifndef QCocoaMessageBox_h__
#define QCocoaMessageBox_h__

#include <QtWidgets/QMessageBox>

class QCocoaMessageBox : public QMessageBox
{
  Q_OBJECT

public:
  using QMessageBox::QMessageBox;

  bool isCheckBoxChecked() const;

#ifdef Q_OS_MAC
  int exec() override;

  static QMessageBox::StandardButton critical(
    QWidget* parent, const QString& title, const QString& text,
    QMessageBox::StandardButtons buttons = QMessageBox::Ok,
    QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);
  static QMessageBox::StandardButton information(
    QWidget* parent, const QString& title, const QString& text,
    QMessageBox::StandardButtons buttons = QMessageBox::Ok,
    QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);
  static QMessageBox::StandardButton question(
    QWidget* parent, const QString& title, const QString& text,
    QMessageBox::StandardButtons buttons =
      QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No),
    QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);
  static QMessageBox::StandardButton warning(
    QWidget* parent, const QString& title, const QString& text,
    QMessageBox::StandardButtons buttons = QMessageBox::Ok,
    QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

#endif

private:
};

#endif // QCocoaMessageBox_h__
