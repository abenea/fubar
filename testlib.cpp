#include "library/library.h"
#include <QString>
#include <QList>
#include <cstdio>

int main()
{
    Library library;

    QList<QString> folders;
    folders << "/home/bogdan/music_test";
    library.setMusicFolders(folders);
    library.dumpDatabase();
    library.watch();
    return 0;
}