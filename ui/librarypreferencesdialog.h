#pragma once

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
    void on_actionStartMonitoring_clicked();
    void on_actionStopMonitoring_clicked();
    void on_actionRescanLibrary_clicked();

private:
    QStringList getCurrentList();
    Library& library_;
};
