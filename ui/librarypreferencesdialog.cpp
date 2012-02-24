#include "librarypreferencesdialog.h"
#include <QDebug>

const QString FOLDER_SEPARATOR = ";";
LibraryPreferencesDialog::LibraryPreferencesDialog(Library& library, QWidget* parent)
    : QDialog(parent)
    , library_(library)
{
    setupUi(this);
    QStringList folders = library_.getMusicFolders();
    libraryPaths->setText(folders.join(FOLDER_SEPARATOR));
}

void LibraryPreferencesDialog::accept()
{
    QSet<QString> current_set;
    QStringList current_list = libraryPaths->text().split(FOLDER_SEPARATOR);
    foreach (QString path, current_list) {
        current_set.insert(path);
    }
    QSet<QString> old_set;
    QStringList old_list = library_.getMusicFolders();
    foreach (QString path, old_list) {
        old_set.insert(path);
    }
    if (old_set != current_set) {
        library_.setMusicFolders(current_list);
        library_.restartMonitoring();
    }
    QDialog::accept();
}

void LibraryPreferencesDialog::reject()
{
    QDialog::reject();
}

void LibraryPreferencesDialog::on_actionStopMonitoring_clicked()
{
    library_.stopMonitoring();
}

void LibraryPreferencesDialog::on_actionStartMonitoring_clicked()
{
    library_.startMonitoring();
}

void LibraryPreferencesDialog::on_actionRescanLibrary_clicked()
{
    library_.setMusicFolders(libraryPaths->text().split(FOLDER_SEPARATOR));
    library_.restartMonitoring();
}

#include "librarypreferencesdialog.moc"