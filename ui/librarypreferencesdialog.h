#pragma once

#include "library/library.h"
#include <QDialog>

namespace Ui {
class LibraryPreferences;
}

class LibraryPreferencesDialog : public QDialog {
    Q_OBJECT
public:
    LibraryPreferencesDialog(Library &library, QWidget *parent = 0);
    virtual ~LibraryPreferencesDialog();

private slots:
    void accept();
    void reject();
    void on_actionStartMonitoring_clicked();
    void on_actionStopMonitoring_clicked();
    void on_actionRescanLibrary_clicked();

private:
    std::unique_ptr<Ui::LibraryPreferences> ui_;
    QStringList getCurrentList();
    Library &library_;
};
