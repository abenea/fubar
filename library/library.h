#pragma once

#include "library/libraryeventtype.h"
#include "library/track_forward.h"

#include <QList>
#include <QMap>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QWaitCondition>

class Directory;
class DirectoryWatcher;

struct LibraryEvent {
    PTrack track;
    LibraryEventType op;

    LibraryEvent() {}
    LibraryEvent(PTrack t, LibraryEventType o) : track(t), op(o) {}

    std::string op2str();
};

class Library : public QThread {
    Q_OBJECT
public:
    Library(QObject *parent = 0);
    ~Library();

    void dumpDatabase() const;

    void watch();
    void stopWatch();

    QStringList getMusicFolders();
    QList<PTrack> getTracks();

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
    void libraryChanged(QList<PTrack> tracks);

public:
    // length hack: called when audioplayer sets a track's length
    void dirtyHack(PTrack track);

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
    void scanDirectory(const QString &path);
    PTrack scanFile(const QString &path);
    PTrack scanCue(const QString &path);

    void addDirectory(std::shared_ptr<Directory> directory);
    void addFile(PTrack track, bool loading = false);
    void removeFile(QString path);
    void removeDirectory(QString path);

    void fileCallback(QString path, LibraryEventType event);
    void directoryCallback(QString path, LibraryEventType event);

    // CUE hax
    void emitLibraryChanged(PTrack track, LibraryEventType type);
    QList<PTrack> getDirectoryTracks(std::shared_ptr<Directory> directory);

private:
    QStringList music_folders_;

    typedef QMap<QString, std::shared_ptr<Directory>> DirectoryMap;
    DirectoryMap directories_;

    std::shared_ptr<DirectoryWatcher> watcher_;
    QRecursiveMutex mutex_;
    bool quit_;

    QMutex stop_rescan_mutex_, pause_monitoring_mutex_;
    QWaitCondition stop_rescan_, pause_monitoring_;
    bool rescanning_;        // library is rescanning folders
    bool watching_;          // library is handling inotify events
    bool should_be_working_; // library should be working on something

    bool dirty_; // library was modified since stored to disk
};
