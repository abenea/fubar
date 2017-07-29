#pragma once

#include <QObject>
#include <QtPlugin>
#include <QMap>
#include <QString>
#include "plugins/PluginInterface.h"
#include "library/track_forward.h"
#include <lastfm5/Track.h>
#include <lastfm5/Audioscrobbler.h>
#include <memory>

class QNetworkReply;

namespace lastfm
{
class Audioscrobbler;
}

class Preferences;

class LastfmPlugin : public QObject, public PluginInterface
{
    Q_OBJECT
    Q_INTERFACES(PluginInterface)
    Q_PLUGIN_METADATA(IID "fubar.lastfm" FILE "lastfm.json")

public:
    explicit LastfmPlugin(QObject* parent = 0);
    ~LastfmPlugin();

    void init(QObject& fubarApp);
    void deinit();
    void configure();

private slots:
    void trackPlaying(PTrack track);
    void stopped(qint64 /*ms*/ finalPosition, qint64 /*ms*/ trackLength);
    void trackPositionChanged(qint64 position, bool userSeek);
    void onAuthenticated();

private:
    QMap< QString, QNetworkReply* > m_jobs;

    void login();
    void logout();
    void gotSession();
    void resetVariables();
    void checkScrobble();
    void copyTrackMetadata(lastfm::MutableTrack& to, PTrack from);

    std::unique_ptr<lastfm::Audioscrobbler> m_scrobbler;
    lastfm::MutableTrack m_current;
    qint64 m_lastPosition;
    qint64 m_totalPlayed;

    QString m_sessionKey;
    char *m_sessionKeyArray;

    QObject* fubar_;

    friend class Preferences;
};
