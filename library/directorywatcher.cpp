#include "directorywatcher.h"
#include <sys/inotify.h>
#include <sys/select.h>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <errno.h>
#include <string.h>

void myperror(std::string message)
{
    qDebug() << message.c_str() << ": " << strerror(errno);
}

DirectoryWatcher::DirectoryWatcher()
: stop_(false)
{
    fd_ = inotify_init();
    if (fd_ < 0)
        myperror("inotify_init()");
}

DirectoryWatcher::~DirectoryWatcher()
{
    if (fd_ >= 0) {
        if (close(fd_))
            myperror("inotify close");
    }
}

void DirectoryWatcher::addWatch(QString path)
{
    int wd = inotify_add_watch(fd_, path.toUtf8().constData(), IN_CREATE | IN_DELETE | IN_DELETE_SELF
        | IN_MOVE /*| IN_IGNORED */| IN_MOVE_SELF | IN_CLOSE_WRITE);
    if (wd < 0) {
        myperror("inotify_add_watch");
        qDebug() << "Cannot monitor " << path;
    } else {
        watches_.left.insert(std::make_pair(wd, path));
    }
}

void DirectoryWatcher::removeWatch(QString path)
{
    WatchMap::right_iterator it = watches_.right.find(path);
    if (it == watches_.right.end()) {
        qDebug() << "Wanted to remove watch " << path << ". Cannot find it's wd";
        return;
    }
    if (inotify_rm_watch(fd_, it->second))
        myperror("inotify_rm_watch");
    watches_.right.erase(it);
}

void DirectoryWatcher::stop()
{
    stop_ = true;
    mutex_.lock();
    stop_condition_.wait(&mutex_);
    mutex_.unlock();
}

void DirectoryWatcher::watch()
{
    while (!stop_) {
        struct timeval time;
        fd_set rfds;
        int ret;
        FD_ZERO(&rfds);
        FD_SET(fd_, &rfds);

        time.tv_sec = 1;
        time.tv_usec = 0;
        ret = select(fd_ + 1, &rfds, NULL, NULL, &time);
        if (stop_) {
            stop_condition_.wakeAll();
            return;
        }
        if (ret < 0)
            myperror("select");
        else if (FD_ISSET(fd_, &rfds))
            /* inotify events are available! */
            readEvents();
    }
}

void DirectoryWatcher::readEvents()
{
    /* size of the event structure, not counting name */
    const int EVENT_SIZE = sizeof(struct inotify_event);
    /* reasonable guess as to size of 1024 events */
    const int BUF_LEN = 1024 * (EVENT_SIZE + 16);
    char buf[BUF_LEN];

    int len, i = 0;

    len = read (fd_, buf, BUF_LEN);
    if (len < 0) {
        if (errno == EINTR)
            // need to reissue system call
            return;
        else
            myperror("read");
    } else if (!len)
        // BUF_LEN too small?
        qDebug() << "the buffer for reading inotify too small?";
    if (len <= 0)
        return;

    while (i < len) {
        struct inotify_event *event = reinterpret_cast<inotify_event*>(&buf[i]);
        handleEvent(event);
        i += EVENT_SIZE + event->len;
    }
}

std::string mask2str(int m)
{
    std::string r;
    if (m & IN_Q_OVERFLOW)
        r += "IN_Q_OVERFLOW ";
    if (m & IN_CREATE)
        r += "IN_CREATE ";
    if (m & IN_DELETE)
        r += "IN_DELETE ";
    if (m & IN_DELETE_SELF)
        r += "IN_DELETE_SELF ";
    if (m & IN_MOVED_TO)
        r += "IN_MOVED_TO ";
    if (m & IN_MOVED_FROM)
        r += "IN_MOVED_FROM ";
    if (m & IN_MOVE_SELF)
        r += "IN_MOVE_SELF ";
    if (m & IN_IGNORED)
        r += "IN_IGNORED ";
    if (m & IN_CLOSE_WRITE)
        r += "IN_CLOSE_WRITE ";
    return r;
}

void DirectoryWatcher::handleEvent(inotify_event* event)
{
    if (event->mask & IN_Q_OVERFLOW) {
        qDebug() << "inotify event queue overflow!!1";
        return;
    }
    qDebug() << "wd=" << event->wd << "mask=" << event->mask << "cookie="
            << event->cookie << "len=" << event->len
            << (event->len ? event->name : "")
            << mask2str(event->mask).c_str();

    WatchMap::left_iterator it = watches_.left.find(event->wd);
    if (it == watches_.left.end()) {
        qDebug() << "ffs can't map the inotify wd to path";
        return;
    }
    LibraryEventType eventType = UNKNOWN;
    if (event->mask & IN_CLOSE_WRITE) {
        eventType = MODIFY;
    } else if (event->mask & IN_CREATE || event->mask & IN_MOVED_TO) {
        eventType = CREATE;
    } else if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM) {
        eventType = DELETE;
    }
    if (eventType != UNKNOWN) {
        if (event->len == 0)
            qDebug() << "wtf no event->name";
        QString path = QFileInfo(QDir(it->second), QString(event->name)).absoluteFilePath();
        if (event->mask & IN_ISDIR) {
            if (eventType != MODIFY)
                directory_callback_(path, eventType);
        } else {
            file_callback_(path, eventType);
        }
    }
    if (event->mask & IN_MOVE_SELF || event->mask & IN_DELETE_SELF) {
        if (it != watches_.left.end()) {
            directory_callback_(it->second, DELETE);
        }
    }
    //dumpWatches();
}

void DirectoryWatcher::dumpWatches()
{
    for (WatchMap::right_const_iterator it = watches_.right.begin(); it != watches_.right.end(); ++it)
        qDebug() << it->first << " --> " << it->second;
}
