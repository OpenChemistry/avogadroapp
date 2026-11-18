/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#include "preferencesdialog.h"
#include "ui_preferencesdialog.h"

#include "i18n.h"

#include <QtWidgets/QListWidget>

#include <QtCore/QSettings>

namespace Avogadro {

PreferencesDialog::PreferencesDialog(QWidget* parent_, int initialPageIndex)
  : QDialog(parent_), m_ui(new Ui::PreferencesDialog), m_settings(),
    m_availableLocales(availableLocales())
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

  // Each page should have a corresponding apply method that changes m_settings
  // to reflect the state of the widgets. The method is then called if the
  // preferences dialog is closed by clicking OK. It is never called if the
  // dialog is closed with Cancel.
  connect(this, &QDialog::accepted, this, &PreferencesDialog::applyAll);
}

PreferencesDialog::~PreferencesDialog()
{
  delete m_ui;
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
  m_ui->userInterfaceLanguageComboBox->addItem("English (Default)");
  // Populate the language selection box with available translations
  if (!m_availableLocales.isEmpty()) {
    // This is gated by the availability of translations so that we don't
    // pretend to the user they can have the system language if they can't!
    m_ui->userInterfaceLanguageComboBox->addItem(tr("System Language"));
    // Displaying in the native script causes a long delay if no appropriate
    // font can be found (at least when there are lots of fonts installed).
    // To save Qt loading and trying fonts to see if they work, we will check
    // if the language is even supported, and if not, omit it.
    auto availableWritingSystems = QFontDatabase::writingSystems();
    foreach (const QString& localeCode, m_availableLocales) {
      QLocale locale(localeCode);
      auto writingSystem = Avogadro::scriptToWritingSystem(locale.script());
      if (writingSystem && availableWritingSystems.contains(writingSystem)) {
        QString languageLabel = locale.nativeLanguageName();
        // Add the territory to disambiguate if the locale code has it too
        // (but not for locales where Qt's "native name" already says it)
        if (localeCode.contains('_') && !localeCode.startsWith("en") &&
            !(localeCode == "fr_CA"))
          languageLabel += " (" + locale.nativeTerritoryName() + ")";
        languageLabel += " [" + localeCode + "]";
        m_ui->userInterfaceLanguageComboBox->addItem(languageLabel);
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
  // If there are no translations available, we can skip this as there is only
  // one option anyway: English (Default)
  if (!m_availableLocales.isEmpty()) {
    int languageIndex;
    if (m_settings.contains("locale")) {
      QString configLocale = m_settings.value("locale").toString();
      qDebug() << "Locale is configured as:" << configLocale;
      if (configLocale == "default") {
        languageIndex = 0;
      } else if (configLocale == "system") {
        languageIndex = 1;
      } else {
        // Add 2 as the first combobox item is for default English with no
        // translation and the second is for the system locale
        languageIndex = m_availableLocales.indexOf(configLocale) + 2;
      }
    } else {
      // No explicit setting in the config indicates to use the system
      // language, which the second item in the combobox corresponds to
      languageIndex = 1;
    }
    m_ui->userInterfaceLanguageComboBox->setCurrentIndex(languageIndex);
  }

  // Indexing convention
  unsigned indexing = m_settings.value("indexing", 1).toUInt();
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
  int languageIndex = m_ui->userInterfaceLanguageComboBox->currentIndex();
  qDebug() << "Selected language:"
           << m_ui->userInterfaceLanguageComboBox->currentText();
  if (languageIndex == -1) {
    // Nothing was selected, so don't change anything
  } else if (languageIndex == 0) {
    // Choice is to not translate at all
    m_settings.setValue("locale", "default");
  } else if (languageIndex == 1) {
    // Choice is to follow the system language
    m_settings.setValue("locale", "system");
  } else {
    // Take off 2 as system and default aren't locales and aren't in the list
    int localeIndex = languageIndex - 2;
    // at will cause crash if index is out-of-range, but there's no reason why
    // the combobox shouldn't contain one row per available locale
    QString chosenLocale = m_availableLocales.at(localeIndex);
    qDebug() << "Corresponding locale code:" << chosenLocale;
    m_settings.setValue("locale", chosenLocale);
  }

  // Indexing convention
  m_settings.setValue("indexing", m_ui->indexingComboBox->currentIndex());
}

// Controls
// [navigator/zoom] = invert scroll direction

void PreferencesDialog::updateControls()
{
  // Scroll inversion
  int zoom = m_settings.value("navigator/zoom", 1).toInt();
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
    m_settings.setValue("navigator/zoom", -1);
  } else {
    m_settings.setValue("navigator/zoom", 1);
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
// [bonding/tolerance] = added to vdW radii -> max distance for bond perception
// [bonding/minDistance] = square of min distance for bond perception

void PreferencesDialog::updateBonding()
{
  // Bonding tolerance
  m_ui->distanceToleranceDoubleSpinBox->setValue(
    m_settings.value("bonding/tolerance", 0.450).toDouble());
  // Minimum distance
  m_ui->minimumDistanceDoubleSpinBox->setValue(
    m_settings.value("bonding/minDistance", 0.320).toDouble());
}

void PreferencesDialog::applyBonding()
{
  // Bonding tolerance
  m_settings.setValue("bonding/tolerance",
                      m_ui->distanceToleranceDoubleSpinBox->value());
  // Minimum distance
  m_settings.setValue("bonding/minDistance",
                      m_ui->minimumDistanceDoubleSpinBox->value());
}

} // End namespace Avogadro
