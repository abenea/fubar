#ifndef LIBRARYPREFERENCESDIALOG_H
#define LIBRARYPREFERENCESDIALOG_H

#include "ui/ui_librarypreferences.h"
#include "library/library.h"
#include <QDialog>

class LibraryPreferencesDialog : public QDialog, private Ui::LibraryPreferences
{
    Q_OBJECT
public:
    LibraryPreferencesDialog(Library& library, QWidget *parent = 0);

private slots:
    void accept();
    void reject();
    // TODO: is this on_action convention retarded or I am?
    void on_actionStartMonitoring_clicked();
    void on_actionStopMonitoring_clicked();
    void on_actionRescanLibrary_clicked();

private:
    QStringList getCurrentList();
    Library& library_;
};

#endif // LIBRARYPREFERENCESDIALOG_H
