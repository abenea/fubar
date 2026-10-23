// Runs against the server named by NAVIDROME_TEST_CONFIG (skipped when unset). Failure messages
// must never contain URLs, response bodies or anything from the config file.
#include "library/track.h"
#include "navidrome/navidromeclient.h"
#include "navidrome/navidromelibrary.h"
#include "navidrome/navidrometrack.h"
#include "gtest/gtest.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <algorithm>
#include <memory>

namespace {

// File format: "<server url>\nuser <name>\npassword <password>\n".
bool loadLiveConfig(NavidromeConfig *config) {
    QString fileName = qEnvironmentVariable("NAVIDROME_TEST_CONFIG");
    QFile file(fileName);
    if (fileName.isEmpty() || !file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QString url, user, password;
    for (const QString &rawLine : QString::fromUtf8(file.readAll()).split('\n')) {
        QString line = rawLine.trimmed();
        if (line.startsWith("http")) {
            url = line;
            continue;
        }
        QString key = line.section(' ', 0, 0).toLower();
        QString value = line.section(' ', 1).trimmed();
        if (key == "user")
            user = value;
        else if (key == "password")
            password = value;
    }
    if (url.isEmpty() || user.isEmpty() || password.isEmpty())
        return false;
    config->serverUrl = NavidromeConfig::normalizeServerUrl(url);
    config->username = user;
    config->setPassword(password);
    config->scrobble = false;
    return true;
}

struct Result {
    bool called = false;
    QJsonObject response;
    QString error;
};

class NavidromeLiveTest : public testing::Test {
protected:
    void SetUp() override {
        NavidromeConfig config;
        if (!loadLiveConfig(&config))
            GTEST_SKIP() << "NAVIDROME_TEST_CONFIG is not set or not readable";
        client_.setConfig(config);
    }

    Result call(const std::function<void(NavidromeClient::Callback)> &request) {
        auto result = std::make_shared<Result>();
        request([result](const QJsonObject &response, const QString &error) {
            result->called = true;
            result->response = response;
            result->error = error;
        });
        static_cast<void>(QTest::qWaitFor([&] { return result->called; }, 30000));
        return *result;
    }

    // A search3 page of all songs
    Result songPage(int count, int offset) {
        NavidromeClient::Params params{{"query", ""},
                                       {"artistCount", "0"},
                                       {"albumCount", "0"},
                                       {"songCount", QString::number(count)},
                                       {"songOffset", QString::number(offset)}};
        return call(
            [&](NavidromeClient::Callback cb) { client_.get("search3", params, &context_, cb); });
    }

    QJsonArray firstSongs() {
        Result result = songPage(20, 0);
        EXPECT_TRUE(result.called);
        EXPECT_TRUE(result.error.isEmpty());
        return result.response.value("searchResult3").toObject().value("song").toArray();
    }

    NavidromeClient client_;
    QObject context_;
};

} // namespace

TEST_F(NavidromeLiveTest, Ping) {
    Result result =
        call([&](NavidromeClient::Callback cb) { client_.get("ping", {}, &context_, cb); });
    ASSERT_TRUE(result.called);
    EXPECT_TRUE(result.error.isEmpty());
    EXPECT_TRUE(result.response.value("status").toString() == "ok");
}

TEST_F(NavidromeLiveTest, Search3FirstPage) {
    QJsonArray songs = firstSongs();
    ASSERT_GT(songs.size(), 0);
    QList<PTrack> tracks = navidrome::tracksFromSongs(songs);
    EXPECT_EQ(tracks.size(), songs.size());
    for (const PTrack &track : tracks) {
        EXPECT_TRUE(navidrome::isNavidromeLocation(track->location));
        EXPECT_FALSE(navidrome::songIdFromLocation(track->location).isEmpty());
        EXPECT_FALSE(track->path().isEmpty());
        EXPECT_EQ(track->mtime, 0u);
    }
}

// The library treats a page without a searchResult3 object as an invalid response.
TEST_F(NavidromeLiveTest, Search3PastTheEndIsAnEmptyResult) {
    Result result = songPage(1, 1000000);
    ASSERT_TRUE(result.called);
    ASSERT_TRUE(result.error.isEmpty());
    QJsonValue searchResult = result.response.value("searchResult3");
    EXPECT_TRUE(searchResult.isObject());
    EXPECT_EQ(searchResult.toObject().value("song").toArray().size(), 0);
}

TEST_F(NavidromeLiveTest, GetAlbumMapsToTracks) {
    QJsonArray songs = firstSongs();
    ASSERT_GT(songs.size(), 0);
    QJsonObject song = songs.first().toObject();
    QString albumId = song.value("albumId").toString();
    ASSERT_FALSE(albumId.isEmpty());

    Result result =
        call([&](NavidromeClient::Callback cb) { client_.getAlbum(albumId, &context_, cb); });
    ASSERT_TRUE(result.called);
    ASSERT_TRUE(result.error.isEmpty());
    QJsonArray albumSongs = result.response.value("album").toObject().value("song").toArray();
    QList<PTrack> tracks = navidrome::tracksFromSongs(albumSongs);
    EXPECT_GT(tracks.size(), 0);
    EXPECT_EQ(tracks.size(), albumSongs.size());
    QString location = navidrome::locationForSong(song.value("id").toString());
    EXPECT_TRUE(std::any_of(tracks.begin(), tracks.end(),
                            [&](const PTrack &track) { return track->location == location; }));
}

TEST_F(NavidromeLiveTest, StreamRangeRequest) {
    QJsonArray songs = firstSongs();
    ASSERT_GT(songs.size(), 0);
    QString songId = songs.first().toObject().value("id").toString();

    QNetworkAccessManager network;
    QNetworkRequest request(client_.streamUrl(songId));
    request.setRawHeader("Range", "bytes=0-1023");
    request.setTransferTimeout(30000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::SameOriginRedirectPolicy);
    QNetworkReply *reply = network.get(request);
    bool done = false;
    int status = 0;
    QString contentType;
    auto inspect = [&]() {
        if (done)
            return;
        done = true;
        status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
        // Never download more than the headers, even if the server ignores Range.
        reply->abort();
    };
    QObject::connect(reply, &QNetworkReply::metaDataChanged, reply, inspect);
    QObject::connect(reply, &QNetworkReply::finished, reply, inspect);
    ASSERT_TRUE(QTest::qWaitFor([&] { return done; }, 30000));
    EXPECT_TRUE(status == 206 || status == 200) << "HTTP status " << status;
    EXPECT_TRUE(contentType.startsWith("audio/")) << qPrintable(contentType);
}

TEST_F(NavidromeLiveTest, LibrarySyncMatchesIndependentFetch) {
    auto scanStatus = [&]() {
        Result result = call([&](NavidromeClient::Callback cb) {
            client_.get("getScanStatus", {}, &context_, cb);
        });
        EXPECT_TRUE(result.called);
        EXPECT_TRUE(result.error.isEmpty());
        return result.response.value("scanStatus").toObject();
    };
    QJsonObject before = scanStatus();
    if (before.value("scanning").toBool())
        GTEST_SKIP() << "the server is scanning";

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    NavidromeLibrary library(client_, dir.filePath("navidrome_library"));
    QSignalSpy finished(&library, &NavidromeLibrary::syncFinished);
    QSignalSpy failed(&library, &NavidromeLibrary::syncFailed);
    ASSERT_TRUE(finished.isValid() && failed.isValid());
    library.sync();
    ASSERT_TRUE(QTest::qWaitFor([&] { return finished.count() + failed.count() > 0; }, 300000));
    ASSERT_EQ(failed.count(), 0);
    QList<PTrack> tracks = library.tracks();
    ASSERT_GT(tracks.size(), 0);
    EXPECT_EQ(finished.first().first().toInt(), tracks.size());
    QSet<QString> ids;
    for (const PTrack &track : tracks)
        ids.insert(navidrome::songIdFromLocation(track->location));
    EXPECT_EQ(ids.size(), tracks.size()) << "duplicate ids";
    EXPECT_FALSE(ids.contains(QString()));

    constexpr int kPageSize = 400;
    QSet<QString> fetched;
    for (int offset = 0;; offset += kPageSize) {
        ASSERT_LT(offset, 1000000);
        Result page = songPage(kPageSize, offset);
        ASSERT_TRUE(page.called);
        ASSERT_TRUE(page.error.isEmpty());
        QJsonArray songs = page.response.value("searchResult3").toObject().value("song").toArray();
        for (const QJsonValue &value : songs) {
            QJsonObject song = value.toObject();
            if (!song.value("isDir").toBool())
                fetched.insert(song.value("id").toString());
        }
        if (songs.size() < kPageSize)
            break;
    }

    QJsonObject after = scanStatus();
    if (after.value("scanning").toBool() || after.value("lastScan") != before.value("lastScan"))
        GTEST_SKIP() << "the server scanned during the test";
    EXPECT_TRUE(ids == fetched) << "synced " << ids.size() << ", fetched " << fetched.size();
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    QCoreApplication app(argc, argv);
    return RUN_ALL_TESTS();
}
