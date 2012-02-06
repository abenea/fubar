#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <QtGui/QMainWindow>
#include <phonon/Phonon/MediaObject>
#include <phonon/Phonon/AudioOutput>
#include <QModelIndex>
#include <phonon/SeekSlider>
#include <phonon/VolumeSlider>
#include "playlisttab.h"

namespace Ui {
class MainWindowClass;
}

class PlaylistTab;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(Library& library, QWidget *parent = 0);
    ~MainWindow();

    Phonon::MediaObject *mediaObject;
    Phonon::AudioOutput *audioOutput;

    static MainWindow *instance;
public slots:
    void addDirectory();
    void libraryPreferences();
    void addView(PlaylistTab* playlistTab, const QString& name);

protected:
    virtual void closeEvent(QCloseEvent* );

private:
    boost::scoped_ptr<Ui::MainWindowClass> ui_;
    Phonon::SeekSlider *seekSlider_;
    Phonon::VolumeSlider *volumeSlider_;

    Library& library_;

    PlaylistTab *current();
    void writeSettings();
    void readSettings();

private slots:
    void tick(qint64 pos);
};

#endif // MAINWINDOW_H
