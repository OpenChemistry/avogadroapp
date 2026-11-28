/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#include "preferencesdialog.h"
#include "ui_preferencesdialog.h"

#include "i18n.h"

#include <QtWidgets/QDialog>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QWidget>

#include <QtGui/QFontDatabase>

#include <QtCore/QHash>
#include <QtCore/QLatin1StringView>
#include <QtCore/QSettings>
#include <QtCore/QString>

using namespace Qt::StringLiterals;

namespace Avogadro {

PreferencesDialog::PreferencesDialog(QWidget* parent, int initialPageIndex)
  : QDialog(parent), m_ui(new Ui::PreferencesDialog)
{
  m_ui->setupUi(this);

  // Each page may have a corresponding setup method, but it is optional. It
  // should be defined if the page has any widgets that require additional setup
  // that cannot be done in the .ui file (for example, a combobox with a set of
  // options that can only be known discovered at runtime).
  setupAll();

  // Each page should have a corresponding update method that changes the state
  // of the widgets to reflect the state of m_settings. The method should not
  // change the settings in any way while doing this.
  updateAll();

  m_ui->pageList->setCurrentRow(initialPageIndex);
  changePage(initialPageIndex);

  connect(m_ui->pageList, &QListWidget::currentRowChanged, this,
          &PreferencesDialog::changePage);
}

PreferencesDialog::~PreferencesDialog()
{
  delete m_ui;
}

void PreferencesDialog::accept()
{
  // Each page should have a corresponding apply method that changes m_settings
  // to reflect the state of the widgets. The method is then called if the
  // preferences dialog is closed by clicking OK. It is thus never called if the
  // dialog is closed with Cancel.
  applyAll();
  QDialog::accept();
}

void PreferencesDialog::changePage(int index)
{
  m_ui->pageView->setCurrentIndex(index);
}

void PreferencesDialog::setupAll()
{
  setupGeneral();
}

void PreferencesDialog::updateAll()
{
  updateGeneral();
  updateControls();
  updateRendering();
  updateColors();
  updateAccessibility();
  updateBonding();
}

void PreferencesDialog::applyAll()
{
  applyGeneral();
  applyControls();
  applyRendering();
  applyColors();
  applyAccessibility();
  applyBonding();
}

// General
// [locale] = UI lang
// [indexing] = global indexing convention

void PreferencesDialog::setupGeneral()
{
  m_ui->userInterfaceLanguageComboBox->addItem(
    "English (Default)"_L1, // Don't translate as this is the "native name"
    "default"_L1);
  // Populate the language selection box with available translations
  QStringList availableLocales = Avogadro::availableLocales();
  if (!availableLocales.isEmpty()) {
    // This is gated by the availability of translations so that we don't
    // pretend to the user they can have the system language if they can't!
    m_ui->userInterfaceLanguageComboBox->addItem(tr("System Language"),
                                                 "system"_L1);
    // Displaying in the native script causes a long delay if no appropriate
    // font can be found (at least when there are lots of fonts installed).
    // To save Qt loading and trying fonts to see if they work, we will check
    // if the language is even supported, and if not, omit it.
    auto availableWritingSystems = QFontDatabase::writingSystems();
    for (const QString& localeCode : availableLocales) {
      QLocale locale(localeCode);
      auto writingSystem = Avogadro::scriptToWritingSystem(locale.script());
      if (writingSystem && availableWritingSystems.contains(*writingSystem)) {
        QString languageLabel;
        // Some languages Qt just gets plain wrong (e.g. variations with @ are
        // ignored) and we just need to override the whole thing
        const QHash<QString, QString> locale_exceptions = {
          { "ca@valencia"_L1, u"valencià"_s },
        };
        if (locale_exceptions.contains(localeCode)) {
          languageLabel = locale_exceptions.value(localeCode);
        } else {
          // Some Qt locales include the location in the native name for some
          // reason
          const QHash<QString, QString> lang_exceptions = {
            { "es"_L1, u"español"_s },
            { "fr"_L1, u"français"_s },
            { "en"_L1, u"English"_s }
          };
          languageLabel = lang_exceptions.value(localeCode.split('_').first(),
                                                locale.nativeLanguageName());
          // Add the territory to disambiguate if the locale code has it too
          if (localeCode.contains(u'_'))
            languageLabel += " ("_L1 + locale.nativeTerritoryName() + ")"_L1;
        }
        languageLabel += " ["_L1 + localeCode + "]"_L1;
        m_ui->userInterfaceLanguageComboBox->addItem(languageLabel, localeCode);
      } else {
        // No point even offering it to the user, since they can't render it
        qDebug() << "No font support for" << locale << "(should use"
                 << locale.script() << ")";
        continue;
      }
    }
  } // Finished listing translations
}

void PreferencesDialog::updateGeneral()
{
  // Interface language
  // Only bother if there are even translations available
  if (m_ui->userInterfaceLanguageComboBox->count() > 1) {
    // We made the locale string the associated data for each item
    int index = m_ui->userInterfaceLanguageComboBox->findData(
      // No explicit setting in the config indicates to use the system language
      m_settings.value("locale"_L1, "system"_L1).toString());
    // If the language specified in the config isn't available, system is used
    index = (index != -1) ? index : 1;
    m_ui->userInterfaceLanguageComboBox->setCurrentIndex(index);
  }

  // Indexing convention
  unsigned indexing = m_settings.value("indexing"_L1, 1).toUInt();
  // Options are 0 and 1, anything else becomes the default (1)
  if (indexing > 1) {
    m_ui->indexingComboBox->setCurrentIndex(1);
  } else {
    m_ui->indexingComboBox->setCurrentIndex(indexing);
  }
}

void PreferencesDialog::applyGeneral()
{
  // Interface language
  // If there are no translations available, the item in the combobox will be
  // English (Default) because it's the only option; however, should translation
  // files later be installed we want the default to remain "system", so don't
  // overwrite the config value if the user couldn't make an active choice
  if (m_ui->userInterfaceLanguageComboBox->count() > 1) {
    QVariant currentData = m_ui->userInterfaceLanguageComboBox->currentData();
    // Shouldn't happen, but if the box has no current item, don't change
    // anything in the settings
    if (currentData.isValid()) {
      qDebug() << "Selected language:"
               << m_ui->userInterfaceLanguageComboBox->currentText();
      QString chosenLocale = currentData.toString();
      qDebug() << "Corresponding locale code:" << chosenLocale;
      m_settings.setValue("locale"_L1, chosenLocale);
    }
  }

  // Indexing convention
  m_settings.setValue("indexing"_L1, m_ui->indexingComboBox->currentIndex());
}

// Controls
// [navigator/zoom] = invert scroll direction

void PreferencesDialog::updateControls()
{
  // Scroll inversion
  int zoom = m_settings.value("navigator/zoom"_L1, 1).toInt();
  // Options are -1 (inversion) and 1 (none), clamp to sign, 0 or missing -> 1
  if (zoom < 0) {
    m_ui->invertScrollDirectionCheckBox->setChecked(true);
  } else {
    m_ui->invertScrollDirectionCheckBox->setChecked(false);
  }
}

void PreferencesDialog::applyControls()
{
  // Scroll inversion
  // Inverted = -1, normal = 1
  if (m_ui->invertScrollDirectionCheckBox->isChecked()) {
    m_settings.setValue("navigator/zoom"_L1, -1);
  } else {
    m_settings.setValue("navigator/zoom"_L1, 1);
  }
}

// Rendering

void PreferencesDialog::updateRendering() {}

void PreferencesDialog::applyRendering() {}

// Colors

void PreferencesDialog::updateColors() {}

void PreferencesDialog::applyColors() {}

// Accessibility

void PreferencesDialog::updateAccessibility() {}

void PreferencesDialog::applyAccessibility() {}

// Bonding
// [bonding/tolerance] = added to vdW radii -> max distance for bond
// perception [bonding/minDistance] = square of min distance for bond
// perception

void PreferencesDialog::updateBonding()
{
  // Bonding tolerance
  m_ui->distanceToleranceDoubleSpinBox->setValue(
    m_settings.value("bonding/tolerance"_L1, 0.450).toDouble());
  // Minimum distance
  m_ui->minimumDistanceDoubleSpinBox->setValue(
    m_settings.value("bonding/minDistance"_L1, 0.320).toDouble());
}

void PreferencesDialog::applyBonding()
{
  // Bonding tolerance
  m_settings.setValue("bonding/tolerance"_L1,
                      m_ui->distanceToleranceDoubleSpinBox->value());
  // Minimum distance
  m_settings.setValue("bonding/minDistance"_L1,
                      m_ui->minimumDistanceDoubleSpinBox->value());
}

} // End namespace Avogadro
