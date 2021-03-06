#include "playertest.h"
#include "library/track.h"
#include "mockaudiooutput.h"
#include "ui/mainwindow.h"
#include <playlistmodel.h>
#include <QDebug>

#define verify(track) {if (audio_->currentSource() != track) qDebug() << "Current source is" << audio_->currentSource() << "!=" << track; QVERIFY(audio_->currentSource() == track); }

QList<PTrack> generateTracks(int start, int no)
{
    QList<PTrack> result;
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

PModel PlayerTest::newPlaylist()
{
    auto p = player_->createPlaylist(false);
    mw_->addPlaylist(p, "", false);
    return p;
}

PModel PlayerTest::newPlaylist(QList<PTrack> tracks)
{
    auto p = newPlaylist();
    p->addTracks(tracks);
    return p;
}

void PlayerTest::removeAt(PModel p, int pos)
{
    p->removeIndexes({p->index(pos, 0)});
}

void PlayerTest::enqueue(PModel p, int pos)
{
    player_->enqueueTracks(p, {QModelIndex(p->index(pos, 0))});
}

void PlayerTest::deletePlaylist(int pos)
{
    mw_->removePlaylistTab(pos);
}

void PlayerTest::init()
{
    audio_ = new MockAudioOutput();
    player_ = new AudioPlayer(nullptr, audio_, true);
    mw_ = new MainWindow(*player_);
    player_->setMainWindow(mw_);
}

void PlayerTest::cleanup()
{
    delete mw_;
    delete player_;
    delete audio_;
}

void PlayerTest::noPlaylists()
{
    player_->play();
}

void PlayerTest::oneEmptyPlaylist()
{
    newPlaylist();
    player_->play();
}

void PlayerTest::oneTrack()
{
    newPlaylist(generateTracks(1));
    player_->play();
    verify("0");
    player_->next();
    player_->prev();
}

void PlayerTest::prevNext()
{
    newPlaylist(generateTracks(2));
    player_->play();
    verify("0");
    player_->next();
    verify("1");
    player_->prev();
    verify("0");
}

void PlayerTest::autoEnqueueNext()
{
    newPlaylist(generateTracks(2));
    player_->play();
    audio_->triggerAboutToFinish();
    audio_->triggerCurrentSourceChanged();
    verify("1");
    audio_->triggerAboutToFinish();
    audio_->triggerCurrentSourceChanged();
    verify("");
    player_->play();
    verify("1");
}

void PlayerTest::autoEnqueueNextDeletedHelper()
{
    auto p = newPlaylist(generateTracks(3));
    player_->play();
    audio_->triggerAboutToFinish();
    removeAt(p, 1);
    audio_->triggerCurrentSourceChanged();
    verify("1");
}

void PlayerTest::autoEnqueueNextDeletedNext()
{
    autoEnqueueNextDeletedHelper();
    player_->next();
    verify("2");
}

void PlayerTest::autoEnqueueNextDeletedPrev()
{
    autoEnqueueNextDeletedHelper();
    player_->prev();
    verify("0");
}

void PlayerTest::autoEnqueueNextDeletedThenAutoEnqueue()
{
    auto p = newPlaylist(generateTracks(3));
    player_->play();
    audio_->triggerAboutToFinish();
    removeAt(p, 1);
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
    player_->play();
    verify("0");
    enqueue(p1, 2);
    enqueue(p2, 1);
    player_->next();
    verify("2");
    audio_->triggerAboutToFinish();
    audio_->triggerCurrentSourceChanged();
    verify("4");
    player_->next();
    verify("5");

    enqueue(p1, 0);
    enqueue(p1, 0);
    enqueue(p1, 0);
    player_->next();
    verify("0");
    player_->next();
    verify("0");
    player_->clearQueue();
    player_->next();
    verify("1");
}

void PlayerTest::enqueueDeleted()
{
    auto p1 = newPlaylist(generateTracks(3));
    player_->play();
    verify("0");
    enqueue(p1, 1);
    removeAt(p1, 1);
    player_->next();
    verify("2");
    enqueue(p1, 1);
    audio_->triggerAboutToFinish();
    removeAt(p1, 1);
    audio_->triggerCurrentSourceChanged();
    verify("2");
    player_->play();
    verify("0");
}

void PlayerTest::enqueueDeletedPlaylist()
{
    auto p1 = newPlaylist(generateTracks(3));
    auto p2 = newPlaylist(generateTracks(3, 3));
    player_->play();
    verify("0");
    enqueue(p2, 1);
    deletePlaylist(1);
    player_->next();
    verify("1");

    // Delete playlist after buffering
    p2 = newPlaylist(generateTracks(3, 3));
    enqueue(p2, 1);
    audio_->triggerAboutToFinish();
    deletePlaylist(1);
    audio_->triggerCurrentSourceChanged();
    verify("4");
    player_->next();
    verify("2");

    // Delete playlist before buffering
    player_->prev();
    verify("1");
    p2 = newPlaylist(generateTracks(3, 3));
    enqueue(p2, 1);
    deletePlaylist(1);
    audio_->triggerAboutToFinish();
    audio_->triggerCurrentSourceChanged();
    verify("2");

    // Delete same playlist before buffering
    player_->prev();
    verify("1");
    enqueue(p1, 0);
    deletePlaylist(0);
    audio_->triggerAboutToFinish();
    audio_->triggerCurrentSourceChanged();
    verify("");
}

QTEST_MAIN(PlayerTest)
#include "playertest.moc"