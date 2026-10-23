#include "navidrome/navidromescrobbler.h"

#include "library/track.h"
#include "navidrome/navidromeclient.h"
#include "navidrome/navidrometrack.h"
#include <QDateTime>
#include <QJsonObject>
#include <QSettings>
#include <algorithm>

namespace {

const char kPendingKey[] = "navidrome/pendingScrobbles";
constexpr int kMaxPending = 500;
constexpr qint64 kMinDurationMs = 30000;
constexpr qint64 kMaxRequiredMs = 240000;
constexpr qint64 kMaxStepMs = 4000;
constexpr int kNotFoundCode = 70;

// Errors are logged by the client.
void ignoreResult(const QJsonObject &, const QString &) {}

// Splits a queue entry "<id> <ms>"; false if it is malformed.
bool parseEntry(const QString &entry, QString *songId, qint64 *timeMs) {
    int space = entry.lastIndexOf(' ');
    bool valid = false;
    *timeMs = entry.mid(space + 1).toLongLong(&valid);
    *songId = entry.left(space);
    return space > 0 && valid;
}

bool isValidEntry(const QString &entry) {
    QString songId;
    qint64 timeMs = 0;
    return parseEntry(entry, &songId, &timeMs);
}

// The server will never accept a submission of a song it does not know.
bool isSongNotFound(const QJsonObject &response) {
    return response.value("status").toString() == "failed" &&
           response.value("error").toObject().value("code").toInt() == kNotFoundCode;
}

} // namespace

NavidromeScrobbler::NavidromeScrobbler(NavidromeClient &client,
                                       QObject *player,
                                       QSettings *settings,
                                       QObject *parent)
    : QObject(parent),
      client_(client),
      ownSettings_(settings ? nullptr : new QSettings),
      settings_(settings ? settings : ownSettings_.get()) {
    connect(player, SIGNAL(trackPlaying(PTrack)), this, SLOT(trackPlaying(PTrack)));
    connect(player, SIGNAL(trackPositionChanged(qint64, bool)), this,
            SLOT(trackPositionChanged(qint64, bool)));
    connect(player, SIGNAL(stopped(qint64, qint64)), this, SLOT(finishTrack()));
    connect(&client_, &NavidromeClient::accountChanged, this,
            &NavidromeScrobbler::onAccountChanged);
}

NavidromeScrobbler::~NavidromeScrobbler() = default;

void NavidromeScrobbler::flushPending() {
    if (!enabled())
        return;
    QStringList entries = pending();
    auto malformed = std::remove_if(entries.begin(), entries.end(),
                                    [](const QString &entry) { return !isValidEntry(entry); });
    if (malformed != entries.end()) {
        entries.erase(malformed, entries.end());
        setPending(entries);
    }
    for (const QString &entry : entries)
        submit(entry);
}

void NavidromeScrobbler::trackPlaying(PTrack track) {
    finishTrack();
    if (!track || !enabled())
        return;
    QString songId = navidrome::songIdFromLocation(track->location);
    if (songId.isEmpty())
        return;
    songId_ = songId;
    durationMs_ = qint64(track->audioproperties.length) * 1000;
    startMs_ = QDateTime::currentMSecsSinceEpoch();
    client_.scrobble(songId_, false, 0, this, ignoreResult);
}

void NavidromeScrobbler::trackPositionChanged(qint64 position, bool userSeek) {
    if (!userSeek && position > lastPosition_ && position - lastPosition_ < kMaxStepMs)
        playedMs_ += position - lastPosition_;
    lastPosition_ = position;
}

bool NavidromeScrobbler::enabled() const {
    return client_.isConfigured() && client_.config().scrobble;
}

void NavidromeScrobbler::onAccountChanged() {
    // Plays of the previous account are never submitted to the new one.
    songId_.clear();
    setPending(QStringList());
}

void NavidromeScrobbler::finishTrack() {
    bool played = !songId_.isEmpty() && durationMs_ > kMinDurationMs &&
                  playedMs_ >= qMin(durationMs_ / 2, kMaxRequiredMs);
    QString entry = songId_ + ' ' + QString::number(startMs_);
    songId_.clear();
    durationMs_ = startMs_ = playedMs_ = lastPosition_ = 0;
    if (!played || !enabled())
        return;
    QStringList entries = pending();
    entries.append(entry);
    while (entries.size() > kMaxPending)
        entries.removeFirst();
    setPending(entries);
    submit(entry);
}

void NavidromeScrobbler::submit(const QString &entry) {
    QString songId;
    qint64 timeMs = 0;
    if (!parseEntry(entry, &songId, &timeMs) || submitting_.contains(entry))
        return;
    submitting_.insert(entry);
    client_.scrobble(songId, true, timeMs, this,
                     [this, entry](const QJsonObject &response, const QString &error) {
                         submitting_.remove(entry);
                         if (!error.isEmpty() && !isSongNotFound(response))
                             return;
                         QStringList entries = pending();
                         entries.removeAll(entry);
                         setPending(entries);
                         if (error.isEmpty())
                             flushPending();
                     });
}

QStringList NavidromeScrobbler::pending() const {
    return settings_->value(kPendingKey).toStringList();
}

void NavidromeScrobbler::setPending(const QStringList &entries) {
    if (entries.isEmpty())
        settings_->remove(kPendingKey);
    else
        settings_->setValue(kPendingKey, entries);
}
