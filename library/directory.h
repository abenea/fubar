#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "track.h"
#include <QString>
#include <QList>
#include <QMap>
#include <boost/shared_ptr.hpp>
#include <vector>

namespace proto {
    class Directory;
    class Library;
}

class Directory {
public:
    Directory(proto::Directory pDirectory);
    Directory(QString path, int mtime);

    void addSubdirectory(boost::shared_ptr<Directory> directory);
    void removeSubdirectory(QString subdirName);
    void addFile(boost::shared_ptr<Track> file);
    boost::shared_ptr<Track> removeFile(QString fileName);
    void addFilesFromDirectory(boost::shared_ptr<Directory> directory);

    std::vector<QString> getSubdirectories();
    boost::shared_ptr<Track> getFile(QString name);
    QList<boost::shared_ptr<Track> > getTracks();
    void clearFiles();

    void addFilesToProto(proto::Library& library);
    void dump();

    QString path() { return location_; }
    int mtime() { return mtime_; }

private:
    // Change to QMap?
    typedef QMap<QString, boost::shared_ptr<Directory> > SubdirectoryMap;
    typedef QMap<QString, boost::shared_ptr<Track> > FileMap;

    QString location_;
    int mtime_;

    SubdirectoryMap subdirs_;
    FileMap files_;
};


#endif // DIRECTORY_H
