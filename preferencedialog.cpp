#include "preferencedialog.h"
#include "ui_preferencedialog.h"
#include "usageinfo.h"
#include "caesiumph.h"
#include "utils.h"

#include <QCloseEvent>
#include <QSettings>
#include <QMessageBox>

PreferenceDialog::PreferenceDialog(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::PreferenceDialog) {
    ui->setupUi(this);
    readPreferences();
}

PreferenceDialog::~PreferenceDialog() {
    delete ui;
}

void PreferenceDialog::on_actionCompression_triggered() {
    ui->stackedWidget->setCurrentIndex(1);
}

void PreferenceDialog::on_actionGeneral_triggered() {
    ui->stackedWidget->setCurrentIndex(0);
}

void PreferenceDialog::on_actionPrivacy_triggered() {
    ui->stackedWidget->setCurrentIndex(2);
}

void PreferenceDialog::closeEvent(QCloseEvent *event) {
    writePreferences();
    event->accept();
}

void PreferenceDialog::writePreferences() {
    QSettings settings;

    //General
    settings.beginGroup(KEY_PREF_GROUP_GENERAL);
    settings.setValue(KEY_PREF_GENERAL_OVERWRITE, ui->overwriteOriginalCheckBox->isChecked());
    settings.setValue(KEY_PREF_GENERAL_SUBFOLDER, ui->subfoldersCheckBox->isChecked());
    settings.setValue(KEY_PREF_GENERAL_PROMPT, ui->promptExitCheckBox->isChecked());
    settings.setValue(KEY_PREF_GENERAL_LOCALE, ui->languageComboBox->currentIndex());
    settings.endGroup();

    //Compression
    settings.beginGroup(KEY_PREF_GROUP_COMPRESSION);
    settings.setValue(KEY_PREF_COMPRESSION_EXIF, ui->exifCheckBox->isChecked());
    settings.setValue(KEY_PREF_COMPRESSION_PROGRESSIVE, ui->progressiveCheckBox->isChecked());
    settings.endGroup();

    //Privacy
    settings.beginGroup(KEY_PREF_GROUP_PRIVACY);
    settings.setValue(KEY_PREF_PRIVACY_USAGE, ui->sendInfoCheckBox->isChecked());
    settings.endGroup();
}

void PreferenceDialog::readPreferences() {
    QSettings settings;

    //General
    settings.beginGroup(KEY_PREF_GROUP_GENERAL);
    ui->overwriteOriginalCheckBox->setChecked(settings.value(KEY_PREF_GENERAL_OVERWRITE).value<bool>());
    ui->subfoldersCheckBox->setChecked(settings.value(KEY_PREF_GENERAL_SUBFOLDER).value<bool>());
    ui->promptExitCheckBox->setChecked(settings.value(KEY_PREF_GENERAL_PROMPT).value<bool>());
    ui->languageComboBox->setCurrentIndex(settings.value(KEY_PREF_GENERAL_LOCALE).value<int>());
    settings.endGroup();

    //Compression
    settings.beginGroup(KEY_PREF_GROUP_COMPRESSION);
    ui->exifCheckBox->setChecked(settings.value(KEY_PREF_COMPRESSION_EXIF).value<bool>());
    ui->progressiveCheckBox->setChecked(settings.value(KEY_PREF_COMPRESSION_PROGRESSIVE).value<bool>());
    settings.endGroup();

    //Privacy
    settings.beginGroup(KEY_PREF_GROUP_PRIVACY);
    ui->seeInfoButton->setChecked(settings.value(KEY_PREF_PRIVACY_USAGE).value<bool>());
    settings.endGroup();
}

void PreferenceDialog::on_seeInfoButton_clicked() {
    QMessageBox msgBox;
    msgBox.setText(tr("This data will help improve this application and won't be shared with anyone."));
    msgBox.setDetailedText(uinfo->printJSON());
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setDefaultButton(QMessageBox::Ok);
    //Fixed size hack
    QSpacerItem* horizontalSpacer = new QSpacerItem(400, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    QGridLayout* layout = (QGridLayout*)msgBox.layout();
    layout->addItem(horizontalSpacer, layout->rowCount(), 0, 1, layout->columnCount());
    msgBox.exec();
}
