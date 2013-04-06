#include "playertest.h"
#include <ui/mainwindow.h>
#include <ui/playlisttab.h>
#include <QDebug>

#define verify(track) {if (audio_->currentSource() != track) qDebug() << "Current source is" << audio_->currentSource() << "!=" << track; QVERIFY(audio_->currentSource() == track); }

PlaylistTab* newPlaylist()
{
    return new PlaylistTab(false);
}

PlaylistTab* newPlaylist(QList<PTrack> tracks)
{
    auto p = new PlaylistTab(false);
    p->addTracks(tracks);
    return p;
}

QList<PTrack> generateTracks(int start, int no)
{
    QList<std::shared_ptr<Track>> result;
    for (int i = start; i < start + no; ++i) {
        auto track = new Track();
        track->location = QString("%1").arg(i);
        result.append(PTrack(track));
    }
    return result;
}

QList<PTrack> generateTracks(int no)
{
    return generateTracks(0, no);
}

void PlayerTest::addPlaylist(PlaylistTab* p)
{
    mw_->addPlaylist(p, "", false);
}

void PlayerTest::init()
{
    audio_ = new MockAudioOutput();
    mw_ = new MainWindow(nullptr, audio_, nullptr, true);
}

void PlayerTest::cleanup()
{
    delete mw_;
    delete audio_;
}

void PlayerTest::noPlaylists()
{
    mw_->play();
}

void PlayerTest::oneEmptyPlaylist()
{
    addPlaylist(newPlaylist());
    mw_->play();
}

void PlayerTest::oneTrack()
{
    addPlaylist(newPlaylist(generateTracks(1)));
    mw_->play();
    verify("0");
    mw_->next();
    mw_->prev();
}

void PlayerTest::prevNext()
{
    addPlaylist(newPlaylist(generateTracks(2)));
    mw_->play();
    verify("0");
    mw_->next();
    verify("1");
    mw_->prev();
    verify("0");
}

void PlayerTest::autoEnqueueNext()
{
    addPlaylist(newPlaylist(generateTracks(2)));
    mw_->play();
    audio_->triggerAboutToFinish();
    audio_->triggerCurrentSourceChanged();
    verify("1");
    audio_->triggerAboutToFinish();
    audio_->triggerCurrentSourceChanged();
    verify("");
    mw_->play();
    verify("1");
}

void PlayerTest::autoEnqueueNextDeleted()
{
    auto p = newPlaylist(generateTracks(3));
    addPlaylist(p);
    mw_->play();
    audio_->triggerAboutToFinish();
    p->removeTrackAt(1);
    audio_->triggerCurrentSourceChanged();
    verify("1");
    mw_->next();
    verify("2");
}

void PlayerTest::autoEnqueueNextDeletedThenAutoEnqueue()
{
    auto p = newPlaylist(generateTracks(3));
    addPlaylist(p);
    mw_->play();
    audio_->triggerAboutToFinish();
    p->removeTrackAt(1);
    audio_->triggerCurrentSourceChanged();
    verify("1");
    audio_->triggerAboutToFinish();
    audio_->triggerCurrentSourceChanged();
    verify("2");
}

void PlayerTest::enqueue()
{
    auto p1 = newPlaylist(generateTracks(3));
    auto p2 = newPlaylist(generateTracks(3, 3));
    addPlaylist(p1);
    addPlaylist(p2);
    mw_->play();
    verify("0");
    p1->enqueuePosition(2);
    p2->enqueuePosition(1);
    mw_->next();
    verify("2");
    audio_->triggerAboutToFinish();
    audio_->triggerCurrentSourceChanged();
    verify("4");
    mw_->next();
    verify("5");

    p1->enqueuePosition(0);
    p1->enqueuePosition(0);
    p1->enqueuePosition(0);
    mw_->next();
    verify("0");
    mw_->next();
    verify("0");
    mw_->on_clearQueueAction_triggered();
    mw_->next();
    verify("1");
}

void PlayerTest::enqueueDeleted()
{
    auto p1 = newPlaylist(generateTracks(3));
    addPlaylist(p1);
    mw_->play();
    verify("0");
    p1->enqueuePosition(1);
    p1->removeTrackAt(1);
    mw_->next();
    verify("2");
    p1->enqueuePosition(1);
    audio_->triggerAboutToFinish();
    p1->removeTrackAt(1);
    audio_->triggerCurrentSourceChanged();
    verify("2");
    mw_->play();
    verify("0");
}

void PlayerTest::enqueueDeletedPlaylist()
{
    auto p1 = newPlaylist(generateTracks(3));
    auto p2 = newPlaylist(generateTracks(3, 3));
    addPlaylist(p1);
    addPlaylist(p2);
    mw_->play();
    verify("0");
    p2->enqueuePosition(1);
    mw_->removePlaylistTab(1);
    mw_->next();
    verify("1");

    // Delete playlist after buffering
    p2 = newPlaylist(generateTracks(3, 3));
    addPlaylist(p2);
    p2->enqueuePosition(1);
    audio_->triggerAboutToFinish();
    mw_->removePlaylistTab(1);
    audio_->triggerCurrentSourceChanged();
    verify("4");
    mw_->next();
    verify("2");

    // Delete playlist before buffering
    mw_->prev();
    verify("1");
    p2 = newPlaylist(generateTracks(3, 3));
    addPlaylist(p2);
    p2->enqueuePosition(1);
    mw_->removePlaylistTab(1);
    audio_->triggerAboutToFinish();
    audio_->triggerCurrentSourceChanged();
    verify("2");

    // Delete same playlist before buffering
    mw_->prev();
    verify("1");
    p1->enqueuePosition(0);
    mw_->removePlaylistTab(0);
    audio_->triggerAboutToFinish();
    audio_->triggerCurrentSourceChanged();
    verify("");
}

QTEST_MAIN(PlayerTest)
#include "playertest.moc"