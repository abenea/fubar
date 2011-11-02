#ifndef LIBRARY_H
#define LIBRARY_H

#include "track.h"
#include "directory.h"

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

private:
    void saveToDisk();
    void loadFromDisk();

    void scan();
    void scanDirectory(const QString& path);
    void scanFile(const QString& path);

    void addDirectory(boost::shared_ptr<Directory> directory);
    void addFile(boost::shared_ptr<Track> track);

private:
    std::vector<QString> music_folders_;

    typedef std::map<QString, boost::shared_ptr<Directory> > DirectoryMap;
    DirectoryMap directories_;
};

#endif // LIBRARY_H
