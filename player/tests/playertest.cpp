#include <QApplication>
#include <QDebug>

#include "library/track.h"
#include "player/tests/mockaudiooutput.h"
#include "ui/audioplayer.h"
#include "ui/mainwindow.h"
#include "ui/playlistmodel.h"
#include "gtest/gtest.h"

QList<PTrack> generateTracks(int no, int start = 0) {
    QList<PTrack> result;
    for (int i = start; i < start + no; ++i) {
        auto track = new Track();
        track->location = QString("%1").arg(i);
        result.append(PTrack(track));
    }
    return result;
}

class PlayerTest : public testing::Test {
protected:
    PlayerTest() : player_(nullptr, &audioOutput_, /*testing=*/true), mw_(player_) {
        player_.setMainWindow(&mw_);
    }

    void autoEnqueueNextDeletedHelper() {
        auto p = newPlaylist(generateTracks(3));
        player_.play();
        audioOutput_.triggerAboutToFinish();
        removeAt(p, 1);
        audioOutput_.triggerCurrentSourceChanged();
        EXPECT_EQ(audioOutput_.currentSource(), "1");
    }

    PModel newPlaylist() {
        auto p = player_.createPlaylist(false);
        mw_.addPlaylist(p, "", false);
        return p;
    }

    PModel newPlaylist(QList<PTrack> tracks) {
        auto p = newPlaylist();
        p->addTracks(tracks);
        return p;
    }

    void deletePlaylist(int pos) { mw_.removePlaylistTab(pos); }

    void removeAt(PModel p, int pos) { p->removeIndexes({p->index(pos, 0)}); }

    void enqueue(PModel p, int pos) { player_.enqueueTracks(p, {QModelIndex(p->index(pos, 0))}); }

    MockAudioOutput audioOutput_;
    AudioPlayer player_;
    MainWindow mw_;
};

TEST_F(PlayerTest, noPlaylists) { player_.play(); }

TEST_F(PlayerTest, oneEmptyPlaylist) {
    newPlaylist();
    player_.play();
}

TEST_F(PlayerTest, oneTrack) {
    newPlaylist(generateTracks(1));
    player_.play();
    EXPECT_EQ(audioOutput_.currentSource(), "0");
    player_.next();
    player_.prev();
}

TEST_F(PlayerTest, prevNext) {
    newPlaylist(generateTracks(2));
    player_.play();
    EXPECT_EQ(audioOutput_.currentSource(), "0");
    player_.next();
    EXPECT_EQ(audioOutput_.currentSource(), "1");
    player_.prev();
    EXPECT_EQ(audioOutput_.currentSource(), "0");
}

TEST_F(PlayerTest, autoEnqueueNext) {
    newPlaylist(generateTracks(2));
    player_.play();
    audioOutput_.triggerAboutToFinish();
    audioOutput_.triggerCurrentSourceChanged();
    EXPECT_EQ(audioOutput_.currentSource(), "1");
    audioOutput_.triggerAboutToFinish();
    audioOutput_.triggerCurrentSourceChanged();
    EXPECT_EQ(audioOutput_.currentSource(), "");
    player_.play();
    EXPECT_EQ(audioOutput_.currentSource(), "1");
}

TEST_F(PlayerTest, autoEnqueueNextDeletedNext) {
    autoEnqueueNextDeletedHelper();
    player_.next();
    EXPECT_EQ(audioOutput_.currentSource(), "2");
}

TEST_F(PlayerTest, autoEnqueueNextDeletedPrev) {
    autoEnqueueNextDeletedHelper();
    player_.prev();
    EXPECT_EQ(audioOutput_.currentSource(), "0");
}

TEST_F(PlayerTest, autoEnqueueNextDeletedThenAutoEnqueue) {
    auto p = newPlaylist(generateTracks(3));
    player_.play();
    audioOutput_.triggerAboutToFinish();
    removeAt(p, 1);
    audioOutput_.triggerCurrentSourceChanged();
    EXPECT_EQ(audioOutput_.currentSource(), "1");
    audioOutput_.triggerAboutToFinish();
    audioOutput_.triggerCurrentSourceChanged();
    EXPECT_EQ(audioOutput_.currentSource(), "2");
}

TEST_F(PlayerTest, enqueue) {
    auto p1 = newPlaylist(generateTracks(3));
    auto p2 = newPlaylist(generateTracks(3, 3));
    player_.play();
    EXPECT_EQ(audioOutput_.currentSource(), "0");
    enqueue(p1, 2);
    enqueue(p2, 1);
    player_.next();
    EXPECT_EQ(audioOutput_.currentSource(), "2");
    audioOutput_.triggerAboutToFinish();
    audioOutput_.triggerCurrentSourceChanged();
    EXPECT_EQ(audioOutput_.currentSource(), "4");
    player_.next();
    EXPECT_EQ(audioOutput_.currentSource(), "5");

    enqueue(p1, 0);
    enqueue(p1, 0);
    enqueue(p1, 0);
    player_.next();
    EXPECT_EQ(audioOutput_.currentSource(), "0");
    player_.next();
    EXPECT_EQ(audioOutput_.currentSource(), "0");
    player_.clearQueue();
    player_.next();
    EXPECT_EQ(audioOutput_.currentSource(), "1");
}

TEST_F(PlayerTest, enqueueDeleted) {
    auto p1 = newPlaylist(generateTracks(3));
    player_.play();
    EXPECT_EQ(audioOutput_.currentSource(), "0");
    enqueue(p1, 1);
    removeAt(p1, 1);
    player_.next();
    EXPECT_EQ(audioOutput_.currentSource(), "2");
    enqueue(p1, 1);
    audioOutput_.triggerAboutToFinish();
    removeAt(p1, 1);
    audioOutput_.triggerCurrentSourceChanged();
    EXPECT_EQ(audioOutput_.currentSource(), "2");
    player_.play();
    EXPECT_EQ(audioOutput_.currentSource(), "0");
}

TEST_F(PlayerTest, enqueueDeletedPlaylist) {
    auto p1 = newPlaylist(generateTracks(3));
    auto p2 = newPlaylist(generateTracks(3, 3));
    player_.play();
    EXPECT_EQ(audioOutput_.currentSource(), "0");
    enqueue(p2, 1);
    deletePlaylist(1);
    player_.next();
    EXPECT_EQ(audioOutput_.currentSource(), "1");

    // Delete playlist after buffering
    p2 = newPlaylist(generateTracks(3, 3));
    enqueue(p2, 1);
    audioOutput_.triggerAboutToFinish();
    deletePlaylist(1);
    audioOutput_.triggerCurrentSourceChanged();
    EXPECT_EQ(audioOutput_.currentSource(), "4");
    player_.next();
    EXPECT_EQ(audioOutput_.currentSource(), "2");

    // Delete playlist before buffering
    player_.prev();
    EXPECT_EQ(audioOutput_.currentSource(), "1");
    p2 = newPlaylist(generateTracks(3, 3));
    enqueue(p2, 1);
    deletePlaylist(1);
    audioOutput_.triggerAboutToFinish();
    audioOutput_.triggerCurrentSourceChanged();
    EXPECT_EQ(audioOutput_.currentSource(), "2");

    // Delete same playlist before buffering
    player_.prev();
    EXPECT_EQ(audioOutput_.currentSource(), "1");
    enqueue(p1, 0);
    deletePlaylist(0);
    audioOutput_.triggerAboutToFinish();
    audioOutput_.triggerCurrentSourceChanged();
    EXPECT_EQ(audioOutput_.currentSource(), "");
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    QApplication app(argc, argv);
    return RUN_ALL_TESTS();
}
