#pragma once

#include "track_forward.h"
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <memory>

namespace proto {
class Directory;
class Library;
} // namespace proto

class Directory {
public:
    Directory(proto::Directory pDirectory);
    Directory(QString path, int mtime);

    void addSubdirectory(std::shared_ptr<Directory> directory);
    void removeSubdirectory(QString subdirName);
    void addFile(PTrack file);
    PTrack removeFile(QString fileName);
    void addFilesFromDirectory(std::shared_ptr<Directory> directory);

    QList<QString> getSubdirectoriesNames();
    PTrack getFile(QString name);
    std::shared_ptr<Directory> getSubdirectory(QString name);
    QList<PTrack> getTracks();
    QSet<QString> getFileSet();
    QSet<QString> getSubdirectoriesPathsSet();
    void clearFiles();

    void addFilesToProto(proto::Library &library);
    void dump();

    QString path() { return location_; }
    uint mtime() { return mtime_; }
    void setMtime(uint mtime) { mtime_ = mtime; }

private:
    typedef QMap<QString, std::shared_ptr<Directory>> SubdirectoryMap;
    typedef QMap<QString, PTrack> FileMap;

    QString location_;
    uint mtime_;

    SubdirectoryMap subdirs_;
    FileMap files_;
};
