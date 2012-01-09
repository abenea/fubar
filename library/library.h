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
#include <vector>
#include <map>


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

    void run();

    void setMusicFolders(const std::vector<QString>& folders);
    void dumpDatabase() const;

    void watch();
    void stopWatch();

    void getPlaylist(Playlist& playlist);

public slots:
    void quit();

signals:
    // idea: make this a vector<events>
    void libraryChanged(LibraryEvent event);

private:
    void saveToDisk();
    void loadFromDisk();

    void scan();
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

    typedef std::map<QString, boost::shared_ptr<Directory> > DirectoryMap;
    DirectoryMap directories_, old_directories_;

    boost::shared_ptr<DirectoryWatcher> watcher_;
    QMutex mutex_;
};

#endif // LIBRARY_H
