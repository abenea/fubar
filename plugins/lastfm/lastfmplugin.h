#ifndef LASTFMPLUGIN_H
#define LASTFMPLUGIN_H

#include <QObject>
#include <QtPlugin>
#include <QMap>
#include <QString>
#include "../PluginInterface.h"
#include "../../library/track.h"
#include <lastfm/Track.h>
#include <lastfm/Audioscrobbler.h>
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

public:
    explicit LastfmPlugin(QObject* parent = 0);
    ~LastfmPlugin();

    void init(QObject& fubarApp);
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

    QObject* mainWindow_;

    friend Preferences;
};

#endif // LASTFMPLUGIN_H
