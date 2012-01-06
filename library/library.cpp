#include "library.h"

#include "playlist.h"
#include "track.pb.h"
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <boost/foreach.hpp>
#include <boost/scoped_array.hpp>
#include <boost/bind.hpp>
#include <QDir>
#include <QDebug>
#include <qdatetime.h>
#include <cstdio>

using namespace std;
using namespace boost;

const char* library_filename = "media_library";


Library::Library()
{
    loadFromDisk();
}

Library::~Library()
{
    saveToDisk();
}

void Library::addDirectory(boost::shared_ptr<Directory> directory)
{
    directories_.insert(make_pair(directory->path(), directory));

    // Add child to father
    QFileInfo dir_path(directory->path());
    DirectoryMap::iterator it = directories_.find(dir_path.absolutePath());
    if (it != directories_.end()) {
        it->second->addSubdirectory(directory);
    }
}

void Library::removeDirectory(QString path)
{
    DirectoryMap::iterator it = directories_.find(path);
    if (it == directories_.end()) {
        qDebug() << "Wanted to remove directory " << path << ". No dice";
        return;
    }
    std::vector<QString> subdirs = it->second->getSubdirectories();
    BOOST_FOREACH (QString subdir, subdirs) {
        removeDirectory(QFileInfo(QDir(path), subdir).absoluteFilePath());
    }
    watcher_->removeWatch(path);

    // Remove child from father
    QFileInfo info(path);
    DirectoryMap::iterator father_it = directories_.find(info.absolutePath());
    if (father_it
        != directories_.end()) {
        father_it->second->removeSubdirectory(info.fileName());
    }
    directories_.erase(it);
}

void Library::addFile(shared_ptr<Track> track)
{
    // Add to directory
    QFileInfo file_info(track->location);
    std::map<QString, boost::shared_ptr<Directory> >::iterator it = directories_.find(file_info.absolutePath());
    if (it != directories_.end()) {
        it->second->addFile(track);
    } else {
        qDebug() << "Library::addFile tried to add a file for an unadded directory!!111";
    }
}

void Library::removeFile(QString path)
{
    QFileInfo file_info(path);
    std::map<QString, boost::shared_ptr<Directory> >::iterator it = directories_.find(file_info.absolutePath());
    if (it != directories_.end()) {
        it->second->removeFile(file_info.fileName());
    } else {
        qDebug() << "Library::addFile tried to add a file for an unadded directory!!111" << path;
    }
}

void Library::loadFromDisk()
{
    FILE *f = std::fopen(library_filename, "rb");
    if (f == NULL)
        return;

    proto::Library plibrary;
    int len = 0;
    if (fread(&len, 4, 1, f) != 1)
        return;

    scoped_array<char> tmp(new char[len]);
    if (fread(tmp.get(), len, 1, f) != 1) {
        qDebug() << "OOps, failed read for pb";
        return;
    }
    fclose(f);

    if (!plibrary.ParseFromArray(tmp.get(), len))
        return;

    for (int i = 0; i < plibrary.directories_size(); ++i) {
        const proto::Directory& pDirectory = plibrary.directories(i);
        addDirectory(shared_ptr<Directory>(new Directory(pDirectory)));
    }
    for (int i = 0; i < plibrary.tracks_size(); ++i) {
        shared_ptr<Track> track(new Track(plibrary.tracks(i)));
        addFile(track);
    }
}

void Library::saveToDisk()
{
    FILE *f = std::fopen(library_filename, "wb");
    if (f == NULL)
        return;

    proto::Library plibrary;
    for (DirectoryMap::const_iterator it = directories_.begin(); it != directories_.end(); ++it) {
        proto::Directory* dir = plibrary.add_directories();
        dir->Clear();
        dir->set_location(it->first.toUtf8().constData());
        dir->set_mtime(it->second->mtime());
        it->second->addFilesToProto(plibrary);
    }

    int len = plibrary.ByteSize();
    scoped_array<char> tmp(new char[len]);
    plibrary.SerializeToArray(tmp.get(), len);

    if (fwrite(&len, 4, 1, f) != 1) {
        fclose(f);
        return;
    }
    if (fwrite(tmp.get(), len, 1, f) != 1) {
        qDebug() << "Cannot write media library";
    }

    fclose(f);
}

void Library::scanDirectory(const QString& path)
{
    shared_ptr<Directory> directory = shared_ptr<Directory>(new Directory(path, QFileInfo(path).lastModified().toTime_t()));
    addDirectory(directory);
    watcher_->addWatch(path);

    DirectoryMap::const_iterator it = old_directories_.find(path);
    if (it != old_directories_.end()) {
        shared_ptr<Directory> old_directory = it->second;
        // Nothing changed in this dir
        if (directory->mtime() == old_directory->mtime()) {
            directory->addFilesFromDirectory(old_directory);
            std::vector<QString> subdirs = old_directory->getSubdirectories();
            BOOST_FOREACH (QString& subdir, subdirs) {
                scanDirectory(QFileInfo(path, subdir).absoluteFilePath());
            }
        // Stuff changed
        } else {
            QDir dir(path);
            dir.setFilter(QDir::Dirs | QDir::Files | QDir::Readable | QDir::Hidden | QDir::NoDotAndDotDot);
            foreach (QFileInfo info, dir.entryInfoList()) {
                if (info.isFile()) {
                    bool rescan = true;
                    if (old_directory) {
                        shared_ptr<Track> file = old_directory->getFile(info.fileName());
                        if (file and file->mtime == info.lastModified().toTime_t()) {
                            directory->addFile(file);
                            rescan = false;
                        }
                    }
                    if (rescan)
                        scanFile(info.filePath());
                } else {
                    scanDirectory(info.filePath());
                }
            }
        }
    // Full scan
    } else {
        QDir dir(path);
        dir.setFilter(QDir::Dirs | QDir::Files | QDir::Readable | QDir::Hidden | QDir::NoDotAndDotDot);
        foreach (QFileInfo info, dir.entryInfoList()) {
            if (info.isFile()) {
                scanFile(info.filePath());
            } else {
                scanDirectory(info.filePath());
            }
        }
    }
}

void Library::scanFile(const QString& path)
{
    //qDebug() << "addFile " << path;
    TagLib::FileRef fileref;
    QByteArray encodedName = QFile::encodeName(path);
    fileref = TagLib::FileRef(encodedName.constData(), true);

    if (!fileref.isNull()) {
        shared_ptr<Track> track(new Track());
        track->location = path;
        if (TagLib::Tag *tag = fileref.tag()) {
            track->metadata["title"] = TStringToQString(tag->title());
            track->metadata["artist"] = TStringToQString(tag->artist());
            track->metadata["album"] = TStringToQString(tag->album());
            track->metadata["comment"] = TStringToQString(tag->comment());
            track->metadata["genre"] = TStringToQString(tag->genre());
            track->metadata["year"] = QString::number(tag->year());
            track->metadata["track"] = QString::number(tag->track());
        }
        if (TagLib::AudioProperties *audioProperties = fileref.audioProperties()) {
            track->audioproperties.length = audioProperties->length();
            track->audioproperties.bitrate = audioProperties->bitrate();
            track->audioproperties.samplerate = audioProperties->sampleRate();
            track->audioproperties.channels = audioProperties->channels();
        }
        track->mtime = QFileInfo(path).lastModified().toTime_t();
        addFile(track);
    }
}

void Library::setMusicFolders(const vector<QString>& folders)
{
    music_folders_ = folders;
    qDebug() << "Scanning media files";
    watcher_.reset(new DirectoryWatcher);
    watcher_->setFileCallback(boost::bind(&Library::fileCallback, this, _1, _2));
    watcher_->setDirectoryCallback(boost::bind(&Library::directoryCallback, this, _1, _2));
    scan();
}

void Library::scan()
{
    old_directories_.clear();
    directories_.swap(old_directories_);
    BOOST_FOREACH (QString path, music_folders_) {
        scanDirectory(path);
    }
}

void Library::watch()
{
    watcher_->watch();
}

void Library::dumpDatabase() const
{
    BOOST_FOREACH (QString music_folder, music_folders_) {
        DirectoryMap::const_iterator it = directories_.find(music_folder);
        if (it != directories_.end())
            it->second->dump();
    }
}

void Library::directoryCallback(QString path, WatchEvent event)
{
    qDebug() << (event == CREATE ? "DIR ADD " : "DIR RM ") << path;
    if (event == CREATE) {
        scanDirectory(path);
    } else if (event == DELETE) {
        removeDirectory(path);
    }
    //dumpDatabase();
}

void Library::fileCallback(QString path, WatchEvent event)
{
    qDebug() << (event == CREATE ? "FILE ADD " : "FILE RM ") << path;
    if (event == CREATE) {
        scanFile(path);
    } else if (event == DELETE) {
        removeFile(path);
    }
//  dumpDatabase();
}
