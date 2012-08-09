#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "track.h"
#include <QString>
#include <QList>
#include <QMap>
#include <QSet>

namespace proto {
    class Directory;
    class Library;
}

class Directory {
public:
    Directory(proto::Directory pDirectory);
    Directory(QString path, int mtime);

    void addSubdirectory(std::shared_ptr<Directory> directory);
    void removeSubdirectory(QString subdirName);
    void addFile(std::shared_ptr<Track> file);
    std::shared_ptr<Track> removeFile(QString fileName);
    void addFilesFromDirectory(std::shared_ptr<Directory> directory);

    QList<QString> getSubdirectories();
    std::shared_ptr<Track> getFile(QString name);
    QList<std::shared_ptr<Track> > getTracks();
    QSet<QString> getFileSet();
    QSet<QString> getSubdirectorySet();
    void clearFiles();

    void addFilesToProto(proto::Library& library);
    void dump();

    QString path() { return location_; }
    uint mtime() { return mtime_; }

private:
    // Change to QMap?
    typedef QMap<QString, std::shared_ptr<Directory> > SubdirectoryMap;
    typedef QMap<QString, std::shared_ptr<Track> > FileMap;

    QString location_;
    uint mtime_;

    SubdirectoryMap subdirs_;
    FileMap files_;
};


#endif // DIRECTORY_H
