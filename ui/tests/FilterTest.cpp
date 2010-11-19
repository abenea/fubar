#include "FilterTest.h"

#include <library/playlist.h>
#include <ui/playlistmodel.h>
#include <ui/playlistfilter.h>

void FilterTest::filterPlaylist()
{
    Playlist playlist;
    playlist.load("../../test.pb");

    PlaylistModel model(playlist);
    PlaylistFilter filter;
    filter.setSourceModel(&model);
    QBENCHMARK {
        filter.setFilter("anAt");
        QModelIndex fake;
        for (int row = 0; row < model.rowCount(fake); ++row) {
            filter.filterAcceptsRow(row, fake);
        }
    }
}

QTEST_MAIN(FilterTest)
#include "FilterTest.moc"