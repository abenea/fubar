#pragma once

// Header-only: also used by tests that do not link the test libraries.
#include "library/track.h"
#include "navidrome/navidromeconfig.h"
#include "navidrome/navidrometrack.h"

#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <ostream>

QT_BEGIN_NAMESPACE
inline void PrintTo(const QString &qString, ::std::ostream *os) { *os << qUtf8Printable(qString); }
QT_END_NAMESPACE

constexpr const char *kSalt = "c19b2d";
constexpr const char *kToken = "26719a1196d2a940705a59634eb18eab"; // makeToken("sesame", kSalt)

inline NavidromeConfig testConfig(const QString &serverUrl = "http://music.example:4533",
                                  const QString &username = "listener") {
    NavidromeConfig config;
    config.serverUrl = serverUrl;
    config.username = username;
    config.salt = kSalt;
    config.token = kToken;
    return config;
}

inline QStringList songLocations(const QStringList &ids) {
    QStringList locations;
    for (const QString &id : ids)
        locations << navidrome::locationForSong(id);
    return locations;
}

inline QStringList locationsOf(const QList<PTrack> &tracks) {
    QStringList locations;
    for (const PTrack &track : tracks)
        locations << track->location;
    return locations;
}

// Collects every Qt message while alive (also from other threads).
class MessageCapture {
public:
    MessageCapture() {
        {
            QMutexLocker lock(&mutex());
            captured().clear();
        }
        previous_ = qInstallMessageHandler(&MessageCapture::handle);
    }
    ~MessageCapture() { qInstallMessageHandler(previous_); }

    QStringList messages() const {
        QMutexLocker lock(&mutex());
        return captured();
    }
    QString text() const { return messages().join('\n'); }

private:
    static void handle(QtMsgType, const QMessageLogContext &, const QString &message) {
        QMutexLocker lock(&mutex());
        captured() << message;
    }
    static QStringList &captured() {
        static QStringList messages;
        return messages;
    }
    static QMutex &mutex() {
        static QMutex instance;
        return instance;
    }

    QtMessageHandler previous_;
};
