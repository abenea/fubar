#include "ui/playlistmodel.h"
#include "library/track.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <QApplication>
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

QT_BEGIN_NAMESPACE
inline void PrintTo(const QString &qString, ::std::ostream *os) { *os << qUtf8Printable(qString); }
QT_END_NAMESPACE

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

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    QApplication app(argc, argv);
    return RUN_ALL_TESTS();
}
