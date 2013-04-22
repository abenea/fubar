#ifndef LIBRARY_H
#define LIBRARY_H

#include "track.h"
#include "directory.h"
#include "directorywatcher.h"
#include "libraryeventtype.h"

#include <QString>
#include <QStringList>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QList>
#include <QMap>

struct LibraryEvent {
    std::shared_ptr<Track> track;
    LibraryEventType op;

    LibraryEvent() {}
    LibraryEvent(std::shared_ptr<Track> t, LibraryEventType o) : track(t), op(o) {}

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

    QStringList getMusicFolders();
    QList<std::shared_ptr<Track> > getTracks();

public slots:
    void setMusicFolders(QStringList folders);

    void startMonitoring();
    void stopMonitoring();
    void restartMonitoring(bool wipeDatabase = false);

    void quit();

signals:
    // idea: make this a vector<events>
    void libraryChanged(LibraryEvent event);
    // Emit when library directories change
    // Completely replace the contents of synced playlists with these tracks
    void libraryChanged(QList<std::shared_ptr<Track>> tracks);

protected:
    void run();

private slots:
    void persist();

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
    std::shared_ptr<Track> scanFile(const QString& path);

    void addDirectory(std::shared_ptr<Directory> directory);
    void addFile(std::shared_ptr<Track> track);
    void removeFile(QString path);
    void removeDirectory(QString path);

    void fileCallback(QString path, LibraryEventType event);
    void directoryCallback(QString path, LibraryEventType event);
private:
    QStringList music_folders_;

    typedef QMap<QString, std::shared_ptr<Directory> > DirectoryMap;
    DirectoryMap directories_;

    std::shared_ptr<DirectoryWatcher> watcher_;
    QMutex mutex_;
    bool quit_;

    QMutex stop_rescan_mutex_, pause_monitoring_mutex_;
    QWaitCondition stop_rescan_, pause_monitoring_;
    bool rescanning_; // library is rescanning folders
    bool watching_;   // library is handling inotify events
    bool should_be_working_; // library should be working on something

    bool dirty_; // library was modified since stored to disk
};

#endif // LIBRARY_H
