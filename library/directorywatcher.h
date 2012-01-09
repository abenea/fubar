#ifndef DIRECTORYWATCHER_H
#define DIRECTORYWATCHER_H

#include "library/libraryeventtype.h"
#include <QString>
#include <boost/function.hpp>
#include <boost/bimap.hpp>
#include <map>


struct inotify_event;

class DirectoryWatcher
{
public:
    DirectoryWatcher();
    virtual ~DirectoryWatcher();

    typedef boost::function<void (QString, LibraryEventType)> WatchCallback;
    void setDirectoryCallback(WatchCallback callback) { directory_callback_ = callback; }
    void setFileCallback(WatchCallback callback) { file_callback_ = callback; }

    void addWatch(QString path);
    void removeWatch(QString path);
    // reads the inotify events; it neva eva ends - nap
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
};

#endif // DIRECTORYWATCHER_H
