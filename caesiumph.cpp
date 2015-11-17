#include "caesiumph.h"
#include "ui_caesiumph.h"
#include "aboutdialog.h"
#include "utils.h"
#include "lossless.h"
#include "cimageinfo.h"
#include "exif.h"
#include "preferencedialog.h"
#include "usageinfo.h"
#include "networkoperations.h"
#include "qdroptreewidget.h"
#include "ctreewidgetitem.h"

#include <QProgressDialog>
#include <QFileDialog>
#include <QStandardPaths>
#include <QGraphicsPixmapItem>
#include <QFileInfo>
#include <QtConcurrent>
#include <QFuture>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QImageReader>
#include <QSettings>
#include <QCloseEvent>
#include <QMessageBox>
#include <QDesktopServices>
#include <QDirIterator>

#include <exiv2/exiv2.hpp>

#include <QDebug>

//TODO GENERAL: handle plurals in counts

CaesiumPH::CaesiumPH(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::CaesiumPH)
{
    ui->setupUi(this);
    initializeSettings();
    initializeConnections();
    initializeUI();
    readPreferences();
    checkUpdates();

#ifdef _WIN32
    QThreadPool::globalInstance()->setMaxThreadCount(1);
#endif
    QThreadPool::globalInstance()->setMaxThreadCount(QThread::idealThreadCount());
}

CaesiumPH::~CaesiumPH() {
    delete ui;
}

void CaesiumPH::initializeUI() {
    QSettings settings;

    //Install event filter for buttons
    ui->addFilesButton->installEventFilter(this);
    ui->addFolderButton->installEventFilter(this);
    ui->compressButton->installEventFilter(this);
    ui->removeItemButton->installEventFilter(this);
    ui->clearButton->installEventFilter(this);
    ui->showSidePanelButton->installEventFilter(this);
    ui->settingsButton->installEventFilter(this);

    //Set the headers size
    ui->listTreeWidget->header()->resizeSection(0, 180);
    ui->listTreeWidget->header()->resizeSection(1, 100);
    ui->listTreeWidget->header()->resizeSection(2, 100);
    ui->listTreeWidget->header()->resizeSection(3, 80);
    ui->listTreeWidget->header()->resizeSection(4, 100);

    //Set menu invisible for Windows/Linux
    ui->menuBar->setVisible(false);

    //Update button visibility
    ui->updateButton->setVisible(false);

    //Restore window state
    settings.beginGroup(KEY_PREF_GROUP_GEOMETRY);
    resize(settings.value(KEY_PREF_GEOMETRY_SIZE, QSize(880, 500)).toSize());
    move(settings.value(KEY_PREF_GEOMETRY_POS, QPoint(200, 200)).toPoint());
    ui->sidePanelDockWidget->setVisible(settings.value(KEY_PREF_GEOMETRY_PANEL_VISIBLE).value<bool>());
    on_sidePanelDockWidget_visibilityChanged(settings.value(KEY_PREF_GEOMETRY_PANEL_VISIBLE).value<bool>());
    ui->listTreeWidget->sortByColumn(settings.value(KEY_PREF_GEOMETRY_SORT_COLUMN).value<int>(),
                                     settings.value(KEY_PREF_GEOMETRY_SORT_ORDER).value<Qt::SortOrder>());
    settings.endGroup();

    //Default EXIF value
    ui->exifTextEdit->setText(tr("No EXIF info available"));

    //No blue border on focus on Mac
    ui->listTreeWidget->setAttribute(Qt::WA_MacShowFocusRect, false);

    //Status bar widgets
    //Vertical line
    QFrame* statusBarLine = new QFrame();
    statusBarLine->setObjectName("statusBarLine");
    statusBarLine->setFrameShape(QFrame::VLine);
    statusBarLine->setFrameShadow(QFrame::Raised);
    //List info label
    QLabel* statusBarLabel = new QLabel();
    statusBarLabel->setObjectName("statusBarLabel");
    statusBarLabel->setText(tr("Welcome to CaesiumPH!"));
    //Add them to the status bar
    ui->statusBar->addPermanentWidget(statusBarLine);
    ui->statusBar->addPermanentWidget(statusBarLabel);

}

void CaesiumPH::initializeConnections() {
    //Edit menu
    //List clear
    connect(ui->actionClear_list, SIGNAL(triggered()), ui->listTreeWidget, SLOT(clear()));
    connect(ui->actionClear_list, SIGNAL(triggered()), this, SLOT(updateStatusBarCount()));
    //List select all
    connect(ui->actionSelect_all, SIGNAL(triggered()), ui->listTreeWidget, SLOT(selectAll()));
    //UI buttons
    connect(ui->compressButton, SIGNAL(released()), this, SLOT(on_actionCompress_triggered()));
    connect(ui->addFilesButton, SIGNAL(released()), this, SLOT(on_actionAdd_pictures_triggered()));
    connect(ui->addFolderButton, SIGNAL(released()), this, SLOT(on_actionAdd_folder_triggered()));
    connect(ui->removeItemButton, SIGNAL(released()), this, SLOT(on_actionRemove_items_triggered()));
    connect(ui->clearButton, SIGNAL(released()), ui->listTreeWidget, SLOT(clear()));
    connect(ui->clearButton, SIGNAL(released()), this, SLOT(updateStatusBarCount()));

    //TreeWidget drop event
    connect(ui->listTreeWidget, SIGNAL(dropFinished(QStringList)), this, SLOT(showImportProgressDialog(QStringList)));
}

void CaesiumPH::initializeSettings() {
    QCoreApplication::setApplicationName("CaesiumPH");
    QCoreApplication::setOrganizationName("SaeraSoft");
    QCoreApplication::setOrganizationDomain("saerasoft.com");

    uinfo->initialize();
}

void CaesiumPH::readPreferences() {
    //Read important parameters from settings
    QSettings settings;

    settings.beginGroup(KEY_PREF_GROUP_COMPRESSION);
    params.exif = settings.value(KEY_PREF_COMPRESSION_EXIF).value<bool>();
    params.progressive = settings.value(KEY_PREF_COMPRESSION_PROGRESSIVE).value<bool>();
    params.importantExifs.clear();
    if (settings.value(KEY_PREF_COMPRESSION_EXIF_COPYRIGHT).value<bool>()) {
        params.importantExifs.append(EXIF_COPYRIGHT);
    }
    if (settings.value(KEY_PREF_COMPRESSION_EXIF_DATE).value<bool>()) {
        params.importantExifs.append(EXIF_DATE);
    }
    if (settings.value(KEY_PREF_COMPRESSION_EXIF_COMMENT).value<bool>()) {
        params.importantExifs.append(EXIF_COMMENTS);
    }
    settings.endGroup();

    settings.beginGroup(KEY_PREF_GROUP_GENERAL);
    params.overwrite = settings.value(KEY_PREF_GENERAL_OVERWRITE).value<bool>();
    params.outMethodIndex = settings.value(KEY_PREF_GENERAL_OUTPUT_METHOD).value<int>();
    params.outMethodString = settings.value(KEY_PREF_GENERAL_OUTPUT_STRING).value<QString>();
    settings.endGroup();
}

//Button hover functions
bool CaesiumPH::eventFilter(QObject *obj, QEvent *event) {
    if (obj == (QObject*) ui->addFilesButton) {
        if (event->type() == QEvent::Enter) {
            ui->addFilesButton->setIcon(QIcon(":/icons/ui/add_hover.png"));
            return true;
        } else if (event->type() == QEvent::Leave){
            ui->addFilesButton->setIcon(QIcon(":/icons/ui/add.png"));
            return true;
        } else {
            return false;
        }
    } else if (obj == (QObject*) ui->addFolderButton) {
        if (event->type() == QEvent::Enter) {
            ui->addFolderButton->setIcon(QIcon(":/icons/ui/folder_hover.png"));
            return true;
        } else if (event->type() == QEvent::Leave){
            ui->addFolderButton->setIcon(QIcon(":/icons/ui/folder.png"));
            return true;
        } else {
            return false;
        }
    } else if (obj == (QObject*) ui->compressButton) {
        if (event->type() == QEvent::Enter) {
            ui->compressButton->setIcon(QIcon(":/icons/ui/compress_hover.png"));
            return true;
        } else if (event->type() == QEvent::Leave){
            ui->compressButton->setIcon(QIcon(":/icons/ui/compress.png"));
            return true;
        } else {
            return false;
        }
    } else if (obj == (QObject*) ui->removeItemButton) {
        if (event->type() == QEvent::Enter) {
            ui->removeItemButton->setIcon(QIcon(":/icons/ui/remove_hover.png"));
            return true;
        } else if (event->type() == QEvent::Leave){
            ui->removeItemButton->setIcon(QIcon(":/icons/ui/remove.png"));
            return true;
        } else {
            return false;
        }
    } else if (obj == (QObject*) ui->clearButton) {
        if (event->type() == QEvent::Enter) {
            ui->clearButton->setIcon(QIcon(":/icons/ui/clear_hover.png"));
            return true;
        } else if (event->type() == QEvent::Leave){
            ui->clearButton->setIcon(QIcon(":/icons/ui/clear.png"));
            return true;
        } else {
            return false;
        }
    } else if (obj == (QObject*) ui->showSidePanelButton) {
        if (!ui->sidePanelDockWidget->isVisible()) {
            if (event->type() == QEvent::Enter) {
                ui->showSidePanelButton->setIcon(QIcon(":/icons/ui/side_panel_active.png"));
                return true;
            } else if (event->type() == QEvent::Leave){
                ui->showSidePanelButton->setIcon(QIcon(":/icons/ui/side_panel.png"));
                return true;
            } else {
                return false;
            }
        }
        else {
            return false;
        }
    } else if (obj == (QObject*) ui->settingsButton) {
        if (event->type() == QEvent::Enter) {
            ui->settingsButton->setIcon(QIcon(":/icons/ui/settings_hover.png"));
            return true;
        } else if (event->type() == QEvent::Leave){
            ui->settingsButton->setIcon(QIcon(":/icons/ui/settings.png"));
            return true;
        } else {
            return false;
        }
    } else {
        //Pass the event on to the parent class
        return QWidget::eventFilter(obj, event);
    }
}

void CaesiumPH::on_actionAbout_CaesiumPH_triggered() {
    //Start the about dialog
    AboutDialog* ad = new AboutDialog(this);
    ad->setWindowFlags(Qt::Tool | Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::CustomizeWindowHint);
    ad->show();
}

void CaesiumPH::on_actionAdd_pictures_triggered() {
    //Generate file dialog for import and call the progress dialog indicator
    QStringList fileList = QFileDialog::getOpenFileNames(this,
                                  tr("Import files..."),
                                  QStandardPaths::standardLocations(QStandardPaths::PicturesLocation).at(0),
                                  inputFilter);
    if (!fileList.isEmpty()) {
        showImportProgressDialog(fileList);
    }
}

void CaesiumPH::showImportProgressDialog(QStringList list) {
    QSettings settings;
    bool scanSubdir = settings.value(KEY_PREF_GROUP_GENERAL + KEY_PREF_GENERAL_SUBFOLDER).value<bool>();

    QProgressDialog progress(tr("Importing..."), tr("Cancel"), 0, list.count(), this);
    progress.setWindowIcon(QIcon(":/icons/main/logo.png"));
    progress.show();
    progress.setWindowModality(Qt::WindowModal);

    //Actual added item count and duplicate count
    int item_count = 0;
    int duplicate_count = 0;

    for (int i = 0; i < list.size(); i++) {

        //Check if it's a folder
        if (QDir(list[i]).exists()) {
            //If so, add the whole content to the end of the list
            QDirIterator it(list[i], inputFilterList, QDir::AllEntries, scanSubdir ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);
            while(it.hasNext()) {
                it.next();
                list.append(it.filePath());
            }
        }

        progress.setValue(i);

        //Validate extension
        if (!isJPEG(QStringToChar(list.at(i)))) {
            continue;
        }

        //Generate new CImageInfo
        CImageInfo* currentItemInfo = new CImageInfo(list.at(i));

        //Check if it has a duplicate
        if (hasADuplicateInList(currentItemInfo)) {
            duplicate_count++;
            continue;
        }

        //Populate list
        QStringList itemContent = QStringList() << currentItemInfo->getBaseName()
                                                << currentItemInfo->getFormattedSize()
                                                << ""
                                                << ""
                                                << currentItemInfo->getFullPath();

        ui->listTreeWidget->addTopLevelItem(new CTreeWidgetItem(ui->listTreeWidget,
                                                                itemContent));

        item_count++;

        if (progress.wasCanceled()) {
            break;
        }
    }
    progress.setValue(list.count());

    //Show import stats in the status bar
    ui->statusBar->showMessage(duplicate_count > 0 ?
                                   QString::number(item_count) + tr(" files added to the list") + ", " +
                                                   QString::number(duplicate_count) + tr(" duplicates found")
                                 : QString::number(item_count) + tr(" files added to the list"));
    updateStatusBarCount();
}

void CaesiumPH::on_actionAdd_folder_triggered() {
    QString path = QFileDialog::getExistingDirectory(this,
                                      tr("Select a folder to import..."),
                                      QStandardPaths::standardLocations(QStandardPaths::PicturesLocation).at(0),
                                      QFileDialog::ShowDirsOnly);
    if (!path.isEmpty()) {
        showImportProgressDialog(QStringList() << path);
    }
}

void CaesiumPH::on_actionRemove_items_triggered() {
    int count = ui->listTreeWidget->selectedItems().count();
    if (count == ui->listTreeWidget->topLevelItemCount()) {
        ui->listTreeWidget->clear();
    } else {
        for (int i = 0; i < count; i++) {
            ui->listTreeWidget->takeTopLevelItem(ui->listTreeWidget->indexOfTopLevelItem(ui->listTreeWidget->selectedItems().at(0)));
        }
    }
    //Clear boxes
    clearUI();
    //Update count
    updateStatusBarCount();
    //Show a message
    ui->statusBar->showMessage(QString::number(count) + tr(" items removed"));
}

extern void compressRoutine(CTreeWidgetItem* item) {
    //Input file path
    QString inputPath = item->text(4);
    QFileInfo* originalInfo = new QFileInfo(item->text(4));
    qint64 originalSize = originalInfo->size();
    QString outputPath;

    if (params.overwrite) {
        /*
         * Overwrite
         * Set the output path to a temporary directory
         * so we can check if it is actually bigger than the original
         * and eventually overwrite it
         *
        */
        if (tempDir.isValid()) {
            //Unique temporary directory
            outputPath = tempDir.path() + QDir::separator() + originalInfo->fileName();
            qDebug() << outputPath;
        } else {
            qDebug() << "Cannot create a temporary folder. Abort.";
            exit(-1);
        }
    } else {
        switch (params.outMethodIndex) {
        case 0:
            //Add a suffix
            outputPath = originalInfo->filePath().replace(originalInfo->completeBaseName(),
                                                          originalInfo->baseName() + params.outMethodString);
            break;
        case 1:
            //Compress in a subfolder
            outputPath = originalInfo->path() + QDir::separator() + params.outMethodString + QDir::separator() + originalInfo->fileName();
            //Create it
            //WARNING This does not check for user permission
            QDir().mkdir(originalInfo->path() + QDir::separator() + params.outMethodString + QDir::separator());
            break;
        case 2:
            //Compress in a custom directory
            outputPath = params.outMethodString + QDir::separator() + originalInfo->fileName();
            //WARNING This does not check for user permission
            QDir().mkdir(params.outMethodString);
        default:
            break;
        }
    }

    //Not really necessary if we copy the whole EXIF data
    Exiv2::ExifData exifData = getExifFromPath(QStringToChar(inputPath));
    //BUG Sometimes files are empty. Check it out.
    cclt_optimize(QStringToChar(inputPath),
                  QStringToChar(outputPath),
                  params.exif,
                  params.progressive,
                  QStringToChar(inputPath));



    //Write important metadata as user requested
    if (params.exif != 2 && !params.importantExifs.isEmpty()) {
        writeSpecificExifTags(exifData, outputPath, params.importantExifs);
    }

    //Gets new file info
    QFileInfo* fileInfo = new QFileInfo(outputPath);
    //Get the new size
    qint64 outputSize = fileInfo->size();

    //Check if the output file is actually bigger than the original
    if (outputSize > originalSize) {
        /*
         * If we choose to overwrite the files, just leave the files in the temporary folder
         * to be removed afterwards
         * Instead, if we compressed in a custom folder, copy the original over the compressed one
         * and set all the output results to point to the original file
         */
        qDebug() << "Output is bigger than input";
        if (!params.overwrite) {
            //Copy the original file over the compressed one
            QFile* outputFile = new QFile(outputPath);
            //Check if the file already exists (just a security check) and remove it
            if (outputFile->exists()) {
                //WARNING No error check
                outputFile->remove();
            }
            //Rename the original file with the output path
            //TODO Better error handling please
            if (!QFile(item->text(4)).copy(outputPath)) {
                qDebug() << "ERROR: Failed while moving: " << item->text(4);
            }
        }
        //Set the importat stats to point to the original file
        outputSize = originalSize;
    } else {
        //The new file is smaller
        //If overwrite is on, move the file from the temp folder into the original
        if (params.overwrite) {
            //Remove the original
            QFile(item->text(4)).remove();
            //Move the compressed
            //TODO Better error handling please
            if (!QFile(outputPath).rename(item->text(4))) {
                qDebug() << "ERROR: Failed while moving: " << item->text(4);
            }
        }
    }
    item->setText(2, toHumanSize(outputSize));
    item->setText(3, getRatio(originalSize, outputSize));

    //Global compression counters for the entire compression process
    originalsSize += originalSize;
    compressedSize += outputSize;
    compressedFiles++;

    //Usage reports
    if (originalInfo->size() > uinfo->max_bytes) {
        uinfo->setMax_bytes(originalInfo->size());
    }

    if ((originalInfo->size() - fileInfo->size()) * 100 / (double) originalInfo->size() > uinfo->best_ratio
            && fileInfo->size() != 0) {
        uinfo->setBest_ratio((originalInfo->size() - fileInfo->size()) * 100 / (double) originalInfo->size());
    }
}

void CaesiumPH::on_actionCompress_triggered() {
    //Read preferences again
    readPreferences();
    //Reset counters
    originalsSize = compressedSize = compressedFiles = 0;
    //Register metatype for emitting changes
    qRegisterMetaType<QVector<int> >("QVector<int>");

    //Setting up a progress dialog
    QProgressDialog progressDialog;
    progressDialog.setWindowTitle(tr("CaesiumPH"));
    progressDialog.setLabelText(tr("Compressing..."));

    //Holds the list
    QList<CTreeWidgetItem*> list;

    //Setup watcher
    QFutureWatcher<void> watcher;

    //Gets the list filled
    for (int i = 0; i < ui->listTreeWidget->topLevelItemCount(); i++) {
        list.append((CTreeWidgetItem*) ui->listTreeWidget->topLevelItem(i));
    }

    QFuture<void> future = QtConcurrent::map(list, compressRoutine);

    //Setting up connections
    //Progress dialog
    connect(&watcher, SIGNAL(progressValueChanged(int)), &progressDialog, SLOT(setValue(int)));
    connect(&watcher, SIGNAL(progressRangeChanged(int, int)), &progressDialog, SLOT(setRange(int,int)));
    connect(&watcher, SIGNAL(finished()), &progressDialog, SLOT(reset()));
    connect(&progressDialog, SIGNAL(canceled()), &watcher, SLOT(cancel()));
    //Connect two slots for handling compression start/finish
    connect(&watcher, SIGNAL(started()), this, SLOT(compressionStarted()));
    connect(&watcher, SIGNAL(finished()), this, SLOT(compressionFinished()));

    //And start
    watcher.setFuture(future);

    //Show the dialog
    progressDialog.exec();
}

void CaesiumPH::compressionStarted() {
    //Start monitoring time while compressing
    timer.start();
}

void CaesiumPH::compressionFinished() {
    //Get elapsed time of the compression
    qDebug() << QTime::currentTime();
    qDebug() << toHumanSize(originalsSize) + " - " + toHumanSize(compressedSize) + " | " + getRatio(originalsSize, compressedSize);

    //Display statistics in the status bar
    ui->statusBar->showMessage(tr("Compression completed! ") +
                               QString::number(compressedFiles) + tr(" files compressed in ") +
                               msToFormattedString(timer.elapsed()) + ", " +
                               tr("from ") + toHumanSize(originalsSize) + tr(" to ") + toHumanSize(compressedSize) +
                               ". " + tr("Saved ") + toHumanSize(originalsSize - compressedSize) +
                               " (" + getRatio(originalsSize, compressedSize) + ")"
                               );
    timer.invalidate();
    //Set parameters for usage info
    uinfo->setCompressed_bytes(uinfo->compressed_bytes + originalsSize);
    uinfo->setCompressed_pictures(uinfo->compressed_pictures + ui->listTreeWidget->topLevelItemCount());

    uinfo->writeJSON();
}

void CaesiumPH::on_sidePanelDockWidget_topLevelChanged(bool topLevel) {
    //Check if it's floating and hide/show the line
    ui->sidePanelLine->setVisible(!topLevel);
}

void CaesiumPH::on_sidePanelDockWidget_visibilityChanged(bool visible) {
    //Handle the close event
    on_showSidePanelButton_clicked(visible);
    ui->showSidePanelButton->setChecked(visible);
}

void CaesiumPH::on_showSidePanelButton_clicked(bool checked) {
    ui->sidePanelDockWidget->setVisible(checked);
    //If it's not floating, we have a dedicated handler for that
    if (!ui->sidePanelDockWidget->isFloating()) {
        ui->sidePanelLine->setVisible(checked);
    }
    //Set icons
    if (checked) {
        ui->showSidePanelButton->setIcon(QIcon(":/icons/ui/side_panel_active.png"));
    } else {
        ui->showSidePanelButton->setIcon(QIcon(":/icons/ui/side_panel.png"));
    }
}


void CaesiumPH::on_listTreeWidget_itemSelectionChanged() {
    //Check if there's a selection
    if (ui->listTreeWidget->selectedItems().length() > 0) {
        //Get the first item selected
        CTreeWidgetItem* currentItem = (CTreeWidgetItem*) ui->listTreeWidget->selectedItems().at(0);

        //Connect the global watcher to the slot
        connect(&imageWatcher, SIGNAL(resultReadyAt(int)), this, SLOT(finishPreviewLoading(int)));
        //Run the image loader function
        imageWatcher.setFuture(QtConcurrent::run<QImage>(this, &CaesiumPH::loadImagePreview, currentItem->text(4)));

        //Load EXIF info
        //TODO Should run in another thread too?
        ui->exifTextEdit->setText(exifDataToString(getExifFromPath(QStringToChar(currentItem->text(4)))));
    } else {
        imageWatcher.cancel();
        clearUI();
    }
}

QImage CaesiumPH::loadImagePreview(QString path) {
    //Load a scaled version of the image into memory
    QImageReader* imageReader = new QImageReader(path);
    imageReader->setScaledSize(getScaledSizeWithRatio(imageReader->size(), ui->imagePreviewLabel->size().width()));
    return imageReader->read();
}

void CaesiumPH::finishPreviewLoading(int i) {
    //Set the image
    ui->imagePreviewLabel->setPixmap(QPixmap::fromImage(imageWatcher.resultAt(i)));
}

void CaesiumPH::on_settingsButton_clicked() {
    NetworkOperations* no = new NetworkOperations(this);
    no->uploadUsageStatistics();
    PreferenceDialog* pd = new PreferenceDialog(this);
    pd->show();
}

void CaesiumPH::closeEvent(QCloseEvent *event) {
    QSettings settings;

    //Save window geometry
    settings.beginGroup(KEY_PREF_GROUP_GEOMETRY);
    settings.setValue(KEY_PREF_GEOMETRY_SIZE, size());
    settings.setValue(KEY_PREF_GEOMETRY_POS, pos());
    settings.setValue(KEY_PREF_GEOMETRY_PANEL_VISIBLE, ui->sidePanelDockWidget->isVisible());
    settings.setValue(KEY_PREF_GEOMETRY_SORT_COLUMN, ui->listTreeWidget->sortColumn());
    settings.setValue(KEY_PREF_GEOMETRY_SORT_ORDER, ui->listTreeWidget->header()->sortIndicatorOrder());
    settings.endGroup();

    if (settings.value(KEY_PREF_GROUP_GENERAL + KEY_PREF_GENERAL_PROMPT).value<bool>()) {
        //Display a prompt
        int res = QMessageBox::warning(this, tr("CaesiumPH"),
                                       tr("Do you really want to exit?"),
                                       QMessageBox::Ok | QMessageBox::Cancel);
        //Exit if OK, go back if Cancel
        //TODO Translate?
        switch (res) {
            case QMessageBox::Ok:
                event->accept();
                break;
            case QMessageBox::Cancel:
                event->ignore();
            default:
                break;
        }
    } else {
        event->accept();
    }
}

void CaesiumPH::checkUpdates() {
    qDebug() << "Check updates called";
    NetworkOperations* op = new NetworkOperations();
    op->checkForUpdates();
    connect(op, SIGNAL(checkForUpdatesFinished(int, QString)), this, SLOT(updateAvailable(int, QString)));
}

void CaesiumPH::updateAvailable(int version, QString versionTag) {
    qDebug() << "FOUND VERSION " << version;
    ui->updateButton->setVisible(version > versionNumber);
    updateVersionTag = versionTag;
}

bool CaesiumPH::hasADuplicateInList(CImageInfo *c) {
    for (int i = 0; i < ui->listTreeWidget->topLevelItemCount(); i++) {
        if (c->isEqual(ui->listTreeWidget->topLevelItem(i)->text(4))) {
            qDebug() << "Duplicate detected. Skipping.";
            return true;
        }
    }
    return false;
}

void CaesiumPH::on_updateButton_clicked() {
    NetworkOperations* op = new NetworkOperations();
    connect(op, SIGNAL(updateDownloadFinished(QString)), this, SLOT(startUpdateProcess(QString)));
    op->downloadUpdateRequest();
}

void CaesiumPH::startUpdateProcess(QString path) {
    QDesktopServices::openUrl(QUrl("file://" + path, QUrl::TolerantMode));
    this->close();
    qDebug() << path;
}

void CaesiumPH::clearUI() {
    ui->exifTextEdit->clear();
    ui->imagePreviewLabel->clear();
}

void CaesiumPH::updateStatusBarCount() {
    ui->statusBar->findChild<QLabel*>("statusBarLabel")->setText(
                QString::number(ui->listTreeWidget->topLevelItemCount()) +
                tr(" files in list"));

    //If the list is empty, we got a call from the clear SIGNAL, so handle the general message too
    if (ui->listTreeWidget->topLevelItemCount() == 0) {
       ui->statusBar->showMessage(tr("List cleared"));
    }
}
