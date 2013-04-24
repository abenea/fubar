#include "directory.h"
#include "track.h"
#include "track.pb.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>

using namespace std;


Directory::Directory(proto::Directory pDirectory)
    : location_(QString::fromUtf8(pDirectory.location().c_str())),
    mtime_(pDirectory.mtime())
{

}

Directory::Directory(QString path, int mtime)
    : location_(path),
    mtime_(mtime)
{
}

void Directory::addFile(shared_ptr<Track> file)
{
    files_.insert(QFileInfo(file->location).fileName(), file);
}

PTrack Directory::removeFile(QString fileName)
{
    PTrack track;
    FileMap::iterator it = files_.find(fileName);
    if (it != files_.end()) {
        track = it.value();
        files_.erase(it);
    } else {
        // May be a non-audio file
        qDebug() << "Tried to delete " << fileName << " from " << location_ << ". No dice";
    }
    return track;
}

void Directory::addSubdirectory(shared_ptr<Directory> directory)
{
    subdirs_.insert(QFileInfo(directory->location_).fileName(), directory);
}

void Directory::removeSubdirectory(QString subdirName)
{
    SubdirectoryMap::iterator it = subdirs_.find(subdirName);
    if (it != subdirs_.end())
        subdirs_.erase(it);
    else
        qDebug() << location_ << " wanted to remove subdir " << subdirName << ". No dice";
}

void Directory::addFilesToProto(proto::Library& library)
{
    for (FileMap::const_iterator it = files_.begin(); it != files_.end(); ++it) {
        proto::Track* track = library.add_tracks();
        it.value()->fillProtoTrack(*track);
    }
}

void Directory::dump()
{
    foreach (FileMap::mapped_type p, files_) {
        printf("%s\n", p->location.toStdString().c_str());
    }
    foreach (SubdirectoryMap::mapped_type p, subdirs_) {
        printf("%s\n", p->path().toStdString().c_str());
        p->dump();
    }
}

void Directory::addFilesFromDirectory(shared_ptr<Directory> directory)
{
    files_ = directory->files_;
}

QList<QString> Directory::getSubdirectoriesNames()
{
    QList<QString> subdirs;
    subdirs.reserve(subdirs_.size());
    foreach (QString dir, subdirs_.keys()) {
        subdirs.append(dir);
    }
    return subdirs;
}

shared_ptr<Track> Directory::getFile(QString name)
{
    shared_ptr<Track> result;
    FileMap::iterator it = files_.find(name);
    if (it != files_.end())
        result = it.value();
    return result;
}

shared_ptr<Directory> Directory::getSubdirectory(QString name)
{
    shared_ptr<Directory> result;
    SubdirectoryMap::iterator it = subdirs_.find(name);
    if (it != subdirs_.end())
        result = it.value();
    return result;
}

QList<PTrack > Directory::getTracks()
{
    QList<PTrack > tracks;
    foreach (PTrack track, files_) {
        tracks.append(track);
    }
    return tracks;
}

QSet<QString> Directory::getFileSet()
{
    QSet<QString> files;
    foreach (PTrack track, files_) {
        files.insert(track->location);
    }
    return files;
}

QSet<QString> Directory::getSubdirectoriesPathsSet()
{
    QSet<QString> subdirectories;
    foreach (QString path, subdirs_.keys()) {
        subdirectories.insert(QFileInfo(QDir(location_), path).absoluteFilePath());
    }
    return subdirectories;
}

void Directory::clearFiles()
{
    files_.clear();
}
