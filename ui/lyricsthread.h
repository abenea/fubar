#pragma once

#include "library/track_forward.h"
#include <QThread>

class MainWindow;

class LyricsThread : public QThread {
    Q_OBJECT
public:
    LyricsThread(PTrack track, const MainWindow *mw);

signals:
    void lyricsUpdated(PTrack track);

protected:
    void run();

private:
    PTrack track_;
};
