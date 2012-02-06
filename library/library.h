#ifndef LIBRARY_H
#define LIBRARY_H

#include "track.h"
#include "directory.h"
#include "directorywatcher.h"
#include "playlist.h"
#include "libraryeventtype.h"

#include <boost/shared_ptr.hpp>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QList>
#include <QMap>

class PlaylistTab;

struct LibraryEvent {
    boost::shared_ptr<Track> track;
    LibraryEventType op;

    LibraryEvent() {}
    LibraryEvent(boost::shared_ptr<Track> t, LibraryEventType o) : track(t), op(o) {}

    std::string op2str();
};

class Library : public QThread
{
    Q_OBJECT
public:
    Library(QObject * parent = 0);
    ~Library();

    void dumpDatabase() const;

    void watch();
    void stopWatch();

    void registerView(PlaylistTab* view);

    QStringList getMusicFolders();

public slots:
    void setMusicFolders(QStringList folders);

    void startMonitoring();
    void stopMonitoring();
    void restartMonitoring();

    void quit();

signals:
    // idea: make this a vector<events>
    void libraryChanged(LibraryEvent event);

protected:
    void run();

private:
    void getFoldersFromSettings();
    void setFoldersInSettings();
    void saveToDisk();
    void loadFromDisk();

    void waitForMonitoringStart();

    // Rescan monitored directories and update data structures
    void rescan();
    void stopRescanning();
    bool stopRescan() { return quit_ || !should_be_working_; }
    void scanDirectory(const QString& path);
    boost::shared_ptr<Track> scanFile(const QString& path);

    void addDirectory(boost::shared_ptr<Directory> directory);
    void addFile(boost::shared_ptr<Track> track);
    void removeFile(QString path);
    void removeDirectory(QString path);

    void fileCallback(QString path, LibraryEventType event);
    void directoryCallback(QString path, LibraryEventType event);
private:
    QStringList music_folders_;

    typedef QMap<QString, boost::shared_ptr<Directory> > DirectoryMap;
    DirectoryMap directories_;

    boost::shared_ptr<DirectoryWatcher> watcher_;
    QMutex mutex_;
    bool quit_;

    QMutex stop_rescan_mutex_, pause_monitoring_mutex_;
    QWaitCondition stop_rescan_, pause_monitoring_;
    bool rescanning_; // library is rescanning folders
    bool watching_;   // library is handling inotify events
    bool should_be_working_; // library should be fucking working on something
};

#endif // LIBRARY_H
