#include "library.h"

#include "util.h"
#include "track.h"
#include "directory.h"
#include "directorywatcher.h"
#include "track.pb.h"
#include "cuefile.h"
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/mpegfile.h>
#include <taglib/flacfile.h>
#include <taglib/xiphcomment.h>
#include <taglib/id3v1tag.h>
#include <taglib/id3v2tag.h>
#include <taglib/tpropertymap.h>
#include <taglib/tfilestream.h>
#include <taglib/id3v2framefactory.h>
#include <boost/scoped_array.hpp>
#include <boost/bind.hpp>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QSettings>
#include <QTimer>
#include <QDateTime>
#include <set>
#include <cstdio>

using namespace std;

const char* library_filename = "media_library";
const int persist_interval = 5 * 60 * 1000;

string LibraryEvent::op2str()
{
    switch(op) {
        case CREATE:
            return "Add";
        case DELETE:
            return "Delete";
        case MODIFY:
            return "Modify";
        case UNKNOWN:
        default:
            return "Unknown";
    }
}

Library::Library(QObject * parent)
    : QThread(parent)
    , mutex_(QMutex::Recursive)
    , quit_(false)
    , rescanning_(false)
    , watching_(false)
    , should_be_working_(true)
    , dirty_(false)
{
    QTimer::singleShot(persist_interval, this, SLOT(persist()));
    getFoldersFromSettings();
    loadFromDisk();
}

Library::~Library()
{
    saveToDisk();
}

void Library::stopRescanning()
{
    QMutexLocker locker(&stop_rescan_mutex_);
    while (rescanning_) {
        stop_rescan_.wait(&stop_rescan_mutex_);
    }
}

void Library::startMonitoring()
{
    QMutexLocker locker(&pause_monitoring_mutex_);
    QList<PTrack> tracks = getTracks();
    emit(libraryChanged(tracks));
    should_be_working_ = true;
    pause_monitoring_.wakeAll();
}

void Library::stopMonitoring()
{
    if (!should_be_working_)
        return;

    should_be_working_ = false;
    stopRescanning();
    stopWatch();
}

void Library::restartMonitoring(bool wipeDatabase)
{
    stopMonitoring();
    if (wipeDatabase) {
        QMutexLocker locker(&mutex_);
        dirty_ = true;
        directories_.clear();
    }
    startMonitoring();
}

void Library::waitForMonitoringStart()
{
    QMutexLocker locker(&pause_monitoring_mutex_);
    while (!should_be_working_) {
        pause_monitoring_.wait(&pause_monitoring_mutex_);
    }
}

void Library::run()
{
    while (!quit_) {
        waitForMonitoringStart();
        qDebug() << "Scanning media files";
        watcher_.reset(new DirectoryWatcher);
        watcher_->setFileCallback(boost::bind(&Library::fileCallback, this, _1, _2));
        watcher_->setDirectoryCallback(boost::bind(&Library::directoryCallback, this, _1, _2));

        if (!quit_) {
            rescan();
        }
        else
            break;
        if (!should_be_working_)
            continue;

        if (!quit_)
            watch();
        else
            break;
    }
}

void Library::quit()
{
    quit_ = true;
    stopMonitoring();
}

void Library::addDirectory(std::shared_ptr<Directory> directory)
{
    QMutexLocker locker(&mutex_);
    directories_.insert(directory->path(), directory);

    // Add child to father
    QFileInfo dir_path(directory->path());
    DirectoryMap::iterator it = directories_.find(dir_path.absolutePath());
    if (it != directories_.end()) {
        it.value()->addSubdirectory(directory);
    }
}

void Library::removeDirectory(QString path)
{
    QMutexLocker locker(&mutex_);
    qDebug() << "Deleting dir" << path;
    DirectoryMap::iterator it = directories_.find(path);
    if (it == directories_.end()) {
//        qDebug() << " Maybe wanted to remove directory " << path << ". No dice";
        return;
    }
    QList<QString> subdirs = it.value()->getSubdirectoriesNames();
    foreach (QString subdir, subdirs) {
        removeDirectory(QFileInfo(QDir(path), subdir).absoluteFilePath());
    }
    if (!rescanning_)
        watcher_->removeWatch(path);

    // Delete tracks from views
    QList<PTrack> tracks = getDirectoryTracks(it.value());
    if (!tracks.empty()) {
        dirty_ = true;
    }
    foreach (PTrack track, tracks) {
        emitLibraryChanged(track, DELETE);
    }
    it.value()->clearFiles();

    // Remove child from father
    QFileInfo info(path);
    DirectoryMap::iterator father_it = directories_.find(info.absolutePath());
    if (father_it != directories_.end()) {
        father_it.value()->removeSubdirectory(info.fileName());
    }
    directories_.erase(it);
    qDebug() << "Done deleting dir" << path;
}

void Library::addFile(PTrack track, bool loading)
{
    QMutexLocker locker(&mutex_);
    // Add to directory
    QFileInfo file_info(track->location);
    DirectoryMap::iterator it = directories_.find(file_info.absolutePath());
    if (it != directories_.end()) {
        std::shared_ptr<Directory> directory = it.value();
        if (!loading) {
            // Update audio info for cue
            if (track->isCue()) {
                track->updateAudioInfo(directory->getFile(track->cueTrackLocation()));
            } else {
                for (PTrack other : directory->getTracks()) {
                    if (other->isCue() && other->cueTrackLocation() == file_info.fileName()) {
                        other->updateAudioInfo(track);
                        emitLibraryChanged(other, MODIFY);
                    }
                }
            }
            dirty_ = true;
            emitLibraryChanged(track, CREATE);
        }
        directory->addFile(track);
    } else {
        qDebug() << "Library::addFile tried to add a file for an unadded directory!!111";
    }
}

void Library::removeFile(QString path)
{
    QMutexLocker locker(&mutex_);
    QFileInfo file_info(path);
    DirectoryMap::iterator it = directories_.find(file_info.absolutePath());
    if (it != directories_.end()) {
        PTrack track = it.value()->removeFile(file_info.fileName());
        if (track) {
            dirty_ = true;
            emitLibraryChanged(track, DELETE);
        }
    } else {
        qDebug() << "Library::addFile tried to delete a file for an unadded directory!!111" << path;
    }
}

void Library::loadFromDisk()
{
    QString filename = settingsDirFilePath(library_filename);
    FILE *f = std::fopen(filename.toStdString().c_str(), "rb");
    if (f == NULL) {
        qDebug() << "Cannot open library file" << filename << strerror(errno);
        return;
    }

    proto::Library plibrary;
    int len = 0;
    if (fread(&len, 4, 1, f) != 1) {
        qDebug() << "Error reading library file" << filename;
        return;
    }

    boost::scoped_array<char> tmp(new char[len]);
    if (fread(tmp.get(), len, 1, f) != 1) {
        qDebug() << "Error reading library file" << filename;
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
        addFile(track, true);
    }
    qDebug() << "Read metadata for" << plibrary.tracks_size() << "tracks in" << plibrary.directories_size() << "directories from disk";
}

void Library::saveToDisk()
{
    if (!dirty_)
        return;

    qDebug() << "Saving library file...";
    QString filename = settingsDirFilePath(library_filename);
    FILE *f = std::fopen(filename.toStdString().c_str(), "wb");
    if (f == NULL) {
        qDebug() << "Cannot open library file" << filename << strerror(errno);
        return;
    }

    proto::Library plibrary;
    for (DirectoryMap::const_iterator it = directories_.begin(); it != directories_.end(); ++it) {
        proto::Directory* dir = plibrary.add_directories();
        dir->Clear();
        dir->set_location(it.key().toUtf8().constData());
        dir->set_mtime(it.value()->mtime());
        it.value()->addFilesToProto(plibrary);
    }

    int len = plibrary.ByteSize();
    boost::scoped_array<char> tmp(new char[len]);
    plibrary.SerializeToArray(tmp.get(), len);

    if ((fwrite(&len, 4, 1, f) == 1) && (fwrite(tmp.get(), len, 1, f) == 1)) {
        dirty_ = false;
    } else {
        qDebug() << "Error writing library file" << filename;
    }
    fclose(f);
}

void Library::persist()
{
    QMutexLocker locker(&mutex_);
    saveToDisk();
    QTimer::singleShot(persist_interval, this, SLOT(persist()));
}

void Library::scanDirectory(const QString& path)
{
    QDir::Filters directory_filter = QDir::Dirs | QDir::Files | QDir::Readable | QDir::Hidden | QDir::NoDotAndDotDot;
    if (stopRescan())
        return;
    DirectoryMap::const_iterator it = directories_.find(path);
    watcher_->addWatch(path);
    // This mtime may be currentTime() because of god-knows-how-stat-and-move-dir-works-in-linux.
    // So we use an incorrect mtime. Not a problem since we use <= to determine if the the directory changed.
    uint mtime = QFileInfo(path).lastModified().toTime_t();

    if (it != directories_.end()) {
        shared_ptr<Directory> directory = it.value();
        // Nothing changed in this dir
        if (mtime <= directory->mtime()) {
            QList<QString> subdirs = directory->getSubdirectoriesNames();
            foreach (QString subdir, subdirs) {
                scanDirectory(QFileInfo(path, subdir).absoluteFilePath());
                if (stopRescan())
                    return;
            }
        // Stuff changed
        } else {
            QList<QString> newly_created_subdirs;
            shared_ptr<Directory> new_directory;
            {
                QMutexLocker locker(&mutex_);
                dirty_ = true;
                QSet<QString> old_files = directory->getFileSet();
                new_directory = shared_ptr<Directory>(new Directory(path, 0));
                addDirectory(new_directory);
                QDir dir(path);
                dir.setFilter(directory_filter);
                QSet<QString> deleted_subdirs = directory->getSubdirectoriesPathsSet();
                foreach (QFileInfo info, dir.entryInfoList()) {
                    if (info.isFile()) {
                        shared_ptr<Track> file = directory->getFile(info.fileName());
                        if (file) {
                            if (file->mtime != info.lastModified().toTime_t()) {
                                file = scanFile(info.filePath());
                                emitLibraryChanged(file, MODIFY);
                            }
                            new_directory->addFile(file);
                            old_files.erase(old_files.find(file->location));
                        } else {
                            file = scanFile(info.filePath());
                            if (file)
                                addFile(file);
                        }
                    } else {
                        shared_ptr<Directory> subdir = directory->getSubdirectory(info.fileName());
                        // Maybe this is a newly monitored directory, but its subdir may be in library already
                        if (!subdir) {
                            DirectoryMap::const_iterator subdir_it = directories_.find(info.absoluteFilePath());
                            if (subdir_it != directories_.end())
                                subdir = *subdir_it;
                        }
                        if (subdir) {
                            new_directory->addSubdirectory(subdir);
                            deleted_subdirs.remove(info.absoluteFilePath());
                        } else {
                            // New directory
                            newly_created_subdirs.append(info.absoluteFilePath());
                        }
                    }
                }
                foreach (QString deleted_file, old_files) {
                    emitLibraryChanged(directory->getFile(QFileInfo(deleted_file).fileName()), DELETE);
                }
                foreach (QString deleted_dir, deleted_subdirs) {
                    // Here removing child from father wont work
                    // because the new_directory does not have deleted_dir as a son since he is freshly crawled
                    // (directory had him as son)
                    removeDirectory(deleted_dir);
                }
                // Add subdirectories so we can set mtime and be done with this directory
                foreach (QFileInfo info, newly_created_subdirs) {
                    addDirectory(shared_ptr<Directory>(new Directory(info.filePath(), 0)));
                }
                new_directory->setMtime(mtime);
            }
            foreach (QString subdir_path, new_directory->getSubdirectoriesPathsSet()) {
                scanDirectory(subdir_path);
            }
        }
    // Full scan
    } else {
        shared_ptr<Directory> directory = shared_ptr<Directory>(new Directory(path, 0));
        addDirectory(directory);
        QDir dir(path);
        dir.setFilter(directory_filter);
        QList<QString> subdirs;
        foreach (QFileInfo info, dir.entryInfoList()) {
            if (stopRescan())
                return;
            if (info.isFile()) {
                PTrack track = scanFile(info.filePath());
                if (track)
                    addFile(track);
            } else {
                if (directories_.find(info.absoluteFilePath()) == directories_.end())
                    addDirectory(shared_ptr<Directory>(new Directory(info.filePath(), 0)));
                subdirs.append(info.absoluteFilePath());
            }
        }
        {
            QMutexLocker locker(&mutex_);
            directory->setMtime(mtime);
        }
        foreach (QFileInfo info, subdirs) {
            scanDirectory(info.filePath());
        }
    }
}

template<class T>
void set_tag_and_properties(T tag, TagLib::Tag **tag_ptr, TagLib::PropertyMap& properties)
{
    if (*tag_ptr && !(*tag_ptr)->title().isEmpty() && !(*tag_ptr)->artist().isEmpty())
        return;
    if (tag) {
        *tag_ptr = tag;
        properties = tag->properties();
    }
}

PTrack Library::scanFile(const QString& path)
{
    if (path.toLower().endsWith(".cue")) {
        return scanCue(path);
    }

    qDebug() << "Scanning file" << path;
    QByteArray encodedName = QFile::encodeName(path);

    TagLib::AudioProperties *audioProperties = nullptr;
    std::unique_ptr<TagLib::File> file;
    TagLib::FileRef fileref;
    TagLib::Tag *tag = nullptr;
    TagLib::PropertyMap properties;
    TagLib::FileStream stream(encodedName.constData(), true);

    shared_ptr<Track> track;

    if (path.toLower().endsWith(".mp3")) {
        TagLib::MPEG::File* mpegFile = new TagLib::MPEG::File(&stream, TagLib::ID3v2::FrameFactory::instance());
        file.reset(mpegFile);
        if (!file->isValid())
            return track;
        set_tag_and_properties(mpegFile->ID3v2Tag(), &tag, properties);
        set_tag_and_properties(mpegFile->ID3v1Tag(), &tag, properties);
    } else if (path.toLower().endsWith(".flac")) {
        TagLib::FLAC::File* flacFile = new TagLib::FLAC::File(&stream, TagLib::ID3v2::FrameFactory::instance());
        file.reset(flacFile);
        if (!file->isValid())
            return track;
        set_tag_and_properties(flacFile->xiphComment(), &tag, properties);
        set_tag_and_properties(flacFile->ID3v2Tag(), &tag, properties);
        set_tag_and_properties(flacFile->ID3v1Tag(), &tag, properties);
    }
    if (file) {
        audioProperties = file->audioProperties();
    } else {
        fileref = TagLib::FileRef(&stream, true);
        if (fileref.isNull())
            return track;
        tag = fileref.tag();
        audioProperties = fileref.audioProperties();
    }

    track.reset(new Track());
    track->location = path;
    if (tag) {
        track->metadata["title"] = TStringToQString(tag->title());
        track->metadata["artist"] = TStringToQString(tag->artist());
        track->metadata["album"] = TStringToQString(tag->album());
        track->metadata["comment"] = TStringToQString(tag->comment());
        track->metadata["genre"] = TStringToQString(tag->genre());
        track->metadata["year"] = QString::number(tag->year());
        track->metadata["track"] = QString::number(tag->track());

        // Get album artist and replaygain
//         qDebug() << "Metadata for " << track->location;
//         for (auto item : properties) {
//             qDebug() << TStringToQString(item.first) << " " << TStringToQString(item.second.front());
//         }
        std::map<QString, QString> tags{{"ALBUM ARTIST", "album artist"}, {"ALBUMARTIST", "album artist"}, {"UNSYNCED LYRICS", "lyrics"}, {"LYRICS", "lyrics"}};
        std::set<QString> replayGainTags = {"REPLAYGAIN_ALBUM_GAIN", "REPLAYGAIN_ALBUM_PEAK", "REPLAYGAIN_TRACK_GAIN", "REPLAYGAIN_TRACK_PEAK"};
        for (const auto& rgtag : replayGainTags) {
            tags.insert(std::make_pair(rgtag, rgtag));
        }

        for (const auto& tag_pair: tags) {
            TagLib::PropertyMap::Iterator it = properties.find(tag_pair.first.toStdString().c_str());
            if (it != properties.end() && !it->second.isEmpty()) {
                QString value = TStringToQString(it->second.front());
                if (!value.isEmpty())
                    track->metadata[tag_pair.second] = value;
            }
        }
    }
    if (audioProperties) {
        track->audioproperties.length = audioProperties->length();
        track->audioproperties.bitrate = audioProperties->bitrate();
        track->audioproperties.samplerate = audioProperties->sampleRate();
        track->audioproperties.channels = audioProperties->channels();
    }

    track->mtime = QFileInfo(path).lastModified().toTime_t();
/*        qDebug() << "LIBRARY: " << track->metadata["artist"] << " " << track->metadata["title"] <<
            " " << track->audioproperties.length;*/
    return track;
}

PTrack Library::scanCue(const QString& path)
{
    qDebug() << "Scanning cue" << path;
    shared_ptr<Track> track;
    try {
        CueFile cue(path);
        track.reset(new Track());
        track->location = path;
        track->mtime = QFileInfo(path).lastModified().toTime_t();
        track->metadata = cue.getMetadata();
        QList<PTrack> tracks;
        for (int i = 1; i <= cue.tracks(); ++i) {
            PTrack subtrack(new Track);
            subtrack->location = cue.getLocation(i);
            subtrack->audioproperties.length = cue.getLength(i);
            subtrack->metadata = cue.getMetadata(i);
            tracks.push_back(subtrack);
        }
        track->setCueTracks(tracks);
    } catch (...) {
        qDebug() << "Failed to scan cue" << path;
    }
    return track;
}

void Library::setMusicFolders(QStringList folders)
{
    music_folders_ = folders;
    music_folders_.removeAll("");
    setFoldersInSettings();
}

void Library::rescan()
{
    qDebug() << "Started rescan";
    {
        QMutexLocker locker(&stop_rescan_mutex_);
        rescanning_ = true;
    }
    // Delete all dirs that are not monitored
    QList<QString> old_dirs;
    foreach (QString dir, directories_.keys()) {
        bool deleted = true;
        foreach (QString path, music_folders_) {
            if (dir.startsWith(path)) {
                deleted = false;
                break;
            }
        }
        if (deleted)
            old_dirs.append(dir);
    }
    foreach (QString old_dir, old_dirs) {
        removeDirectory(old_dir);
    }

    foreach (QString path, music_folders_) {
        scanDirectory(path);
        if (stopRescan())
            break;
    }
    {
        QMutexLocker locker(&stop_rescan_mutex_);
        rescanning_ = false;
    }
    stop_rescan_.wakeAll();
    qDebug() << "Done rescan";
}

void Library::watch()
{
    qDebug() << "Started watch";
    watching_ = true;
    watcher_->watch();
}

void Library::stopWatch()
{
    if (watching_) {
        watcher_->stop();
        qDebug() << "Done watch";
        watching_ = false;
    }
}

void Library::dumpDatabase() const
{
    foreach (QString music_folder, music_folders_) {
        DirectoryMap::const_iterator it = directories_.find(music_folder);
        if (it != directories_.end())
            it.value()->dump();
    }
}

void Library::directoryCallback(QString path, LibraryEventType event)
{
    QMutexLocker locker(&mutex_);
    qDebug() << (event == CREATE ? "DIR ADD " : "DIR RM ") << path;
    if (event == CREATE) {
        scanDirectory(path);
    } else if (event == DELETE) {
        removeDirectory(path);
    }
    //dumpDatabase();
}

void Library::fileCallback(QString path, LibraryEventType event)
{
    QMutexLocker locker(&mutex_);
    if (event == CREATE) {
//        qDebug() << "FILE ADD " << path;
        PTrack track = scanFile(path);
        if (track)
            addFile(track);
    } else if (event == DELETE) {
//        qDebug() << "FILE RM " << path;
        removeFile(path);
    } else if (event == MODIFY) {
        QFileInfo fileInfo(path);
        DirectoryMap::iterator it = directories_.find(fileInfo.absolutePath());
        if (it != directories_.end()) {
            PTrack oldTrack = it.value()->getFile(fileInfo.fileName());
            if (oldTrack) {
//                    qDebug() << "FILE MODIFY " << path;
                if (oldTrack->mtime <= fileInfo.lastModified().toTime_t()) {
                    PTrack track = scanFile(path);
                    if (track) {
                        it.value()->addFile(track);
                        dirty_ = true;
                        emitLibraryChanged(track, MODIFY);
                    }
                }
            } else {
                // We tried taglib-reading this but it failed
                // maybe now we have more data so try again
                qDebug() << "Reading again with taglib" << path;
                PTrack track = scanFile(path);
                if (track)
                    addFile(track);
            }
        } else {
            qDebug() << "Couldn't find the directory where the file was modified";
        }
    }
    //dumpDatabase();
}

QStringList Library::getMusicFolders()
{
    return QStringList(music_folders_);
}

QList<shared_ptr<Track>> Library::getTracks()
{
    QMutexLocker locker(&mutex_);
    QList<shared_ptr<Track>> result;
    for (DirectoryMap::iterator it = directories_.begin(); it != directories_.end(); ++it) {
        result.append(getDirectoryTracks(it.value()));
    }
    return result;
}

void Library::getFoldersFromSettings()
{
    QSettings settings;
    music_folders_ = settings.value("library/folders").toStringList();
    music_folders_.removeAll("");
}

void Library::setFoldersInSettings()
{
    QSettings settings;
    settings.setValue("library/folders", music_folders_);
}

void Library::emitLibraryChanged(PTrack track, LibraryEventType type)
{
    if (track->isCue()) {
        if (type == LibraryEventType::DELETE || type == LibraryEventType::MODIFY) {
            for (PTrack cuetrack : track->getCueTracks())
                emit libraryChanged(LibraryEvent(cuetrack, LibraryEventType::DELETE));
        }
        if (type == LibraryEventType::MODIFY || type == LibraryEventType::CREATE) {
            for (PTrack cuetrack : track->getCueTracks())
                emit libraryChanged(LibraryEvent(cuetrack, LibraryEventType::CREATE));
        }
    } else
        emit libraryChanged(LibraryEvent(track, type));
}

QList<PTrack> Library::getDirectoryTracks(shared_ptr<Directory> directory)
{
    QList<PTrack> tracks;
    for (PTrack track : directory->getTracks()) {
        if (track->isCue()) {
            tracks.append(track->getCueTracks());
        } else
            tracks.append(track);
    }
    return tracks;
}

void Library::dirtyHack(PTrack track)
{
    dirty_ = true;
    emitLibraryChanged(track, MODIFY);
}
