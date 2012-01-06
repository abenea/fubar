#ifndef LIBRARY_H
#define LIBRARY_H

#include "track.h"
#include "directory.h"
#include "directorywatcher.h"

#include <boost/shared_ptr.hpp>
#include <QString>
#include <vector>
#include <map>


class Library
{
public:
    Library();
    ~Library();

    void setMusicFolders(const std::vector<QString>& folders);

    void dumpDatabase() const;

    void watch();
private:
    void saveToDisk();
    void loadFromDisk();

    void scan();
    void scanDirectory(const QString& path);
    void scanFile(const QString& path);

    void addDirectory(boost::shared_ptr<Directory> directory);
    void addFile(boost::shared_ptr<Track> track);
    void removeFile(QString path);
    void removeDirectory(QString path);

    void fileCallback(QString path, WatchEvent event);
    void directoryCallback(QString path, WatchEvent event);
private:
    std::vector<QString> music_folders_;

    typedef std::map<QString, boost::shared_ptr<Directory> > DirectoryMap;
    DirectoryMap directories_, old_directories_;

    boost::shared_ptr<DirectoryWatcher> watcher_;
};

#endif // LIBRARY_H
