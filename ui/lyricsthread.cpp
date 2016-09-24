#include "lyricsthread.h"
#include "library/track.h"
#include "ui/mainwindow.h"
#include <glyr/glyr.h>
#include <boost/scope_exit.hpp>

void fetchLyrics(PTrack track) {
    GlyrQuery my_query;
    GlyrMemCache *list = nullptr;
    BOOST_SCOPE_EXIT(&my_query, &list) {
        if (list)
            glyr_free_list(list);
        glyr_query_destroy(&my_query);
    }
    BOOST_SCOPE_EXIT_END
    glyr_query_init(&my_query);
    glyr_opt_type(&my_query, GLYR_GET_LYRICS);
    glyr_opt_artist(&my_query, qPrintable(track->metadata["artist"]));
    glyr_opt_title(&my_query, qPrintable(track->metadata["title"]));
    list = glyr_get(&my_query, NULL, NULL);
    if (list)
        track->metadata["lyrics"] = list->data;
}

LyricsThread::LyricsThread(PTrack track, const MainWindow *mw) : track_(track) {
    QObject::connect(this, SIGNAL(lyricsUpdated(PTrack)), mw, SLOT(lyricsUpdated(PTrack)));
}

void LyricsThread::run() {
    fetchLyrics(track_);
    emit lyricsUpdated(track_);
}
