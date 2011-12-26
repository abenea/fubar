#include "directory.h"
#include "track.pb.h"
#include <QFileInfo>
#include <boost/foreach.hpp>

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
    files_.insert(make_pair(QFileInfo(file->location).fileName(), file));
}

void Directory::addSubdirectory(shared_ptr<Directory> directory)
{
    subdirs_.insert(make_pair(QFileInfo(directory->location_).fileName(), directory));
}

void Directory::addFilesToProto(proto::Library& library)
{
    for (FileMap::const_iterator it = files_.begin(); it != files_.end(); ++it) {
        proto::Track* track = library.add_tracks();
        it->second->fillProtoTrack(*track);
    }
}

void Directory::dump()
{
    BOOST_FOREACH (FileMap::value_type &p, files_) {
        printf("%s\n", p.second->location.toStdString().c_str());
    }
    BOOST_FOREACH (SubdirectoryMap::value_type &p, subdirs_) {
        printf("%s\n", p.second->path().toStdString().c_str());
        p.second->dump();
    }
}

void Directory::addFilesFromDirectory(shared_ptr<Directory> directory)
{
	files_ = directory->files_;
}

std::vector<QString> Directory::getSubdirectories()
{
	std:vector<QString> subdirs;
	subdirs.reserve(subdirs_.size());
	for (SubdirectoryMap::const_iterator it = subdirs_.begin(); it != subdirs_.end(); ++it) {
		subdirs.push_back(it->first);
	}
	return subdirs;
}

shared_ptr<Track> Directory::getFile(QString name)
{
	shared_ptr<Track> result;
	FileMap::iterator it = files_.find(name);
	if (it != files_.end())
		result = it->second;
	return result;
}
