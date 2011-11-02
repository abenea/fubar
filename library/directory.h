#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "track.h"
#include <QString>
#include <boost/shared_ptr.hpp>

namespace proto {
    class Directory;
    class Library;
}

class Directory {
public:
    Directory(proto::Directory pDirectory);
    Directory(QString path, int mtime);

    void addSubdirectory(boost::shared_ptr<Directory> directory);
    void addFile(boost::shared_ptr<Track> file);

    void addFilesToProto(proto::Library& library);

    void dump();

    QString path() { return location_; }
    int mtime() { return mtime_; }

private:
    // Change to QMap?
    typedef std::map<QString, boost::shared_ptr<Directory> > SubdirectoryMap;
    typedef std::map<QString, boost::shared_ptr<Track> > FileMap;

    QString location_;
    int mtime_;

    SubdirectoryMap subdirs_;
    FileMap files_;
};


#endif // DIRECTORY_H
