#include "ui/lyricsthreaddeleter.h"
#include <QThread>

LyricsThreadDeleter::LyricsThreadDeleter(QThread *thread) : thread_(thread) {
    QObject::connect(thread, SIGNAL(finished()), this, SLOT(cleanup()));
}

void LyricsThreadDeleter::cleanup() {
    if (thread_) {
        delete thread_;
        thread_ = nullptr;
    }
    deleteLater();
}
