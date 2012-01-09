#include "directory.h"
#include "track.pb.h"
#include <QFileInfo>
#include <QDebug>

using namespace std;
using namespace boost;


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

boost::shared_ptr<Track> Directory::removeFile(QString fileName)
{
    boost::shared_ptr<Track> track;
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

std::vector<QString> Directory::getSubdirectories()
{
    std::vector<QString> subdirs;
    subdirs.reserve(subdirs_.size());
    for (SubdirectoryMap::const_iterator it = subdirs_.begin(); it != subdirs_.end(); ++it) {
        subdirs.push_back(it.key());
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

QList<boost::shared_ptr<Track> > Directory::getTracks()
{
    QList<boost::shared_ptr<Track> > tracks;
    foreach (boost::shared_ptr<Track> track, files_) {
        tracks.append(track);
    }
    return tracks;
}

void Directory::clearFiles()
{
    files_.clear();
}
