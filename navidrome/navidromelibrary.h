#pragma once

#include "library/track_forward.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <memory>
#include <optional>

class NavidromeClient;
class QJsonObject;

// All songs of the Navidrome account `client` is configured for, cached in `cacheFile`
// (empty = settingsDirFilePath("navidrome_library")). `client` must outlive the library.
class NavidromeLibrary : public QObject {
    Q_OBJECT
public:
    NavidromeLibrary(NavidromeClient &client, const QString &cacheFile, QObject *parent = nullptr);
    ~NavidromeLibrary() override;

    // Loads the cache on the first call if it belongs to the client's account.
    QList<PTrack> tracks();
    bool isSyncing() const;
    void setPageSize(int size);
    // checkForUpdates() runs again after this delay (default 1 min) when a sync or a check saw the
    // server scanning, or when a sync failed while no scan status is known.
    void setRecheckDelay(int ms);

public slots:
    // Fetches all songs; no-op if not configured or already syncing.
    void sync();
    // Syncs if the server's last scan time or song count differ from the cached ones (or none
    // are cached); checks again later while the server is scanning.
    void checkForUpdates();

signals:
    // Emitted only on real changes. `tracks` is the complete new list: unchanged songs keep their
    // track objects, locations absent from it were removed.
    void tracksUpdated(QList<PTrack> tracks);
    // The account (server or username) changed: replace all tracks. An ongoing sync is dropped
    // without syncFinished/syncFailed.
    void tracksReset(QList<PTrack> tracks);
    void syncStarted();
    void syncFinished(int trackCount);
    void syncFailed(QString error);

private:
    struct ScanStatus {
        QString lastScan;
        int count = 0;

        bool operator==(const ScanStatus &other) const {
            return lastScan == other.lastScan && count == other.count;
        }
        bool operator!=(const ScanStatus &other) const { return !(*this == other); }
        // Of a getScanStatus response
        static ScanStatus fromResponse(const QJsonObject &response);
    };
    struct Sync;

    void onAccountChanged();
    void ensureCacheLoaded();
    // false if not configured or already syncing
    bool beginSync();
    // Fetches the songs of the server with the scan status `status`.
    void startSync(const ScanStatus &status);
    void fetchPage();
    void finishSync();
    void failSync(const QString &error);
    void scheduleRecheck();
    std::unique_ptr<Sync> endSync();
    void loadCache();
    void writeCache();

    NavidromeClient &client_;
    QString cacheFile_;
    int pageSize_ = 500;
    int recheckDelayMs_ = 60 * 1000;
    bool recheckPending_ = false;
    bool cacheLoaded_ = false;
    QList<PTrack> tracks_;
    // Serialized proto::Track of each freshly mapped song, by location.
    QHash<QString, QByteArray> fingerprints_;
    // Of the last sync or the cache
    std::optional<ScanStatus> scanStatus_;
    // Incremented whenever pending replies become stale.
    int generation_ = 0;
    std::unique_ptr<Sync> sync_;
};
