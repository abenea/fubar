#ifndef PLAYERTEST_H
#define PLAYERTEST_H

#include "ui/mainwindow.h"
#include "mockaudiooutput.h"
#include <QtTest/QTest>

class PlaylistTab;

class PlayerTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void noPlaylists();
    void oneEmptyPlaylist();
    void oneTrack();
    void prevNext();

    void autoEnqueueNext();
    void autoEnqueueNextDeleted();
    void autoEnqueueNextDeletedThenAutoEnqueue();

    void enqueue();
    void enqueueDeleted();
    void enqueueDeletedPlaylist();

private:
    void addPlaylist(PlaylistTab* p);

    MainWindow* mw_;
    MockAudioOutput* audio_;
};
#endif // PLAYERTEST_H
