#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "track.h"
#include <QString>
#include <QList>
#include <QMap>
#include <QSet>
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
    void removeSubdirectory(QString subdirName);
    void addFile(boost::shared_ptr<Track> file);
    boost::shared_ptr<Track> removeFile(QString fileName);
    void addFilesFromDirectory(boost::shared_ptr<Directory> directory);

    QList<QString> getSubdirectories();
    boost::shared_ptr<Track> getFile(QString name);
    QList<boost::shared_ptr<Track> > getTracks();
    QSet<QString> getFileSet();
    QSet<QString> getSubdirectorySet();
    void clearFiles();

    void addFilesToProto(proto::Library& library);
    void dump();

    QString path() { return location_; }
    uint mtime() { return mtime_; }

private:
    // Change to QMap?
    typedef QMap<QString, boost::shared_ptr<Directory> > SubdirectoryMap;
    typedef QMap<QString, boost::shared_ptr<Track> > FileMap;

    QString location_;
    uint mtime_;

    SubdirectoryMap subdirs_;
    FileMap files_;
};


#endif // DIRECTORY_H
