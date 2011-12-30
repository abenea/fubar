#include "library/library.h"
#include <QString>
#include <cstdio>

int main()
{
    Library library;

    std::vector<QString> folders;
    folders.push_back("/home/bogdan/music");
    library.setMusicFolders(folders);
    library.dumpDatabase();
	library.watch();
    return 0;
}