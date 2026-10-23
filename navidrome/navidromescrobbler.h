#pragma once

#include "library/track_forward.h"

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <memory>

class NavidromeClient;
class QSettings;

// Reports the Navidrome tracks played by `player` as now playing and, once played long enough, as
// submissions. Submissions stay queued in `settings` (nullptr = the default QSettings) until the
// server accepts them or reports the song as not found; an account change (server or username)
// drops the queue. `player` is an AudioPlayer, connected by signal name: trackPlaying(PTrack),
// trackPositionChanged(qint64,bool), stopped(qint64,qint64). `client` and `settings`, if given,
// must outlive the scrobbler.
class NavidromeScrobbler : public QObject {
    Q_OBJECT
public:
    NavidromeScrobbler(NavidromeClient &client,
                       QObject *player,
                       QSettings *settings = nullptr,
                       QObject *parent = nullptr);
    ~NavidromeScrobbler() override;

    // Retries the queued submissions.
    void flushPending();

private slots:
    void trackPlaying(PTrack track);
    void trackPositionChanged(qint64 position, bool userSeek);
    // Queues and submits the current track if it was played long enough.
    void finishTrack();

private:
    bool enabled() const;
    void onAccountChanged();
    void submit(const QString &entry);
    QStringList pending() const;
    void setPending(const QStringList &entries);

    NavidromeClient &client_;
    std::unique_ptr<QSettings> ownSettings_;
    QSettings *settings_;
    QString songId_;
    qint64 durationMs_ = 0;
    qint64 startMs_ = 0;
    qint64 playedMs_ = 0;
    qint64 lastPosition_ = 0;
    // Queue entries with a submission in flight.
    QSet<QString> submitting_;
};
