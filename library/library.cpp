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
#include <QSettings>
#include <qdatetime.h>

using namespace std;

const char* library_filename = "media_library";


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
    , quit_(false)
    , rescanning_(false)
    , watching_(false)
    , should_be_working_(true)
{
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

void Library::restartMonitoring()
{
    stopMonitoring();
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

        if (!quit_)
            rescan();
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
    qDebug() << "Deleting dir " << path;
    DirectoryMap::iterator it = directories_.find(path);
    if (it == directories_.end()) {
        qDebug() << "Wanted to remove directory " << path << ". No dice";
        return;
    }
    QList<QString> subdirs = it.value()->getSubdirectories();
    foreach (QString subdir, subdirs) {
        removeDirectory(QFileInfo(QDir(path), subdir).absoluteFilePath());
    }
    if (!rescanning_)
        watcher_->removeWatch(path);

    // Delete tracks from views
    QList<PTrack> tracks = it.value()->getTracks();
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
    qDebug() << "Done deleting dir " << path;
}

void Library::addFile(std::shared_ptr<Track> track)
{
    // Add to directory
    QFileInfo file_info(track->location);
    DirectoryMap::iterator it = directories_.find(file_info.absolutePath());
    if (it != directories_.end()) {
        it.value()->addFile(track);
        emit libraryChanged(LibraryEvent(track, CREATE));
    } else {
        qDebug() << "Library::addFile tried to add a file for an unadded directory!!111";
    }
}

void Library::removeFile(QString path)
{
    QFileInfo file_info(path);
    DirectoryMap::iterator it = directories_.find(file_info.absolutePath());
    if (it != directories_.end()) {
        std::shared_ptr<Track> track = it.value()->removeFile(file_info.fileName());
        if (track) {
            emit libraryChanged(LibraryEvent(track, DELETE));
        }
    } else {
        qDebug() << "Library::addFile tried to delete a file for an unadded directory!!111" << path;
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

    boost::scoped_array<char> tmp(new char[len]);
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
        dir->set_location(it.key().toUtf8().constData());
        dir->set_mtime(it.value()->mtime());
        it.value()->addFilesToProto(plibrary);
    }

    int len = plibrary.ByteSize();
    boost::scoped_array<char> tmp(new char[len]);
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
    if (stopRescan())
        return;
    DirectoryMap::const_iterator it = directories_.find(path);
    watcher_->addWatch(path);
    uint mtime = QFileInfo(path).lastModified().toTime_t();

    if (it != directories_.end()) {
        shared_ptr<Directory> directory = it.value();
        // Nothing changed in this dir
        if (mtime == directory->mtime()) {
            QList<QString> subdirs = directory->getSubdirectories();
            foreach (QString subdir, subdirs) {
                scanDirectory(QFileInfo(path, subdir).absoluteFilePath());
                if (stopRescan())
                    return;
            }
        // Stuff changed
        } else {
            QSet<QString> old_files = directory->getFileSet();
            shared_ptr<Directory> new_directory = shared_ptr<Directory>(new Directory(path, mtime));
            addDirectory(new_directory);
            QDir dir(path);
            dir.setFilter(QDir::Dirs | QDir::Files | QDir::Readable | QDir::Hidden | QDir::NoDotAndDotDot);
            QList<QString> subdirs;
            QSet<QString> old_subdirs = directory->getSubdirectorySet();
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
                    subdirs.append(info.absoluteFilePath());
                    old_subdirs.remove(info.absoluteFilePath());
                }
            }
            foreach (QString deleted_file, old_files) {
                emit libraryChanged(LibraryEvent(directory->getFile(QFileInfo(deleted_file).fileName()), DELETE));
            }
            foreach (QString deleted_dir, old_subdirs) {
                // Here removing child from father wont work as the nrew father has no son like deleted dir_path
                // since he is freshly crawled
                removeDirectory(deleted_dir);
            }
            foreach (QFileInfo info, subdirs) {
                scanDirectory(info.filePath());
            }
        }
    // Full scan
    } else {
        shared_ptr<Directory> directory = shared_ptr<Directory>(new Directory(path, mtime));
        addDirectory(directory);
        QDir dir(path);
        dir.setFilter(QDir::Dirs | QDir::Files | QDir::Readable | QDir::Hidden | QDir::NoDotAndDotDot);
        foreach (QFileInfo info, dir.entryInfoList()) {
            if (stopRescan())
                return;
            if (info.isFile()) {
                std::shared_ptr<Track> track = scanFile(info.filePath());
                if (track)
                    addFile(track);
            } else {
                scanDirectory(info.filePath());
            }
        }
    }
}

std::shared_ptr<Track> Library::scanFile(const QString& path)
{
    qDebug() << "Scanning file " << path;
    TagLib::FileRef fileref;
    QByteArray encodedName = QFile::encodeName(path);
    fileref = TagLib::FileRef(encodedName.constData(), true);

    shared_ptr<Track> track;
    if (!fileref.isNull()) {
        track.reset(new Track());
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
/*        qDebug() << "LIBRARY: " << track->metadata["artist"] << " " << track->metadata["title"] <<
            " " << track->audioproperties.length;*/
        track->accessed_by_taglib = true;
    }
    return track;
}

void Library::setMusicFolders(QStringList folders)
{
    music_folders_ = folders;
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
                if (!oldTrack->accessed_by_taglib) {
//                    qDebug() << "FILE MODIFY " << path;
                    PTrack track = scanFile(path);
                    it.value()->addFile(track);
                    emit libraryChanged(LibraryEvent(track, MODIFY));
                } else {
                    // TODO: Should update track's mtime here cause  taglib accessedus
                    oldTrack->accessed_by_taglib = false;
                }
            } else {
                qDebug() << "Modified file did not exist";
            }
        } else {
            qDebug() << "Couldnt find the directory where the file was modified";
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
}

void Library::setFoldersInSettings()
{
    QSettings settings;
    settings.setValue("library/folders", music_folders_);
}

#include "library.moc"