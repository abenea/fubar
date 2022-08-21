#pragma once

#include "player/tests/mockaudiooutput.h"
#include <QtTest/QTest>
#include <ui/audioplayer.h>

class MainWindow;
class AudioPlayer;
class MockAudioOutput;

class PlayerTest : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void noPlaylists();
    void oneEmptyPlaylist();
    void oneTrack();
    void prevNext();

    void autoEnqueueNext();
    void autoEnqueueNextDeletedNext();
    void autoEnqueueNextDeletedPrev();
    void autoEnqueueNextDeletedThenAutoEnqueue();

    void enqueue();
    void enqueueDeleted();
    void enqueueDeletedPlaylist();

private:
    void autoEnqueueNextDeletedHelper();
    PModel newPlaylist();
    PModel newPlaylist(QList<PTrack> tracks);
    void deletePlaylist(int pos);
    void removeAt(PModel p, int pos);
    void enqueue(PModel p, int pos);

    MockAudioOutput audioOutput_;
    AudioPlayer *player_;
    MainWindow *mw_;
};
