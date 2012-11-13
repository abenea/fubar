#include "lastfmplugin.h"
#include <QDebug>
#include <QCryptographicHash>
#include <QNetworkReply>
#include <QSettings>
#include <QDir>
#include <lastfm/ws.h>
#include <lastfm/XmlQuery.h>
#include <cstdio>

LastfmPlugin::LastfmPlugin(QObject* parent)
    : QObject(parent)
    , m_sessionKeyArray(0)
{

}

LastfmPlugin::~LastfmPlugin()
{
    qDebug() << "[Lastfm] LastfmPlugin::~LastfmPlugin()";
}

void LastfmPlugin::init(QObject& fubarApp)
{
    qDebug() << "[Lastfm] LastfmPlugin::LastfmPlugin()";
    theApp = &fubarApp;

    QString username;
    QString password;

    // Read config
    FILE* cfg = fopen("lastfm.cfg", "rt");
    if (!cfg) {
        qDebug() << "[Lastfm] Cannot find lastfm.cfg";
        return;
    }
    char user[50], pass[50];
    fscanf(cfg, "%s %s", user, pass);
    fclose(cfg);

    username = user;
    password = pass;

    // set the global static Lastfm::Ws stuff
    lastfm::ws::ApiKey = "bf8e2991b5fee92612b2f4c22f1d4a8b";
    lastfm::ws::SharedSecret = "0d3a53011c519ae243bae01ec1e4e87f";
    lastfm::ws::Username = username.toLatin1().data();

    QSettings settings;
    m_sessionKey = settings.value("lastfm/sessionKey").toString();

    // now authenticate w/ last.fm and get our session key if we don't have one
    if (m_sessionKey.isEmpty()) {
        qDebug() << "[Lastfm] got no saved session key, authenticating with last.fm";
        QMap<QString, QString> query;
        query["method"] = QString("auth.getMobileSession");
        query["username"] = username;
        query["password"] = password;
        m_jobs["auth"] = lastfm::ws::post(query);
        connect(m_jobs["auth"], SIGNAL(finished()), SLOT(onAuthenticated()));
    } else {
        qDebug() << "[Lastfm] using saved sessionkey from last.fm";
        gotSession();
    }

}

void LastfmPlugin::onAuthenticated()
{
    if(!m_jobs["auth"]) {
        qDebug() << "[Lastfm] WARNING: GOT RESULT but no object";
        return;
    }

    switch (m_jobs["auth"]->error()) {
        case QNetworkReply::NoError:
        {
            lastfm::XmlQuery lfm;
            lfm.parse((m_jobs["auth"]->readAll()));

            if (lfm.children("error").size() > 0) {
                qDebug() << "[Lastfm] error from authenticating with last.fm service:" << lfm.text();
                QSettings settings;
                settings.setValue("lastfm/sessionKey", "");
                return;
            }
            m_sessionKey = lfm["session"]["key"].text();

            QSettings settings;
            settings.setValue("lastfm/sessionKey", m_sessionKey);

            gotSession();
            break;
        }
        case QNetworkReply::AuthenticationRequiredError:
            qDebug() << "[Lastfm] errorMessage " << "Either the username was not recognized, or the password was incorrect.";
            return;

        default:
            qDebug() << "[Lastfm] errorMessage " << m_jobs[ "auth" ]->error() << "There was a problem communicating with the Last.fm services. Please try again later.";
            for (QNetworkReply::RawHeaderPair p : m_jobs[ "auth" ]->rawHeaderPairs())
                qDebug() << p.first << " -> " << p.second;
            qDebug() << m_jobs[ "auth" ]->readAll();
            return;
    }
    m_jobs[ "auth" ]->deleteLater();
}

void LastfmPlugin::gotSession()
{
    qDebug() << "[Lastfm] Last.fm session " << m_sessionKey;
    m_sessionKeyArray = qstrdup(m_sessionKey.toLatin1().data());
    lastfm::ws::SessionKey = m_sessionKeyArray;

    m_scrobbler.reset(new lastfm::Audioscrobbler("fubar"));
    resetVariables();

    //HACK work around a bug in liblastfm---it doesn't create its config dir, so when it
    // tries to write the track cache, it fails silently. until we have a fixed version, do this
    // path finding code taken from liblastfm/src/misc.cpp
    QString lpath = QDir::home().filePath( ".local/share/Last.fm" );
    QDir ldir = QDir( lpath );
    if( !ldir.exists() )
    {
        ldir.mkpath( lpath );
    }

    connect(theApp, SIGNAL(trackPlaying(PTrack)), this, SLOT(trackPlaying(PTrack)));
    connect(theApp, SIGNAL(stopped(qint64, qint64)), this, SLOT(stopped(qint64, qint64)));
    connect(theApp, SIGNAL(trackPositionChanged(qint64, bool)), this, SLOT(trackPositionChanged(qint64, bool)));
}

void LastfmPlugin::stopped(qint64 finalPosition, qint64 /*trackLength*/)
{
//    qDebug() << "[Lastfm] LastfmPlugin got stopped " << finalPosition;
    trackPositionChanged(finalPosition, false);
    checkScrobble();
}

void LastfmPlugin::trackPlaying(PTrack track)
{
//    qDebug() << "[Lastfm] LastfmPlugin got trackPlaying " << track->location;
    checkScrobble();
    m_current.stamp();

    m_current.setDuration(track->audioproperties.length);
    copyTrackMetadata(m_current, track);
    m_current.setSource(lastfm::Track::Player);
    if (!m_current.isNull()) {
//        qDebug() << "[Lastfm] nowPlaying: " << m_current.artist() << " - " << m_current.album() << " - " << m_current.title();
        m_scrobbler->nowPlaying(m_current);
    }
}

void LastfmPlugin::trackPositionChanged(qint64 position, bool userSeek)
{
//    qDebug() << "[Lastfm] LastfmPlugin got trackPositionChanged " << position;
    //qDebug() << "[Lastfm] userSeek" << userSeek << "position:" << position << "m_lastPosition" << m_lastPosition << "m_totalPlayed" << m_totalPlayed;
    if( !userSeek && position > m_lastPosition && ( position - m_lastPosition ) < 4000 )
        m_totalPlayed += position - m_lastPosition;
    m_lastPosition = position;
    //qDebug() << "[Lastfm] userSeek" << userSeek << "position:" << position << "m_lastPosition" << m_lastPosition << "m_totalPlayed" << m_totalPlayed;
}

void LastfmPlugin::copyTrackMetadata(lastfm::MutableTrack& to, PTrack from)
{
    to.setAlbum(from->metadata["album"]);
    to.setArtist(from->metadata["artist"]);
    to.setTitle(from->metadata["title"]);
}

void LastfmPlugin::checkScrobble()
{
//    qDebug() << "[Lastfm] total played" << m_totalPlayed << "duration" << m_current.duration() * 1000 / 2 << "isNull" << m_current.isNull();
    if (( m_totalPlayed > m_current.duration() * 1000 / 2) && !m_current.isNull()) {
        qDebug() << "[Lastfm] Scrobble: " << m_current.artist() << " - " << m_current.album() << " - " << m_current.title();
        m_scrobbler->cache(m_current);
        m_scrobbler->submit();
    }
    resetVariables();
}

void LastfmPlugin::resetVariables()
{
    m_current = lastfm::MutableTrack();
    m_totalPlayed = m_lastPosition = 0;
}

QT_BEGIN_NAMESPACE
Q_EXPORT_PLUGIN2(lastfm, LastfmPlugin);
QT_END_NAMESPACE
