#include "ui/playlistmodel.h"
#include "library/track.h"
#include "navidrome/navidrometrack.h"
#include "navidrome/tests/testsupport.h"
#include "player/playlistmimedata.h"
#include "ui/playlistfilter.h"
#include "ui/playlisttab.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <QApplication>
#include <QJsonObject>
#include <QSignalSpy>

using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

constexpr std::string_view kTestString = R"json({
    "album": "Woe",
    "artist": "An Abstract Illusion",
    "track": "The Behemoth That Lies Asleep",
    "duration": 203,
    "webpage_url": "https://music.youtube.com/watch?v=KmErIdMyoYg",
    "playlist_index": 1
})json";

TEST(PlaylistModelTest, YouTubePlaylist) {
    PlaylistModel m(true);
    QSignalSpy spy(&m, &PlaylistModel::youtubeDlDone);
    m.setFakeYoutubeDl(QString(kTestString.data()));
    m.addUrls({QUrl(
        "https://music.youtube.com/playlist?list=OLAK5uy_kmdBpo1XktUr9VkTunIFRQa9hkegRZHIU")});
    spy.wait(1000);
    ASSERT_THAT(m.playlist().tracks, SizeIs(1));
    const Track &track = *m.playlist().tracks[0];
    EXPECT_THAT(track.location, "https://music.youtube.com/watch?v=KmErIdMyoYg");
    EXPECT_THAT(track.audioproperties.length, 203);
    EXPECT_THAT(track.metadata.toStdMap(),
                UnorderedElementsAre(Pair("track", "1"), Pair("artist", "An Abstract Illusion"),
                                     Pair("title", "The Behemoth That Lies Asleep"),
                                     Pair("album", "Woe")));
}

PTrack makeTrack(const QString &location, const QString &path = QString()) {
    auto track = std::make_shared<Track>();
    track->location = location;
    if (!path.isEmpty())
        track->metadata["_path"] = path;
    return track;
}

QStringList locations(PlaylistModel &model) {
    QStringList result;
    for (const PTrack &track : model.playlist().tracks)
        result << track->location;
    return result;
}

QList<int> rows(const QSignalSpy &spy) {
    QList<int> result;
    for (const QList<QVariant> &arguments : spy)
        result << arguments.at(1).toInt() << arguments.at(2).toInt();
    return result;
}

TEST(PlaylistModelTest, UpdateTracksOnEmptyModelAddsAll) {
    PlaylistModel m(false);
    QList<PTrack> tracks{makeTrack("n:1"), makeTrack("n:2")};
    m.updateTracks(tracks);
    EXPECT_EQ(m.playlist().tracks, tracks);
}

TEST(PlaylistModelTest, UpdateTracksDiff) {
    PlaylistModel m(false);
    QList<PTrack> old;
    for (int i = 1; i <= 7; ++i)
        old << makeTrack(QString("n:%1").arg(i));
    m.updateTracks(old);
    QList<QPersistentModelIndex> persistent;
    for (int row = 0; row < 7; ++row)
        persistent << QPersistentModelIndex(m.index(row, 0));

    QSignalSpy removed(&m, &QAbstractItemModel::rowsRemoved);
    QSignalSpy inserted(&m, &QAbstractItemModel::rowsInserted);
    QSignalSpy changed(&m, &QAbstractItemModel::dataChanged);
    QSignalSpy reset(&m, &QAbstractItemModel::modelReset);
    // Removed: n:2 + n:3 (one range), n:5, n:7 (last row). Changed: n:4 (a new object).
    // n:1 and n:6 come as the same objects: they did not change.
    PTrack changed4 = makeTrack("n:4", "changed");
    QList<PTrack> tracks{makeTrack("n:9"), changed4, old[0], old[5], makeTrack("n:8")};
    m.updateTracks(tracks);

    EXPECT_EQ(locations(m), QStringList({"n:1", "n:4", "n:6", "n:9", "n:8"}));
    EXPECT_EQ(m.playlist().tracks[0], old[0]);
    EXPECT_EQ(m.playlist().tracks[1], changed4);
    EXPECT_EQ(m.playlist().tracks[2], old[5]);
    EXPECT_EQ(m.playlist().tracks[3], tracks[0]);
    EXPECT_EQ(rows(removed), QList<int>({6, 6, 4, 4, 1, 2}));
    EXPECT_EQ(rows(inserted), QList<int>({3, 4}));
    ASSERT_EQ(changed.size(), 1);
    EXPECT_EQ(changed[0].at(0).toModelIndex().row(), 1);
    EXPECT_EQ(changed[0].at(1).toModelIndex().row(), 1);
    EXPECT_TRUE(reset.isEmpty());

    EXPECT_EQ(persistent[0].row(), 0);
    EXPECT_EQ(persistent[3].row(), 1);
    EXPECT_EQ(persistent[5].row(), 2);
    for (int i : {1, 2, 4, 6})
        EXPECT_FALSE(persistent[i].isValid()) << i;

    // Nothing to do
    removed.clear();
    inserted.clear();
    changed.clear();
    m.updateTracks(tracks);
    EXPECT_TRUE(removed.isEmpty());
    EXPECT_TRUE(inserted.isEmpty());
    EXPECT_TRUE(changed.isEmpty());

    m.updateTracks({});
    EXPECT_EQ(m.rowCount(QModelIndex()), 0);
    EXPECT_EQ(rows(removed), QList<int>({0, 4}));
}

TEST(PlaylistModelTest, UpdateTracksReplacesRunsOfNewObjects) {
    PlaylistModel m(false);
    QList<PTrack> old;
    for (int i = 1; i <= 5; ++i)
        old << makeTrack(QString("n:%1").arg(i));
    m.updateTracks(old);
    QPersistentModelIndex third(m.index(2, 0));

    QSignalSpy changed(&m, &QAbstractItemModel::dataChanged);
    QList<PTrack> tracks{old[0], makeTrack("n:2"), makeTrack("n:3"), old[3], makeTrack("n:5")};
    m.updateTracks(tracks);
    EXPECT_EQ(m.playlist().tracks, tracks);
    EXPECT_EQ(third.row(), 2);
    ASSERT_EQ(changed.size(), 2);
    EXPECT_EQ(changed[0].at(0).toModelIndex().row(), 1);
    EXPECT_EQ(changed[0].at(1).toModelIndex().row(), 2);
    EXPECT_EQ(changed[1].at(0).toModelIndex().row(), 4);
    EXPECT_EQ(changed[1].at(1).toModelIndex().row(), 4);
}

// Exposes the slot for reloads of the local library
class LibraryViewModel : public PlaylistModel {
public:
    LibraryViewModel() : PlaylistModel(false) {}
    using PlaylistModel::libraryChanged;
};

TEST(PlaylistModelTest, LibraryReloadReplacesTheTracks) {
    LibraryViewModel m;
    QSignalSpy removed(&m, &QAbstractItemModel::rowsRemoved);
    QSignalSpy inserted(&m, &QAbstractItemModel::rowsInserted);
    m.libraryChanged(QList<PTrack>());
    EXPECT_TRUE(removed.isEmpty());
    EXPECT_TRUE(inserted.isEmpty());
    m.libraryChanged(QList<PTrack>{makeTrack("n:1"), makeTrack("n:2")});
    QPersistentModelIndex first(m.index(0, 0));
    m.libraryChanged(QList<PTrack>{makeTrack("n:1")});
    EXPECT_EQ(locations(m), QStringList({"n:1"}));
    EXPECT_FALSE(first.isValid());
}

TEST(PlaylistModelTest, FilterAndSortByPath) {
    PlaylistModel m(false);
    // Sorted by location this would be: local, Beta, Alpha
    m.addTracks({makeTrack("navidrome:aa1", "Beta/Two/01 - y.flac"),
                 makeTrack("navidrome:zz9", "Alpha/One/01 - x.flac"),
                 makeTrack("/music/Gamma/03 - z.flac")});
    PlaylistFilter filter;
    filter.setSourceModel(&m);
    filter.setDynamicSortFilter(true);
    filter.sort(0);
    auto filtered = [&]() {
        QStringList result;
        for (int row = 0; row < filter.rowCount(); ++row)
            result << filter.index(row, 0).data(TrackRole).value<PTrack>()->path();
        return result;
    };
    EXPECT_EQ(filtered(),
              QStringList({"/music/Gamma/03 - z.flac", "Alpha/One/01 - x.flac",
                           "Beta/Two/01 - y.flac"}));
    // Opaque ids and the location prefix do not match
    filter.setFilter("zz9");
    EXPECT_EQ(filtered(), QStringList());
    filter.setFilter("navidrome");
    EXPECT_EQ(filtered(), QStringList());
    filter.setFilter("alpha one");
    EXPECT_EQ(filtered(), QStringList({"Alpha/One/01 - x.flac"}));
    filter.setFilter("gamma");
    EXPECT_EQ(filtered(), QStringList({"/music/Gamma/03 - z.flac"}));
}

TEST(PlaylistModelTest, NavidromeTrackSerializeRoundTrip) {
    PTrack track = navidrome::trackFromSong(QJsonObject{{"id", "song-1"},
                                                        {"title", "Title"},
                                                        {"artist", "Artist"},
                                                        {"album", "Album"},
                                                        {"track", 3},
                                                        {"suffix", "flac"},
                                                        {"duration", 241},
                                                        {"path", "Artist/Album/03 - Title.flac"}});
    ASSERT_TRUE(track);
    PlaylistModel m(true);
    m.addTracks({track});
    QByteArray bytes;
    m.playlist().serialize(bytes);
    PlaylistModel restored(true);
    restored.deserialize(bytes);
    ASSERT_EQ(restored.rowCount(QModelIndex()), 1);
    const Track &copy = *restored.playlist().tracks[0];
    EXPECT_EQ(copy.location, "navidrome:song-1");
    EXPECT_EQ(copy.path(), "Artist/Album/03 - Title.flac");
    EXPECT_EQ(copy.metadata, track->metadata);
    EXPECT_EQ(copy.audioproperties.length, 241);
}

TEST(PlaylistModelTest, DropOnReadOnlyTabIsIgnored) {
    PlaylistMimeData data({makeTrack("/music/a.flac")});
    auto readOnly = std::make_shared<PlaylistModel>(false);
    PlaylistTab readOnlyTab(readOnly);
    ASSERT_FALSE(readOnlyTab.isEditable());
    readOnlyTab.dropMimeData(&data);
    EXPECT_EQ(readOnly->rowCount(QModelIndex()), 0);

    auto editable = std::make_shared<PlaylistModel>(true);
    PlaylistTab editableTab(editable);
    editableTab.dropMimeData(&data);
    EXPECT_EQ(editable->rowCount(QModelIndex()), 1);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    QApplication app(argc, argv);
    return RUN_ALL_TESTS();
}
