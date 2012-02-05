#ifndef LIBRARY_H
#define LIBRARY_H

#include "track.h"
#include "directory.h"
#include "directorywatcher.h"
#include "playlist.h"
#include "libraryeventtype.h"

#include <boost/shared_ptr.hpp>
#include <QString>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <vector>
#include <map>

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

public slots:
    void setMusicFolders(const std::vector<QString>& folders);
    void quit();

signals:
    // idea: make this a vector<events>
    void libraryChanged(LibraryEvent event);

protected:
    void run();

private:
    void saveToDisk();
    void loadFromDisk();

    // Rescan monitored directories and update data structures
    void rescan();
    bool stopRescan() { return stop_ || restart_; }
    void scanDirectory(const QString& path);
    boost::shared_ptr<Track> scanFile(const QString& path);

    void addDirectory(boost::shared_ptr<Directory> directory);
    void addFile(boost::shared_ptr<Track> track);
    void removeFile(QString path);
    void removeDirectory(QString path);

    void fileCallback(QString path, LibraryEventType event);
    void directoryCallback(QString path, LibraryEventType event);
private:
    std::vector<QString> music_folders_;

    typedef QMap<QString, boost::shared_ptr<Directory> > DirectoryMap;
    DirectoryMap directories_;

    boost::shared_ptr<DirectoryWatcher> watcher_;
    QMutex mutex_;
    bool stop_, restart_;

    QMutex stop_mutex_;
    QWaitCondition stop_rescan_;
    bool rescanning_;
};

#endif // LIBRARY_H
