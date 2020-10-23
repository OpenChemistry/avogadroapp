#include "qcocoamessagebox.h"

#include <QtCore/QEventLoop>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QCheckBox>

#import <AppKit/AppKit.h>

int processMessageBoxResult(NSAlert* alert, NSInteger retCode,
                            QCocoaMessageBox* pMsgBox);

// CocoaMessageBoxHandler

@interface CocoaMessageBoxHandler : NSObject {
}
- (id)initWithLoop:(QEventLoop*)lp;
@property (readonly, nonatomic, nonnull) QEventLoop* loop;
@end

@implementation CocoaMessageBoxHandler

@synthesize loop;

- (id)initWithLoop:(QEventLoop*)lp {
  self = [super init];

  loop = lp;

  return self;
}

- (void)alertDidEnd:(NSAlert*)alert
         returnCode:(NSInteger)returnCode
        contextInfo:(void*)contextInfo {
  Q_UNUSED(contextInfo);
  Q_UNUSED(alert);

  [self loop] -> exit(static_cast<int>(returnCode));
}

@end

// end of CocoaMessageBoxHandler

bool QCocoaMessageBox::isCheckBoxChecked() const
{
  return checkBox() && checkBox()->isChecked();
}

int QCocoaMessageBox::exec()
{
  QMacAutoReleasePool pool;

  NSAlert* alert = [[[NSAlert alloc] init] autorelease];

  QString txt = text();
  QString infoText = informativeText();

  if (txt.size())
    [alert setMessageText:txt.toNSString()];

  if (infoText.size())
    [alert setInformativeText:infoText.toNSString()];

  if (icon() == QMessageBox::Critical)
    alert.alertStyle = NSAlertStyleCritical;
  else if (icon() == QMessageBox::Warning)
    alert.alertStyle = NSAlertStyleWarning;

  QCheckBox* checkBtn = checkBox();

  if (checkBtn) {
    [alert setShowsSuppressionButton:YES];

    // use small checkbox, it's prettier
    NSCell* cell = [[alert suppressionButton] cell];
    [cell setControlSize:NSControlSizeSmall];
    [cell setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];

    if (checkBtn->isChecked())
      [cell setState:NSOnState];

    // если не указывать текст QCheckBox то будет дефолтный от macOS

    if (checkBtn->text().size())
      [cell setTitle:checkBtn->text().toNSString()];
  }

  for (QAbstractButton* btn : buttons()) {
    QMessageBox::ButtonRole role = buttonRole(btn);

    NSButton* addedButton = [alert addButtonWithTitle:btn->text().toNSString()];

    if (role == QMessageBox::RejectRole)
      [addedButton
        setKeyEquivalent:[NSString stringWithFormat:@"%C", 0x1b]]; // Escape key
  }

  NSModalResponse retCode = 0;

  if (QObject::parent()) {
    QEventLoop loop;

    CocoaMessageBoxHandler* handler =
      [[CocoaMessageBoxHandler alloc] initWithLoop:&loop];

    QWidget* parent = static_cast<QWidget*>(QObject::parent());
    NSView* view = reinterpret_cast<NSView*>(parent->winId());
    NSWindow* wnd = [view window];

    [alert beginSheetModalForWindow:wnd
                      modalDelegate:handler
                     didEndSelector:@selector(alertDidEnd:
                                               returnCode:contextInfo:)
                        contextInfo:nil];

    retCode = loop.exec();

    [handler release];
  } else
    retCode = [alert runModal];

  return processMessageBoxResult(alert, retCode, this);
}

int processMessageBoxResult(NSAlert* alert, NSInteger retCode,
                            QCocoaMessageBox* pMsgBox)
{
  int nReturnCode = QMessageBox::NoButton;

  QCheckBox* checkBtn = pMsgBox->checkBox();

  if (checkBtn && [[alert suppressionButton] state] == NSOnState)
    checkBtn->setChecked(true);

  QAbstractButton* btn = nullptr;

  int nButtonIndex = static_cast<int>(retCode) - NSAlertFirstButtonReturn;

  if (nButtonIndex < pMsgBox->buttons().size() && nButtonIndex >= 0)
    btn = pMsgBox->buttons().at(nButtonIndex);

  if (btn) {
    if (pMsgBox->button(QMessageBox::Ok) == btn)
      nReturnCode = QMessageBox::Ok;
    else if (pMsgBox->button(QMessageBox::Open) == btn)
      nReturnCode = QMessageBox::Open;
    else if (pMsgBox->button(QMessageBox::Save) == btn)
      nReturnCode = QMessageBox::Save;
    else if (pMsgBox->button(QMessageBox::Cancel) == btn)
      nReturnCode = QMessageBox::Cancel;
    else if (pMsgBox->button(QMessageBox::Close) == btn)
      nReturnCode = QMessageBox::Close;
    else if (pMsgBox->button(QMessageBox::Discard) == btn)
      nReturnCode = QMessageBox::Discard;
    else if (pMsgBox->button(QMessageBox::Apply) == btn)
      nReturnCode = QMessageBox::Apply;
    else if (pMsgBox->button(QMessageBox::Reset) == btn)
      nReturnCode = QMessageBox::Reset;
    else if (pMsgBox->button(QMessageBox::RestoreDefaults) == btn)
      nReturnCode = QMessageBox::RestoreDefaults;
    else if (pMsgBox->button(QMessageBox::Help) == btn)
      nReturnCode = QMessageBox::Help;
    else if (pMsgBox->button(QMessageBox::SaveAll) == btn)
      nReturnCode = QMessageBox::SaveAll;
    else if (pMsgBox->button(QMessageBox::Yes) == btn)
      nReturnCode = QMessageBox::Yes;
    else if (pMsgBox->button(QMessageBox::YesToAll) == btn)
      nReturnCode = QMessageBox::YesToAll;
    else if (pMsgBox->button(QMessageBox::No) == btn)
      nReturnCode = QMessageBox::No;
    else if (pMsgBox->button(QMessageBox::NoToAll) == btn)
      nReturnCode = QMessageBox::NoToAll;
    else if (pMsgBox->button(QMessageBox::Abort) == btn)
      nReturnCode = QMessageBox::Abort;
    else if (pMsgBox->button(QMessageBox::Retry) == btn)
      nReturnCode = QMessageBox::Retry;
    else if (pMsgBox->button(QMessageBox::Ignore) == btn)
      nReturnCode = QMessageBox::Ignore;
    else if (pMsgBox->button(QMessageBox::Ignore) == btn)
      nReturnCode = QMessageBox::Ignore;
    else {
      // if the message box uses standard buttons, we need to return
      // QMessageBox::StandardButton otherwise, the user must determine which
      // button was clicked via the QMessageBox::clickedButton method

      btn->click();
    }
  }

  return nReturnCode;
}

QMessageBox::StandardButton QCocoaMessageBox::critical(
  QWidget* parent, const QString& title, const QString& text,
  QMessageBox::StandardButtons buttons,
  QMessageBox::StandardButton defaultButton)
{
  QCocoaMessageBox msgBox(parent);

  msgBox.setIcon(QMessageBox::Critical);
  msgBox.setWindowTitle(title);
  msgBox.setText(text);
  msgBox.setStandardButtons(buttons);
  msgBox.setDefaultButton(defaultButton);

  return (QMessageBox::StandardButton)msgBox.exec();
}

QMessageBox::StandardButton QCocoaMessageBox::information(
  QWidget* parent, const QString& title, const QString& text,
  QMessageBox::StandardButtons buttons,
  QMessageBox::StandardButton defaultButton)
{
  QCocoaMessageBox msgBox(parent);

  msgBox.setIcon(QMessageBox::Information);
  msgBox.setWindowTitle(title);
  msgBox.setText(text);
  msgBox.setStandardButtons(buttons);
  msgBox.setDefaultButton(defaultButton);

  return (QMessageBox::StandardButton)msgBox.exec();
}

QMessageBox::StandardButton QCocoaMessageBox::question(
  QWidget* parent, const QString& title, const QString& text,
  QMessageBox::StandardButtons buttons,
  QMessageBox::StandardButton defaultButton)
{
  QCocoaMessageBox msgBox(parent);

  msgBox.setIcon(QMessageBox::Question);
  msgBox.setWindowTitle(title);
  msgBox.setText(text);
  msgBox.setStandardButtons(buttons);
  msgBox.setDefaultButton(defaultButton);

  return (QMessageBox::StandardButton)msgBox.exec();
}

QMessageBox::StandardButton QCocoaMessageBox::warning(
  QWidget* parent, const QString& title, const QString& text,
  QMessageBox::StandardButtons buttons,
  QMessageBox::StandardButton defaultButton)
{
  QCocoaMessageBox msgBox(parent);

  msgBox.setIcon(QMessageBox::Warning);
  msgBox.setWindowTitle(title);
  msgBox.setText(text);
  msgBox.setStandardButtons(buttons);
  msgBox.setDefaultButton(defaultButton);

  return (QMessageBox::StandardButton)msgBox.exec();
}
