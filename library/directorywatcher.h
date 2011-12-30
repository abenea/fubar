#ifndef DIRECTORYWATCHER_H
#define DIRECTORYWATCHER_H

#include <QString>
#include <boost/function.hpp>
#include <boost/bimap.hpp>
#include <map>


struct inotify_event;

enum WatchEvent {DELETE, CREATE};

class DirectoryWatcher
{
public:
	DirectoryWatcher();
	virtual ~DirectoryWatcher();

	typedef boost::function<void (QString, WatchEvent)> WatchCallback;
	void setDirectoryCallback(WatchCallback callback) { directory_callback_ = callback; }
	void setFileCallback(WatchCallback callback) { file_callback_ = callback; }

	void addWatch(QString path);
	void removeWatch(QString path);
	// reads the inotify events; it neva eva ends - nap
	void watch();

private:
	void readEvents();
	void handleEvent(inotify_event *event);
	void dumpWatches();

	int fd_;
	WatchCallback directory_callback_;
	WatchCallback file_callback_;

	typedef boost::bimap<int, QString> WatchMap;
	WatchMap watches_;
};

#endif // DIRECTORYWATCHER_H
