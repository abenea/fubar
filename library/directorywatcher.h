#pragma once

#include <QMutex>
#include <QString>
#include <QWaitCondition>
#include <boost/bimap.hpp>

#include "library/libraryeventtype.h"

struct inotify_event;

class DirectoryWatcher {
public:
    DirectoryWatcher();
    virtual ~DirectoryWatcher();

    typedef std::function<void(QString, LibraryEventType)> WatchCallback;
    void setDirectoryCallback(WatchCallback callback) { directory_callback_ = callback; }
    void setFileCallback(WatchCallback callback) { file_callback_ = callback; }

    void addWatch(QString path);
    void removeWatch(QString path);
    void watch();
    void stop();

private:
    void readEvents();
    void handleEvent(inotify_event *event);
    void dumpWatches();

    int fd_;
    WatchCallback directory_callback_;
    WatchCallback file_callback_;

    typedef boost::bimap<int, QString> WatchMap;
    WatchMap watches_;

    volatile bool stop_;
    QMutex mutex_;
    QWaitCondition stop_condition_;
};
