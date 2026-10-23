#include "navidrome/navidromelibrary.h"

#include "library/track.h"
#include "library/track.pb.h"
#include "navidrome/navidromeclient.h"
#include "navidrome/navidrometrack.h"
#include "util.h"
#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <QTimer>
#include <string>

namespace {

const char kCacheMagic[] = "fubar-navidrome";
constexpr qint32 kCacheVersion = 1;
constexpr int kMaxSongs = 1000000;

QByteArray fingerprint(Track &track) {
    proto::Track ptrack;
    track.fillProtoTrack(ptrack);
    return QByteArray::fromStdString(ptrack.SerializeAsString());
}

bool isScanning(const QJsonObject &response) {
    return response.value("scanStatus").toObject().value("scanning").toBool();
}

// Appends `track`, a serialized proto::Track, to the serialized proto::Library `library`.
void appendTrack(QByteArray &library, const QByteArray &track) {
    // The tracks field, length-delimited
    library.append(char((proto::Library::kTracksFieldNumber << 3) | 2));
    quint32 size = quint32(track.size());
    for (; size >= 0x80; size >>= 7)
        library.append(char((size & 0x7f) | 0x80));
    library.append(char(size));
    library.append(track);
}

} // namespace

NavidromeLibrary::ScanStatus
NavidromeLibrary::ScanStatus::fromResponse(const QJsonObject &response) {
    QJsonObject status = response.value("scanStatus").toObject();
    return ScanStatus{status.value("lastScan").toString(), status.value("count").toInt()};
}

struct NavidromeLibrary::Sync {
    NavidromeConfig config;
    QObject *context = nullptr;
    int pageSize = 0;
    int offset = 0;
    QList<PTrack> tracks;
    // By location
    QHash<QString, QByteArray> fingerprints;
    ScanStatus scanStatus;
};

NavidromeLibrary::NavidromeLibrary(NavidromeClient &client,
                                   const QString &cacheFile,
                                   QObject *parent)
    : QObject(parent),
      client_(client),
      cacheFile_(cacheFile.isEmpty() ? settingsDirFilePath("navidrome_library") : cacheFile) {
    qRegisterMetaType<QList<PTrack>>();
    connect(&client_, &NavidromeClient::accountChanged, this, &NavidromeLibrary::onAccountChanged);
}

NavidromeLibrary::~NavidromeLibrary() = default;

QList<PTrack> NavidromeLibrary::tracks() {
    ensureCacheLoaded();
    return tracks_;
}

bool NavidromeLibrary::isSyncing() const { return sync_ != nullptr; }

void NavidromeLibrary::setPageSize(int size) { pageSize_ = qMax(1, size); }

void NavidromeLibrary::setRecheckDelay(int ms) { recheckDelayMs_ = ms; }

void NavidromeLibrary::sync() {
    if (!beginSync())
        return;
    int generation = generation_;
    client_.get(sync_->config, "getScanStatus", {}, sync_->context,
                [this, generation](const QJsonObject &response, const QString &error) {
                    if (generation != generation_)
                        return;
                    if (!error.isEmpty()) {
                        failSync(error);
                        return;
                    }
                    // Songs can change between the pages
                    if (isScanning(response))
                        scheduleRecheck();
                    sync_->scanStatus = ScanStatus::fromResponse(response);
                    fetchPage();
                });
    emit syncStarted();
}

void NavidromeLibrary::checkForUpdates() {
    if (sync_ || !client_.isConfigured())
        return;
    ensureCacheLoaded();
    int generation = generation_;
    client_.get(client_.config(), "getScanStatus", {}, this,
                [this, generation](const QJsonObject &response, const QString &error) {
                    if (generation != generation_ || !error.isEmpty())
                        return;
                    if (isScanning(response)) {
                        scheduleRecheck();
                        return;
                    }
                    ScanStatus status = ScanStatus::fromResponse(response);
                    if (scanStatus_ != status)
                        startSync(status);
                });
}

void NavidromeLibrary::onAccountChanged() {
    if (sync_)
        endSync();
    ++generation_;
    cacheLoaded_ = true;
    tracks_.clear();
    fingerprints_.clear();
    scanStatus_.reset();
    QFile::remove(cacheFile_);
    emit tracksReset(QList<PTrack>());
}

void NavidromeLibrary::ensureCacheLoaded() {
    if (cacheLoaded_)
        return;
    cacheLoaded_ = true;
    loadCache();
}

bool NavidromeLibrary::beginSync() {
    if (sync_ || !client_.isConfigured())
        return false;
    ensureCacheLoaded();
    sync_.reset(new Sync);
    sync_->config = client_.config();
    sync_->context = new QObject(this);
    sync_->pageSize = pageSize_;
    ++generation_;
    return true;
}

void NavidromeLibrary::startSync(const ScanStatus &status) {
    if (!beginSync())
        return;
    sync_->scanStatus = status;
    fetchPage();
    emit syncStarted();
}

void NavidromeLibrary::fetchPage() {
    int generation = generation_;
    NavidromeClient::Params params{{"query", ""},
                                   {"artistCount", "0"},
                                   {"albumCount", "0"},
                                   {"songCount", QString::number(sync_->pageSize)},
                                   {"songOffset", QString::number(sync_->offset)}};
    client_.get(sync_->config, "search3", params, sync_->context,
                [this, generation](const QJsonObject &response, const QString &error) {
                    if (generation != generation_)
                        return;
                    if (!error.isEmpty()) {
                        failSync(error);
                        return;
                    }
                    QJsonValue result = response.value("searchResult3");
                    if (!result.isObject()) {
                        failSync("Invalid response from server");
                        return;
                    }
                    QJsonArray songs = result.toObject().value("song").toArray();
                    for (const QJsonValue &song : songs) {
                        PTrack track = navidrome::trackFromSong(song.toObject());
                        if (!track || sync_->fingerprints.contains(track->location))
                            continue;
                        sync_->fingerprints.insert(track->location, fingerprint(*track));
                        sync_->tracks.append(track);
                    }
                    sync_->offset += songs.size();
                    if (songs.size() < sync_->pageSize || sync_->offset >= kMaxSongs)
                        finishSync();
                    else
                        fetchPage();
                });
}

void NavidromeLibrary::finishSync() {
    std::unique_ptr<Sync> sync = endSync();
    QHash<QString, PTrack> oldTracks;
    for (const PTrack &track : tracks_)
        oldTracks.insert(track->location, track);

    QList<PTrack> tracks;
    bool modified = sync->tracks.size() != tracks_.size();
    for (const PTrack &track : sync->tracks) {
        auto old = fingerprints_.constFind(track->location);
        if (old != fingerprints_.constEnd() && *old == sync->fingerprints.value(track->location)) {
            tracks.append(oldTracks.value(track->location));
        } else {
            modified = true;
            tracks.append(track);
        }
    }

    bool scanChanged = scanStatus_ != sync->scanStatus;
    scanStatus_ = sync->scanStatus;
    if (modified) {
        tracks_ = tracks;
        fingerprints_ = std::move(sync->fingerprints);
    }
    if (modified || scanChanged)
        writeCache();
    if (modified)
        emit tracksUpdated(tracks_);
    emit syncFinished(tracks_.size());
}

void NavidromeLibrary::failSync(const QString &error) {
    endSync();
    emit syncFailed(error);
    // E.g. a first sync while the network is not up yet
    if (!scanStatus_)
        scheduleRecheck();
}

void NavidromeLibrary::scheduleRecheck() {
    if (recheckPending_)
        return;
    recheckPending_ = true;
    QTimer::singleShot(recheckDelayMs_, this, [this]() {
        recheckPending_ = false;
        // checkForUpdates() would do nothing
        if (sync_)
            scheduleRecheck();
        else
            checkForUpdates();
    });
}

std::unique_ptr<NavidromeLibrary::Sync> NavidromeLibrary::endSync() {
    ++generation_;
    // Not deleted directly: this may run inside one of its callbacks.
    sync_->context->deleteLater();
    return std::move(sync_);
}

void NavidromeLibrary::loadCache() {
    const NavidromeConfig &account = client_.config();
    QFile file(cacheFile_);
    if (account.serverUrl.isEmpty() || !file.open(QIODevice::ReadOnly))
        return;
    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_5_15);
    QByteArray magic(sizeof(kCacheMagic) - 1, Qt::Uninitialized);
    if (stream.readRawData(magic.data(), magic.size()) != magic.size() || magic != kCacheMagic)
        return;
    qint32 version = 0;
    stream >> version;
    if (version != kCacheVersion)
        return;
    QString serverUrl, username, lastScan;
    qint32 scanCount = 0;
    QByteArray bytes;
    stream >> serverUrl >> username >> lastScan >> scanCount >> bytes;
    if (stream.status() != QDataStream::Ok || serverUrl != account.serverUrl ||
        username != account.username)
        return;
    proto::Library plibrary;
    if (!plibrary.ParseFromArray(bytes.constData(), bytes.size())) {
        qDebug() << "[Navidrome] Cannot parse the library cache";
        return;
    }
    for (int i = 0; i < plibrary.tracks_size(); ++i) {
        const proto::Track &ptrack = plibrary.tracks(i);
        PTrack track = std::make_shared<Track>(ptrack);
        if (fingerprints_.contains(track->location))
            continue;
        // The record was written from the fingerprint
        fingerprints_.insert(track->location,
                             QByteArray::fromStdString(ptrack.SerializeAsString()));
        tracks_.append(track);
    }
    scanStatus_ = ScanStatus{lastScan, scanCount};
}

void NavidromeLibrary::writeCache() {
    // Not null even when empty: QDataStream writes a null QByteArray differently
    QByteArray library("");
    for (const PTrack &track : tracks_)
        appendTrack(library, fingerprints_.value(track->location));
    const NavidromeConfig &account = client_.config();
    QDir().mkpath(QFileInfo(cacheFile_).absolutePath());
    QSaveFile file(cacheFile_);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "[Navidrome] Cannot write the library cache";
        return;
    }
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_5_15);
    stream.writeRawData(kCacheMagic, sizeof(kCacheMagic) - 1);
    stream << kCacheVersion << account.serverUrl << account.username << scanStatus_->lastScan
           << qint32(scanStatus_->count) << library;
    if (stream.status() != QDataStream::Ok || !file.commit())
        qDebug() << "[Navidrome] Cannot write the library cache";
}
