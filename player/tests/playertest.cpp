#include <QApplication>
#include <QStringList>

#include "library/library.h"
#include "library/track.h"
#include "navidrome/tests/testsupport.h"
#include "player/tests/mockaudiooutput.h"
#include "ui/audioplayer.h"
#include "ui/mainwindow.h"
#include "ui/mprisplayer.h"
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

PTrack remoteTrack(const QString &id, const QString &path) {
    auto track = std::make_shared<Track>();
    track->location = "navidrome:" + id;
    track->metadata["_path"] = path;
    track->metadata["title"] = id;
    return track;
}

// Track `number` of a cue sheet for one file
PTrack cueTrack(int number, int offsetMs, int lengthS) {
    auto track = std::make_shared<Track>();
    track->location = "/music/album.flac";
    track->metadata["track"] = QString::number(number);
    track->metadata["title"] = QString("Part %1").arg(number);
    track->metadata["_cue_offset"] = QString::number(offsetMs);
    track->audioproperties.length = lengthS;
    return track;
}

class PlayerTest : public testing::Test {
protected:
    PlayerTest() : player_(nullptr, &audioOutput_, /*testing=*/true), mw_(player_) {
        player_.setMainWindow(&mw_);
        QObject::connect(
            &player_, &AudioPlayer::playbackError, [this](PTrack track, QString reason) {
                errors_.append({track, reason});
            });
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

    static int rowOf(PModel p, const QString &location) {
        for (int row = 0; row < p->rowCount(QModelIndex()); ++row) {
            if (p->playlist().tracks[row]->location == location)
                return row;
        }
        return -1;
    }

    MockAudioOutput audioOutput_;
    AudioPlayer player_;
    MainWindow mw_;
    QList<std::pair<PTrack, QString>> errors_;
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

// The same location queued from two playlists
TEST_F(PlayerTest, enqueuedInAnotherPlaylistSurvivesClosingOne) {
    auto p1 = newPlaylist(generateTracks(2));
    auto p2 = newPlaylist(generateTracks(2, 1));
    player_.play();
    EXPECT_EQ(audioOutput_.currentSource(), "0");
    enqueue(p1, 1);
    enqueue(p2, 0);
    deletePlaylist(0);
    EXPECT_TRUE(player_.isEnqueued(p2, p2->index(0, 0)));
    player_.next();
    EXPECT_EQ(audioOutput_.currentSource(), "1");
    EXPECT_EQ(player_.getPlayingPlaylistModel(), p2);
    EXPECT_EQ(player_.getPlayingIndex().row(), 0);
    EXPECT_FALSE(player_.isEnqueued(p2, p2->index(0, 0)));
    player_.next();
    EXPECT_EQ(audioOutput_.currentSource(), "2");
}

TEST_F(PlayerTest, sourceResolver) {
    auto tracks = generateTracks(4);
    tracks[0]->location = "test:a";
    tracks[1]->location = "test:b";
    tracks[2]->location = "test:empty";
    newPlaylist(tracks);
    player_.registerSourceResolver("test:", [](const QString &location, QString *) {
        return location == "test:empty" ? QString() : "resolved/" + location;
    });
    player_.play();
    EXPECT_EQ(audioOutput_.currentSource(), "resolved/test:a");
    // Buffered (gapless) tracks are resolved too
    audioOutput_.triggerAboutToFinish();
    audioOutput_.triggerCurrentSourceChanged();
    EXPECT_EQ(audioOutput_.currentSource(), "resolved/test:b");
    // An unplayable next track is not buffered
    EXPECT_EQ(audioOutput_.nextSource(), "");
    // An empty resolution makes the track unplayable: playback stops, nothing reaches the output
    int received = audioOutput_.received().size();
    player_.next();
    EXPECT_EQ(audioOutput_.received().size(), received);
    EXPECT_EQ(audioOutput_.stopCount(), 1);
    EXPECT_EQ(audioOutput_.currentSource(), "");
    EXPECT_EQ(audioOutput_.nextSource(), "");
    ASSERT_EQ(errors_.size(), 1);
    EXPECT_EQ(errors_[0].first, tracks[2]);
    EXPECT_EQ(errors_[0].second, "no source");
    // Locations without a registered prefix are passed through
    player_.next();
    EXPECT_EQ(audioOutput_.currentSource(), "3");
    EXPECT_EQ(errors_.size(), 1);
    EXPECT_FALSE(audioOutput_.received().contains(""));
    EXPECT_FALSE(audioOutput_.received().contains("test:empty"));
    player_.unregisterSourceResolver("test:");
    player_.prev();
    player_.prev();
    EXPECT_EQ(audioOutput_.currentSource(), "test:b");
}

TEST_F(PlayerTest, unplayableNextTrackStopsWhenItsTurnComes) {
    auto tracks = generateTracks(3);
    tracks[1]->location = "test:empty";
    newPlaylist(tracks);
    player_.registerSourceResolver("test:", [](const QString &, QString *) { return QString(); });
    player_.play();
    EXPECT_EQ(audioOutput_.currentSource(), "0");
    EXPECT_EQ(audioOutput_.nextSource(), "");
    EXPECT_TRUE(errors_.isEmpty());
    // The output runs out of sources
    int received = audioOutput_.received().size();
    audioOutput_.triggerFinished();
    EXPECT_EQ(audioOutput_.received().size(), received);
    EXPECT_FALSE(audioOutput_.received().contains(""));
    ASSERT_EQ(errors_.size(), 1);
    EXPECT_EQ(errors_[0].first, tracks[1]);
    EXPECT_EQ(errors_[0].second, "no source");
    // No auto-advance past the unplayable track
    EXPECT_EQ(audioOutput_.currentSource(), "");
    EXPECT_EQ(audioOutput_.nextSource(), "");
    EXPECT_EQ(player_.getCurrentTrack(), tracks[1]);
}

TEST_F(PlayerTest, sourceResolverReasonIsReported) {
    auto tracks = generateTracks(2);
    tracks[0]->location = "test:a";
    newPlaylist(tracks);
    player_.registerSourceResolver("test:", [](const QString &, QString *error) {
        *error = "Not configured";
        return QString();
    });
    player_.play();
    EXPECT_EQ(audioOutput_.currentSource(), "");
    ASSERT_EQ(errors_.size(), 1);
    EXPECT_EQ(errors_[0].first, tracks[0]);
    EXPECT_EQ(errors_[0].second, "Not configured");
}

TEST_F(PlayerTest, lengthHackNeedsALibrary) {
    mw_.getConfig().set("library.length_hack", QVariant(true));
    auto tracks = generateTracks(1);
    tracks[0]->location = "/music/0.flac";
    tracks[0]->audioproperties.length = 10;
    newPlaylist(tracks);
    player_.play();
    audioOutput_.triggerTick(60000);
    EXPECT_EQ(tracks[0]->audioproperties.length, 10);
}

TEST_F(PlayerTest, lengthHackSkipsNavidromeTracks) {
    Library library;
    MockAudioOutput output;
    AudioPlayer player(&library, &output, /*testing=*/true);
    // The constructor took over the global instance the fixture's MainWindow relies on
    AudioPlayer::instance = &player_;
    player.setMainWindow(&mw_);
    mw_.getConfig().set("library.length_hack", QVariant(true));
    int changes = 0;
    QObject::connect(&library, QOverload<LibraryEvent>::of(&Library::libraryChanged), [&]() {
        ++changes;
    });
    auto remote = remoteTrack("r", "Artist/Album/r.flac");
    auto local = generateTracks(1)[0];
    local->location = "/music/local.flac";
    remote->audioproperties.length = local->audioproperties.length = 10;
    auto p = player.createReadOnlyPlaylist();
    p->addTracks({remote, local});
    // Not synced with the local library
    emit library.libraryChanged(QList<PTrack>());
    EXPECT_EQ(p->rowCount(QModelIndex()), 2);

    player.play(p, p->index(0, 0));
    output.triggerTick(60000);
    EXPECT_EQ(remote->audioproperties.length, 10);
    EXPECT_EQ(changes, 0);

    player.play(p, p->index(1, 0));
    output.triggerTick(60000);
    EXPECT_EQ(local->audioproperties.length, 60);
    EXPECT_EQ(changes, 1);
}

TEST_F(PlayerTest, loadFailedWhileLoadingThePlayingTrack) {
    auto tracks = generateTracks(3);
    newPlaylist(tracks);
    audioOutput_.setLoadOnPlay(false);
    player_.play();
    EXPECT_EQ(audioOutput_.currentSource(), "0");
    audioOutput_.triggerLoadFailed("loading failed");
    EXPECT_EQ(audioOutput_.stopCount(), 1);
    ASSERT_EQ(errors_.size(), 1);
    EXPECT_EQ(errors_[0].first, tracks[0]);
    EXPECT_EQ(errors_[0].second, "loading failed");
    // The output goes idle afterwards; nothing else gets played
    int received = audioOutput_.received().size();
    audioOutput_.triggerFinished();
    EXPECT_EQ(audioOutput_.received().size(), received);
    EXPECT_EQ(audioOutput_.currentSource(), "");
    EXPECT_EQ(audioOutput_.nextSource(), "");
    EXPECT_EQ(player_.getCurrentTrack(), tracks[0]);
    // The user can go on
    audioOutput_.setLoadOnPlay(true);
    player_.next();
    EXPECT_EQ(audioOutput_.currentSource(), "1");
    EXPECT_EQ(errors_.size(), 1);
}

TEST_F(PlayerTest, loadFailedWhileLoadingThePlayingTrackWithTheNextOneBuffered) {
    auto tracks = generateTracks(3);
    newPlaylist(tracks);
    audioOutput_.setLoadOnPlay(false);
    player_.play();
    // Clearing the queue buffers the next track while the playing one is still loading
    player_.clearQueue();
    EXPECT_EQ(audioOutput_.nextSource(), "1");
    audioOutput_.triggerLoadFailed("loading failed");
    ASSERT_EQ(errors_.size(), 1);
    EXPECT_EQ(errors_[0].first, tracks[0]);
}

TEST_F(PlayerTest, loadFailedForTheBufferedTrack) {
    auto tracks = generateTracks(3);
    newPlaylist(tracks);
    audioOutput_.setLoadOnPlay(false);
    player_.play();
    audioOutput_.triggerCurrentSourceChanged();
    EXPECT_EQ(audioOutput_.currentSource(), "0");
    EXPECT_EQ(audioOutput_.nextSource(), "1");
    // The playing track ends and the output fails to load the buffered one
    audioOutput_.triggerLoadFailed("loading failed");
    EXPECT_EQ(audioOutput_.stopCount(), 1);
    ASSERT_EQ(errors_.size(), 1);
    EXPECT_EQ(errors_[0].first, tracks[1]);
    EXPECT_EQ(errors_[0].second, "loading failed");
    int received = audioOutput_.received().size();
    audioOutput_.triggerFinished();
    EXPECT_EQ(audioOutput_.received().size(), received);
    EXPECT_EQ(audioOutput_.currentSource(), "");
    EXPECT_EQ(audioOutput_.nextSource(), "");
    EXPECT_EQ(errors_.size(), 1);
}

TEST_F(PlayerTest, loadFailedAfterAGaplessTransition) {
    auto tracks = generateTracks(3);
    newPlaylist(tracks);
    player_.play();
    audioOutput_.triggerCurrentSourceChanged();
    EXPECT_EQ(audioOutput_.currentSource(), "1");
    EXPECT_EQ(audioOutput_.nextSource(), "2");
    audioOutput_.triggerLoadFailed("loading failed");
    ASSERT_EQ(errors_.size(), 1);
    EXPECT_EQ(errors_[0].first, tracks[2]);
}

// The output reports finished() whenever it goes idle (at startup, after stop()). Such a report
// can be delivered after a later play().
TEST_F(PlayerTest, finishedWhileTheRequestedTrackLoadsIsStale) {
    auto tracks = generateTracks(3);
    auto model = newPlaylist(tracks);
    audioOutput_.setLoadOnPlay(false);
    player_.play();
    audioOutput_.triggerFinished();
    EXPECT_EQ(audioOutput_.currentSource(), "0");
    audioOutput_.triggerCurrentSourceChanged();

    // Another track while the next one is buffered
    player_.play(model, model->index(1, 0));
    int received = audioOutput_.received().size();
    audioOutput_.triggerFinished();
    EXPECT_EQ(audioOutput_.currentSource(), "1");
    EXPECT_EQ(audioOutput_.received().size(), received);
    EXPECT_EQ(player_.getCurrentTrack(), tracks[1]);

    // Stop, then play again
    audioOutput_.triggerCurrentSourceChanged();
    player_.stop();
    player_.play();
    audioOutput_.triggerFinished();
    EXPECT_EQ(audioOutput_.currentSource(), "1");
    EXPECT_EQ(player_.getCurrentTrack(), tracks[1]);

    // Once loaded, the end of the track advances
    audioOutput_.triggerCurrentSourceChanged();
    audioOutput_.triggerFinished();
    EXPECT_EQ(audioOutput_.currentSource(), "2");
    EXPECT_TRUE(errors_.isEmpty());
}

// stop() is handled before the queued currentSourceChanged() of a gapless transition.
TEST_F(PlayerTest, stopAtAGaplessTransitionStaysStopped) {
    auto tracks = generateTracks(3);
    newPlaylist(tracks);
    player_.play();
    EXPECT_EQ(audioOutput_.nextSource(), "1");
    int playing = 0;
    QObject context;
    QObject::connect(&player_, &AudioPlayer::trackPlaying, &context, [&]() { ++playing; });
    player_.stop();
    int received = audioOutput_.received().size();
    audioOutput_.triggerCurrentSourceChanged();
    audioOutput_.triggerFinished();
    EXPECT_EQ(playing, 0);
    EXPECT_EQ(audioOutput_.received().size(), received);
    EXPECT_EQ(audioOutput_.currentSource(), "");
    EXPECT_EQ(audioOutput_.nextSource(), "");
    EXPECT_EQ(player_.getCurrentTrack(), tracks[0]);
    EXPECT_TRUE(errors_.isEmpty());
}

// The playing cue track was the last row when a track was appended
TEST_F(PlayerTest, cueTrackEndPlaysATrackAppendedMeanwhile) {
    auto p = newPlaylist({cueTrack(2, 100000, 100), cueTrack(3, 200000, 100)});
    player_.play();
    audioOutput_.triggerCurrentSourceChanged();
    // Past track 2: on to track 3 of the same file
    audioOutput_.triggerTick(100000 + 101000);
    ASSERT_EQ(player_.getCurrentTrack()->metadata.value("track"), "3");
    auto appended = generateTracks(1)[0];
    p->addTracks({appended});
    audioOutput_.triggerTick(200000 + 101000);
    EXPECT_EQ(player_.getCurrentTrack(), appended);
    EXPECT_EQ(audioOutput_.currentSource(), "0");
    EXPECT_EQ(audioOutput_.stopCount(), 0);
}

TEST_F(PlayerTest, cueTrackEndStopsWithoutANextTrack) {
    QList<PTrack> tracks{cueTrack(2, 100000, 100), cueTrack(3, 200000, 100)};
    newPlaylist(tracks);
    player_.play();
    audioOutput_.triggerCurrentSourceChanged();
    audioOutput_.triggerTick(100000 + 101000);
    int received = audioOutput_.received().size();
    audioOutput_.triggerTick(200000 + 101000);
    EXPECT_EQ(audioOutput_.stopCount(), 1);
    EXPECT_EQ(audioOutput_.received().size(), received);
    EXPECT_EQ(audioOutput_.currentSource(), "");
    EXPECT_EQ(player_.getCurrentTrack(), tracks[1]);
}

TEST_F(PlayerTest, resolvedSourcesAreNeverLogged) {
    MessageCapture capture;
    auto tracks = generateTracks(5);
    for (int i = 0; i < 4; ++i)
        tracks[i]->location = QString("test:%1").arg(i);
    tracks[3]->location = "test:empty";
    auto p = newPlaylist(tracks);
    player_.registerSourceResolver("test:", [](const QString &location, QString *) {
        if (location == "test:empty")
            return QString();
        return "http://x/rest/stream?u=a&t=SECRET&s=b&id=" + location.mid(5);
    });
    const QString source = "http://x/rest/stream?u=a&t=SECRET&s=b&id=%1";
    player_.play();
    EXPECT_EQ(audioOutput_.currentSource(), source.arg(0));
    EXPECT_EQ(audioOutput_.nextSource(), source.arg(1));
    enqueue(p, 2);
    player_.next();
    EXPECT_EQ(audioOutput_.currentSource(), source.arg(2));
    player_.next();
    EXPECT_EQ(errors_.size(), 1);
    player_.play(p, p->index(1, 0));
    EXPECT_EQ(audioOutput_.nextSource(), source.arg(2));
    audioOutput_.triggerLoadFailed("loading failed");
    EXPECT_EQ(errors_.size(), 2);
    const QStringList messages = capture.messages();
    EXPECT_FALSE(messages.isEmpty());
    for (const QString &message : messages)
        EXPECT_FALSE(message.contains("SECRET")) << qPrintable(message.left(40));
}

TEST_F(PlayerTest, updateTracksWhilePlayingFromReadOnlyModel) {
    // Source order differs from the tab's order, which is by path()
    QList<PTrack> tracks{remoteTrack("c", "a/3"),
                         remoteTrack("a", "a/5"),
                         remoteTrack("e", "a/1"),
                         remoteTrack("d", "a/2"),
                         remoteTrack("b", "a/4")};
    PModel model = player_.createReadOnlyPlaylist();
    mw_.addPlaylist(model, "Navidrome", false);
    model->updateTracks(tracks);
    player_.registerSourceResolver("navidrome:", [](const QString &location, QString *) {
        return "stream/" + location.mid(10);
    });
    player_.play(model, model->index(rowOf(model, "navidrome:e"), 0));
    EXPECT_EQ(audioOutput_.currentSource(), "stream/e");
    EXPECT_EQ(audioOutput_.nextSource(), "stream/d");
    // The queue takes over: "b" gets buffered, "a" waits
    player_.enqueueTracks(model,
                          {model->index(rowOf(model, "navidrome:b"), 0),
                           model->index(rowOf(model, "navidrome:a"), 0)});
    EXPECT_EQ(audioOutput_.nextSource(), "stream/b");

    // "c" (the first row) disappears, the buffered and the queued tracks change, "f" is new
    PTrack newB = remoteTrack("b", "a/4 renamed");
    PTrack newA = remoteTrack("a", "a/0");
    model->updateTracks({newA, tracks[2], tracks[3], newB, remoteTrack("f", "a/6")});
    ASSERT_EQ(model->rowCount(QModelIndex()), 5);
    EXPECT_EQ(rowOf(model, "navidrome:c"), -1);
    EXPECT_EQ(player_.getPlayingIndex().row(), rowOf(model, "navidrome:e"));
    EXPECT_TRUE(player_.isEnqueued(model, model->index(rowOf(model, "navidrome:a"), 0)));

    audioOutput_.triggerCurrentSourceChanged();
    EXPECT_EQ(audioOutput_.currentSource(), "stream/b");
    EXPECT_EQ(player_.getCurrentTrack(), newB);
    EXPECT_EQ(player_.getPlayingIndex().row(), rowOf(model, "navidrome:b"));
    // The queued entry survived the update and is buffered next
    EXPECT_TRUE(player_.isEnqueued(model, model->index(rowOf(model, "navidrome:a"), 0)));
    EXPECT_EQ(audioOutput_.nextSource(), "stream/a");

    audioOutput_.triggerCurrentSourceChanged();
    EXPECT_EQ(player_.getCurrentTrack(), newA);
    EXPECT_FALSE(player_.isEnqueued(model, model->index(rowOf(model, "navidrome:a"), 0)));
    // Back to the tab's order: "a" is first now, "e" follows
    EXPECT_EQ(audioOutput_.nextSource(), "stream/e");
    EXPECT_TRUE(errors_.isEmpty());
}

TEST_F(PlayerTest, mprisUrlOnlyForLocalFiles) {
    auto *adaptor = mw_.findChild<MPRISPlayerAdaptor *>();
    ASSERT_NE(adaptor, nullptr);
    auto tracks = generateTracks(2);
    tracks[0]->location = "/music/Artist/01 - local.flac";
    tracks[0]->metadata["title"] = "local";
    tracks[1] = remoteTrack("abc", "Artist/Album/01 - remote.flac");
    newPlaylist(tracks);
    player_.registerSourceResolver("navidrome:", [](const QString &, QString *) {
        return QString("http://x/rest/stream?u=a&t=SECRET&s=b&id=abc");
    });
    player_.play();
    QVariantMap metadata = adaptor->Metadata();
    EXPECT_EQ(metadata.value("xesam:title").toString(), "local");
    EXPECT_EQ(metadata.value("xesam:url").toString(),
              QUrl::fromLocalFile("/music/Artist/01 - local.flac").toString());
    player_.next();
    metadata = adaptor->Metadata();
    EXPECT_EQ(metadata.value("xesam:title").toString(), "abc");
    EXPECT_FALSE(metadata.contains("xesam:url"));
    for (const QVariant &value : metadata)
        EXPECT_FALSE(value.toString().contains("SECRET"));
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    QApplication app(argc, argv);
    return RUN_ALL_TESTS();
}
