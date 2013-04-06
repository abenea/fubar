#include "library.h"

#include "util.h"
#include "playlist.h"
#include "track.pb.h"
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/id3v1tag.h>
#include <taglib/tpropertymap.h>
#include <boost/foreach.hpp>
#include <boost/scoped_array.hpp>
#include <boost/bind.hpp>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QSettings>
#include <QTimer>
#include <QDateTime>
#include <set>

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
    QList<std::shared_ptr<Track>> tracks = getTracks();
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
    QList<PTrack> tracks = it.value()->getTracks();
    if (!tracks.empty()) {
        dirty_ = true;
    }
    foreach (PTrack track, tracks) {
        emit libraryChanged(LibraryEvent(track, DELETE));
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

void Library::addFile(std::shared_ptr<Track> track)
{
    QMutexLocker locker(&mutex_);
    // Add to directory
    QFileInfo file_info(track->location);
    DirectoryMap::iterator it = directories_.find(file_info.absolutePath());
    if (it != directories_.end()) {
        dirty_ = true;
        it.value()->addFile(track);
        emit libraryChanged(LibraryEvent(track, CREATE));
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
        std::shared_ptr<Track> track = it.value()->removeFile(file_info.fileName());
        if (track) {
            dirty_ = true;
            emit libraryChanged(LibraryEvent(track, DELETE));
        }
    } else {
        qDebug() << "Library::addFile tried to delete a file for an unadded directory!!111" << path;
    }
}

void Library::loadFromDisk()
{
    const char* filename = settingsDirFilePath(library_filename);
    FILE *f = std::fopen(filename, "rb");
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
        addFile(track);
    }
    qDebug() << "Read metadata for" << plibrary.tracks_size() << "tracks in" << plibrary.directories_size() << "directories from disk";
    dirty_ = false;
}

void Library::saveToDisk()
{
    if (!dirty_)
        return;

    qDebug() << "Saving library file...";
    const char* filename = settingsDirFilePath(library_filename);
    FILE *f = std::fopen(filename, "wb");
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
                                emit libraryChanged(LibraryEvent(file, MODIFY));
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
                    emit libraryChanged(LibraryEvent(directory->getFile(QFileInfo(deleted_file).fileName()), DELETE));
                }
                foreach (QString deleted_dir, deleted_subdirs) {
                    // Here removing child from father wont work as the new father has no son like deleted dir_path
                    // since he is freshly crawled
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
                std::shared_ptr<Track> track = scanFile(info.filePath());
                if (track)
                    addFile(track);
            } else {
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

std::shared_ptr<Track> Library::scanFile(const QString& path)
{
    qDebug() << "Scanning file" << path;
    QByteArray encodedName = QFile::encodeName(path);

    TagLib::FileRef fileref;
    std::unique_ptr<TagLib::MPEG::File> mpegFile;
    TagLib::AudioProperties *audioProperties = 0;
    TagLib::Tag *tag = 0;

    shared_ptr<Track> track;

    if (path.toLower().endsWith(".mp3")) {
        mpegFile.reset(new TagLib::MPEG::File(encodedName));
        if (!mpegFile->isValid())
            return track;
        tag = mpegFile->ID3v2Tag();
        if (!tag || tag->title().isEmpty() || tag->artist().isEmpty())
            tag = mpegFile->ID3v1Tag();
        audioProperties = mpegFile->audioProperties();
    } else {
        fileref = TagLib::FileRef(encodedName.constData(), true);
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
    }
    if (audioProperties) {
        track->audioproperties.length = audioProperties->length();
        track->audioproperties.bitrate = audioProperties->bitrate();
        track->audioproperties.samplerate = audioProperties->sampleRate();
        track->audioproperties.channels = audioProperties->channels();
    }

    // mp3: Get album artist and replaygain
    if (mpegFile && mpegFile->isValid()) {
        TagLib::ID3v2::Tag* id3v2 = mpegFile->ID3v2Tag();
        if (id3v2) {
            auto properties = id3v2->properties();
//             qDebug() << "Metadata for " << track->location;
//             for (auto item : properties) {
//                 qDebug() << TStringToQString(item.first) << " " << TStringToQString(item.second.front());
//             }

            std::map<QString, QString> mp3Tags{{"ALBUM ARTIST", "album artist"}, {"ALBUMARTIST", "album artist"}};
            std::set<QString> replayGainTags = {"REPLAYGAIN_ALBUM_GAIN", "REPLAYGAIN_ALBUM_PEAK", "REPLAYGAIN_TRACK_GAIN", "REPLAYGAIN_TRACK_PEAK"};
            for (const auto& rgtag : replayGainTags) {
                mp3Tags.insert(std::make_pair(rgtag, rgtag));
            }

            for (const auto& mp3tag: mp3Tags) {
                TagLib::PropertyMap::Iterator it = properties.find(mp3tag.first.toStdString().c_str());
                if (it != properties.end() && !it->second.isEmpty()) {
                    QString tmp = TStringToQString(it->second.front());
                    if (!tmp.isEmpty())
                        track->metadata[mp3tag.second] = tmp;
                }
            }
        }
    }

    track->mtime = QFileInfo(path).lastModified().toTime_t();
/*        qDebug() << "LIBRARY: " << track->metadata["artist"] << " " << track->metadata["title"] <<
            " " << track->audioproperties.length;*/
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
        std::shared_ptr<Track> track = scanFile(path);
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
                if (oldTrack->mtime != fileInfo.lastModified().toTime_t()) {
//                    qDebug() << "FILE MODIFY " << path;
                    PTrack track = scanFile(path);
                    it.value()->addFile(track);
                    dirty_ = true;
                    emit libraryChanged(LibraryEvent(track, MODIFY));
                }
            } else {
                // We tried taglib-reading this but it failed
                // maybe now we have more data so try again
                qDebug() << "Reading again with taglib" << path;
                std::shared_ptr<Track> track = scanFile(path);
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

QList< shared_ptr< Track > > Library::getTracks()
{
    QMutexLocker locker(&mutex_);
    QList<shared_ptr<Track> > result;
    for (DirectoryMap::iterator it = directories_.begin(); it != directories_.end(); ++it) {
        result.append(it.value()->getTracks());
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

#include "library.moc"
