#include "library/track.h"
#include "library/track.pb.h"
#include "navidrome/navidromeclient.h"
#include "navidrome/navidromeconfig.h"
#include "navidrome/navidromelibrary.h"
#include "navidrome/navidromescrobbler.h"
#include "navidrome/navidrometrack.h"
#include "navidrome/tests/fakesubsonicserver.h"
#include "navidrome/tests/testsupport.h"
#include "util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <QCoreApplication>
#include <QDataStream>
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTest>
#include <QUrlQuery>
#include <algorithm>
#include <memory>

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace {

// True if `path` exists and neither its group nor others may access it.
bool isPrivate(const QString &path) {
    const QFile::Permissions others = QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup |
                                      QFile::ReadOther | QFile::WriteOther | QFile::ExeOther;
    return QFile::exists(path) && !(QFile::permissions(path) & others);
}

void makeReadableByOthers(const QString &path) {
    QFile::setPermissions(path, QFile::permissions(path) | QFile::ReadGroup | QFile::ReadOther);
}

QJsonObject parseObject(const char *json) { return QJsonDocument::fromJson(json).object(); }

QUrlQuery queryOf(const QUrl &url) { return QUrlQuery(url.query(QUrl::FullyEncoded)); }

QString queryValue(const QUrl &url, const QString &key) {
    return queryOf(url).queryItemValue(key, QUrl::FullyDecoded);
}

struct Result {
    bool called = false;
    QJsonObject response;
    QString error;
};

// Starts a request with a callback recording into the returned Result.
std::shared_ptr<Result> start(const std::function<void(NavidromeClient::Callback)> &request) {
    auto result = std::make_shared<Result>();
    request([result](const QJsonObject &response, const QString &error) {
        result->called = true;
        result->response = response;
        result->error = error;
    });
    return result;
}

Result call(const std::function<void(NavidromeClient::Callback)> &request) {
    auto result = start(request);
    static_cast<void>(QTest::qWaitFor([&] { return result->called; }, 5000));
    return *result;
}

} // namespace

TEST(RedactUrlCredentialsTest, RedactsAuthParameters) {
    EXPECT_EQ(redactUrlCredentials(
                  "http://h/rest/stream?u=alice&t=0123abcd&s=f00d&v=1.16.1&c=fubar&id=so-1"),
              "http://h/rest/stream?u=***&t=***&s=***&v=1.16.1&c=fubar&id=so-1");
    EXPECT_EQ(redactUrlCredentials("http://h/rest/ping?p=enc:736573616d65&apiKey=k-1#top"),
              "http://h/rest/ping?p=***&apiKey=***#top");
    EXPECT_EQ(redactUrlCredentials("http://h/rest/stream?id=1&u=a%20b&t="),
              "http://h/rest/stream?id=1&u=***&t=***");
}

TEST(RedactUrlCredentialsTest, RedactsInsideFreeText) {
    EXPECT_EQ(redactUrlCredentials("Failed to open \"http://h/rest/stream?id=1&t=abc&s=def\": 403, "
                                   "retrying http://h/rest/stream?u=x&id=2 now"),
              "Failed to open \"http://h/rest/stream?id=1&t=***&s=***\": 403, "
              "retrying http://h/rest/stream?u=***&id=2 now");
}

TEST(RedactUrlCredentialsTest, KeepsOtherText) {
    for (const char *text : {"no urls here", "http://h/x?U=Alice&T=x&S=y&APIKEY=z",
                             "http://h/x?subs=1&xu=2&ts=3&tt=4&pp=5&apiKeys=6", "u=alice t=secret",
                             "http://h/rest/stream?id=so-1"}) {
        EXPECT_EQ(redactUrlCredentials(text), text);
    }
}

TEST(NavidromeConfigTest, TokenVector) {
    EXPECT_EQ(NavidromeConfig::makeToken("sesame", "c19b2d"), "26719a1196d2a940705a59634eb18eab");
}

TEST(NavidromeConfigTest, NormalizeServerUrl) {
    auto normalize = &NavidromeConfig::normalizeServerUrl;
    EXPECT_EQ(normalize(""), "");
    EXPECT_EQ(normalize("   "), "");
    EXPECT_EQ(normalize("localhost:4533"), "http://localhost:4533");
    EXPECT_EQ(normalize(" music.example:4533/ "), "http://music.example:4533");
    EXPECT_EQ(normalize("http://music.example:4533/"), "http://music.example:4533");
    EXPECT_EQ(normalize("http://music.example:4533//"), "http://music.example:4533");
    EXPECT_EQ(normalize("https://music.example/rest"), "https://music.example");
    EXPECT_EQ(normalize("music.example:4533/rest/"), "http://music.example:4533");
    EXPECT_EQ(normalize("https://music.example/navidrome/"), "https://music.example/navidrome");
    EXPECT_EQ(normalize("https://music.example/navidrome/rest"), "https://music.example/navidrome");
    EXPECT_EQ(normalize("http://someone:secret@music.example/base/rest?u=x&t=y#top"),
              "http://music.example/base");
    EXPECT_EQ(normalize("HTTPS://Music.Example:8443/"), "https://music.example:8443");
    EXPECT_EQ(normalize("http://music.example/restful"), "http://music.example/restful");
    EXPECT_EQ(normalize("http://music.example/rest/rest"), "http://music.example");
    EXPECT_EQ(normalize("music.example/base/rest//rest/"), "http://music.example/base");
    EXPECT_EQ(normalize("http://rest"), "http://rest");
    EXPECT_EQ(normalize("https://rest/rest"), "https://rest");
    EXPECT_EQ(normalize("http://music.example/my%20music/rest"), "http://music.example/my%20music");
    EXPECT_EQ(normalize("http://music.example/my music/"), "http://music.example/my%20music");
}

TEST(NavidromeConfigTest, NormalizeServerUrlIsIdempotent) {
    auto normalize = &NavidromeConfig::normalizeServerUrl;
    QStringList urls{"", "   ", "localhost:4533", " music.example:4533/ ",
                     "http://music.example:4533//", "https://music.example/rest",
                     "music.example:4533/rest/", "https://music.example/navidrome/",
                     "http://someone:secret@music.example/base/rest?u=x&t=y#top",
                     "HTTPS://Music.Example:8443/", "http://music.example/restful",
                     "http://music.example/rest/rest", "music.example/base/rest//rest/",
                     "http://rest", "https://rest/rest/rest",
                     "http://music.example/my%20music/rest", "http://music.example/my music/",
                     "http://music.example:port/", "http://exa mple.com", "http://",
                     "ftp://music.example/rest", "file:///music", "http://user:secret@",
                     "http://user:secret@bad host:4533", "user:secret@bad host",
                     "http://us er:secret@music.example/rest/",
                     "http://user:se@cret@music.example/rest/"};
    for (const QString &url : urls) {
        QString once = normalize(url);
        EXPECT_EQ(normalize(once), once) << qPrintable(url);
    }
}

TEST(NavidromeConfigTest, NormalizeServerUrlKeepsUnparsableInput) {
    auto normalize = &NavidromeConfig::normalizeServerUrl;
    EXPECT_EQ(normalize("  http://music.example:port/  "), "http://music.example:port/");
    EXPECT_EQ(normalize(" exa mple.com:4533 "), "exa mple.com:4533");
    EXPECT_EQ(normalize("http://"), "http://");
    EXPECT_EQ(normalize("file:///music"), "file:///music");
    // Except for the user info.
    EXPECT_EQ(normalize("http://user:secret@"), "http://");
    EXPECT_EQ(normalize("http://user:secret@bad host:4533"), "http://bad host:4533");
    EXPECT_EQ(normalize(" user:secret@bad host "), "bad host");
    EXPECT_EQ(normalize("user:se@cret@bad host/rest"), "bad host/rest");
    EXPECT_EQ(normalize("http://user:secret@bad host/a@b"), "http://bad host/a@b");
    // QUrl splits the user info at the first '@'.
    EXPECT_EQ(normalize("http://user:se@cret@music.example/rest/"), "http://music.example");
    for (const char *url : {"http://us er:secret@music.example/rest/", "us er:secret@music.example",
                            "http://user:secret@music.example:port/"}) {
        EXPECT_FALSE(normalize(url).contains("secret")) << url;
    }
}

TEST(NavidromeConfigTest, IsValidServerUrl) {
    for (const char *url : {"http://music.example:4533", "https://music.example/navidrome",
                            "http://127.0.0.1:4533", "http://[::1]:4533", "http://rest"}) {
        EXPECT_TRUE(NavidromeConfig::isValidServerUrl(url)) << url;
    }
    for (const char *url : {"", "localhost:4533", "music.example", "ftp://music.example",
                            "http://", "file:///music", "http://music.example:port",
                            "exa mple.com:4533"}) {
        EXPECT_FALSE(NavidromeConfig::isValidServerUrl(url)) << url;
    }
}

TEST(NavidromeConfigTest, SaveLoadRoundTrip) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QString fileName = dir.filePath("fubar.conf");
    NavidromeConfig config = testConfig("https://music.example/navidrome");
    config.scrobble = false;
    {
        QSettings settings(fileName, QSettings::IniFormat);
        config.save(settings);
    }
    QSettings settings(fileName, QSettings::IniFormat);
    NavidromeConfig loaded = NavidromeConfig::load(settings);
    EXPECT_EQ(loaded.serverUrl, config.serverUrl);
    EXPECT_EQ(loaded.username, config.username);
    EXPECT_EQ(loaded.salt, config.salt);
    EXPECT_EQ(loaded.token, config.token);
    EXPECT_FALSE(loaded.scrobble);
    EXPECT_THAT(settings.allKeys(),
                UnorderedElementsAre("navidrome/serverUrl", "navidrome/username", "navidrome/salt",
                                     "navidrome/token", "navidrome/scrobble"));
}

TEST(NavidromeConfigTest, SavedConfigIsPrivate) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QString fileName = dir.filePath("fubar.conf");
    {
        QSettings settings(fileName, QSettings::IniFormat);
        testConfig().save(settings);
        EXPECT_TRUE(isPrivate(fileName));
    }
    // A settings file written before
    makeReadableByOthers(fileName);
    ASSERT_FALSE(isPrivate(fileName));
    {
        QSettings settings(fileName, QSettings::IniFormat);
        testConfig().save(settings);
    }
    EXPECT_TRUE(isPrivate(fileName));
    // Other writers keep it private
    {
        QSettings settings(fileName, QSettings::IniFormat);
        settings.setValue("navidrome/pendingScrobbles", QStringList{"so-1 1700000000000"});
    }
    EXPECT_TRUE(isPrivate(fileName));
    QSettings settings(fileName, QSettings::IniFormat);
    EXPECT_TRUE(settings.contains("navidrome/pendingScrobbles"));
}

TEST(NavidromeConfigTest, LoadDefaults) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QSettings settings(dir.filePath("empty.conf"), QSettings::IniFormat);
    NavidromeConfig config = NavidromeConfig::load(settings);
    EXPECT_FALSE(config.isComplete());
    EXPECT_TRUE(config.serverUrl.isEmpty());
    EXPECT_TRUE(config.scrobble);
}

TEST(NavidromeConfigTest, SetPassword) {
    NavidromeConfig config = testConfig();
    config.setPassword("sesame");
    EXPECT_TRUE(QRegularExpression("^[0-9a-f]{32}$").match(config.salt).hasMatch());
    EXPECT_EQ(config.token, NavidromeConfig::makeToken("sesame", config.salt));
    EXPECT_TRUE(QRegularExpression("^[0-9a-f]{32}$").match(config.token).hasMatch());

    NavidromeConfig other = config;
    other.setPassword("sesame");
    EXPECT_NE(other.salt, config.salt);
    EXPECT_NE(other.token, config.token);
}

TEST(NavidromeConfigTest, Comparisons) {
    NavidromeConfig config = testConfig();
    EXPECT_TRUE(config.isComplete());
    for (QString NavidromeConfig::*field : {&NavidromeConfig::serverUrl, &NavidromeConfig::username,
                                            &NavidromeConfig::salt, &NavidromeConfig::token}) {
        NavidromeConfig incomplete = config;
        incomplete.*field = "";
        EXPECT_FALSE(incomplete.isComplete());
        EXPECT_FALSE(incomplete.sameConnection(config));
    }

    NavidromeConfig other = config;
    other.scrobble = false;
    EXPECT_TRUE(other.sameConnection(config));
    other.token = NavidromeConfig::makeToken("other", kSalt);
    EXPECT_FALSE(other.sameConnection(config));
    EXPECT_TRUE(other.sameAccount(config));
    other.username = "someone";
    EXPECT_FALSE(other.sameAccount(config));
    other = config;
    other.serverUrl = "http://other.example:4533";
    EXPECT_FALSE(other.sameAccount(config));
}

TEST(NavidromeUrlTest, ApiUrlHasAuthAndParams) {
    QUrl url = NavidromeClient::apiUrl(testConfig(), "getAlbum", {{"id", "al-1"}});
    EXPECT_EQ(url.scheme(), "http");
    EXPECT_EQ(url.host(), "music.example");
    EXPECT_EQ(url.port(), 4533);
    EXPECT_EQ(url.path(), "/rest/getAlbum");
    QUrlQuery query = queryOf(url);
    EXPECT_EQ(query.queryItemValue("u"), "listener");
    EXPECT_EQ(query.queryItemValue("t"), kToken);
    EXPECT_EQ(query.queryItemValue("s"), kSalt);
    EXPECT_EQ(query.queryItemValue("v"), "1.16.1");
    EXPECT_EQ(query.queryItemValue("c"), "fubar");
    EXPECT_EQ(query.queryItemValue("f"), "json");
    EXPECT_EQ(query.queryItemValue("id"), "al-1");
    EXPECT_FALSE(query.hasQueryItem("p"));
    EXPECT_FALSE(url.toString(QUrl::FullyEncoded).contains("sesame"));
}

TEST(NavidromeUrlTest, BasePathKept) {
    QUrl url = NavidromeClient::apiUrl(testConfig("https://music.example/navidrome"), "ping", {});
    EXPECT_EQ(url.scheme(), "https");
    EXPECT_EQ(url.path(), "/navidrome/rest/ping");
}

TEST(NavidromeUrlTest, StreamUrl) {
    NavidromeClient client;
    client.setConfig(testConfig());
    QUrl url = client.streamUrl("so-1");
    EXPECT_EQ(url.path(), "/rest/stream");
    QUrlQuery query = queryOf(url);
    EXPECT_EQ(query.queryItemValue("id"), "so-1");
    EXPECT_EQ(query.queryItemValue("u"), "listener");
    EXPECT_EQ(query.queryItemValue("t"), kToken);
    EXPECT_EQ(query.queryItemValue("s"), kSalt);
    for (const char *key : {"f", "format", "maxBitRate", "p"})
        EXPECT_FALSE(query.hasQueryItem(key)) << key;
}

TEST(NavidromeUrlTest, SpecialCharactersSurviveFullyEncoded) {
    NavidromeConfig config = testConfig();
    config.username = "a b@c&d+e%f";
    QUrl url = NavidromeClient::apiUrl(config, "search3", {{"query", "x & y + z % @ #=?"}});
    QString encoded = url.toString(QUrl::FullyEncoded);
    EXPECT_TRUE(encoded.contains("u=a%20b%40c%26d%2Be%25f")) << qPrintable(encoded);
    EXPECT_FALSE(encoded.contains('+')) << qPrintable(encoded);
    QUrl reparsed(encoded, QUrl::StrictMode);
    ASSERT_TRUE(reparsed.isValid());
    EXPECT_EQ(queryValue(reparsed, "u"), "a b@c&d+e%f");
    EXPECT_EQ(queryValue(reparsed, "query"), "x & y + z % @ #=?");
    EXPECT_EQ(reparsed.fragment(), "");
}

constexpr const char *kFlacSong = R"json({
    "id": "so-flac-1", "parent": "al-1", "isDir": false, "title": "First Light",
    "album": "Example Album", "albumId": "al-1", "artist": "Example Artist", "track": 3,
    "year": 2019, "genre": "Ambient", "comment": "Remastered", "coverArt": "mf-so-flac-1",
    "size": 31457280, "contentType": "audio/flac", "suffix": "flac", "duration": 245,
    "bitRate": 1024, "samplingRate": 44100, "channelCount": 2, "discNumber": 1,
    "path": "Example Artist/Example Album/01-03 - First Light.flac",
    "displayArtist": "Example Artist feat. Guest", "displayAlbumArtist": "Example Artist",
    "albumArtists": [{"id": "ar-1", "name": "Example Artist"}],
    "replayGain": {"trackGain": -7.25, "albumGain": -6.5, "trackPeak": 0.988, "albumPeak": 1},
    "playCount": 4, "userRating": 5
})json";

constexpr const char *kMp3Song = R"json({
    "id": "so-mp3-2", "isDir": false, "title": "Second Song", "album": "Example Compilation",
    "artist": "Guest Artist", "track": 12, "year": 0, "genre": "", "suffix": "mp3",
    "contentType": "audio/mpeg", "duration": 181, "bitRate": 320, "samplingRate": 48000,
    "channelCount": 2, "discNumber": 0,
    "path": "Various Artists/Example Compilation/12 - Second Song.mp3",
    "displayAlbumArtist": "Various Artists",
    "albumArtists": [{"id": "ar-va", "name": "Various Artists"}], "replayGain": {}
})json";

constexpr const char *kSongWithoutPath = R"json({
    "id": "so-3", "title": "Third Song", "album": "Split Album",
    "displayArtist": "Band A & Band B",
    "albumArtists": [{"id": "ar-a", "name": "Band A"}, {"id": "ar-b", "name": "Band B"}],
    "track": 7, "discNumber": 2, "suffix": "opus", "duration": 200,
    "replayGain": {"trackGain": 0}
})json";

TEST(NavidromeTrackTest, Locations) {
    EXPECT_EQ(navidrome::kLocationPrefix, "navidrome:");
    EXPECT_EQ(navidrome::locationForSong("so-1"), "navidrome:so-1");
    EXPECT_TRUE(navidrome::isNavidromeLocation("navidrome:so-1"));
    EXPECT_FALSE(navidrome::isNavidromeLocation("/music/navidrome:so-1.flac"));
    EXPECT_EQ(navidrome::songIdFromLocation("navidrome:so-1"), "so-1");
    EXPECT_EQ(navidrome::songIdFromLocation("/music/a.flac"), "");
    EXPECT_EQ(navidrome::songIdFromLocation("http://music.example/a.mp3"), "");
}

TEST(NavidromeTrackTest, LocalTrackPathIsLocation) {
    Track track;
    track.location = "/music/a.flac";
    EXPECT_EQ(track.path(), "/music/a.flac");
    EXPECT_EQ(track.mtime, 0u);
}

TEST(NavidromeTrackTest, FlacWithReplayGainAndAlbumArtist) {
    PTrack track = navidrome::trackFromSong(parseObject(kFlacSong));
    ASSERT_TRUE(track);
    EXPECT_EQ(track->location, "navidrome:so-flac-1");
    EXPECT_EQ(track->path(), "Example Artist/Example Album/01-03 - First Light.flac");
    EXPECT_EQ(track->mtime, 0u);
    EXPECT_EQ(track->audioproperties.length, 245);
    EXPECT_EQ(track->audioproperties.bitrate, 1024);
    EXPECT_EQ(track->audioproperties.samplerate, 44100);
    EXPECT_EQ(track->audioproperties.channels, 2);
    EXPECT_EQ(track->audioFormat(), "FLAC");
    EXPECT_THAT(track->metadata.toStdMap(),
                UnorderedElementsAre(
                    Pair("title", "First Light"), Pair("album", "Example Album"),
                    Pair("artist", "Example Artist"), Pair("album artist", "Example Artist"),
                    Pair("genre", "Ambient"), Pair("comment", "Remastered"), Pair("year", "2019"),
                    Pair("track", "3"), Pair("disc", "1"), Pair("audio-format", "FLAC"),
                    Pair("REPLAYGAIN_TRACK_GAIN", "-7.25 dB"),
                    Pair("REPLAYGAIN_ALBUM_GAIN", "-6.50 dB"),
                    Pair("REPLAYGAIN_TRACK_PEAK", "0.988000"),
                    Pair("REPLAYGAIN_ALBUM_PEAK", "1.000000"),
                    Pair("_path", "Example Artist/Example Album/01-03 - First Light.flac")));
}

TEST(NavidromeTrackTest, Mp3WithEmptyReplayGainAndVariousArtists) {
    PTrack track = navidrome::trackFromSong(parseObject(kMp3Song));
    ASSERT_TRUE(track);
    EXPECT_EQ(track->location, "navidrome:so-mp3-2");
    EXPECT_EQ(track->path(), "Various Artists/Example Compilation/12 - Second Song.mp3");
    EXPECT_EQ(track->audioproperties.length, 181);
    EXPECT_EQ(track->audioproperties.bitrate, 320);
    EXPECT_EQ(track->audioproperties.samplerate, 48000);
    EXPECT_EQ(track->audioproperties.channels, 2);
    EXPECT_THAT(track->metadata.toStdMap(),
                UnorderedElementsAre(
                    Pair("title", "Second Song"), Pair("album", "Example Compilation"),
                    Pair("artist", "Guest Artist"), Pair("album artist", "Various Artists"),
                    Pair("track", "12"), Pair("audio-format", "MP3"),
                    Pair("_path", "Various Artists/Example Compilation/12 - Second Song.mp3")));
}

TEST(NavidromeTrackTest, SongWithoutPath) {
    PTrack track = navidrome::trackFromSong(parseObject(kSongWithoutPath));
    ASSERT_TRUE(track);
    EXPECT_EQ(track->location, "navidrome:so-3");
    EXPECT_EQ(track->path(), "Band A, Band B/Split Album/2-07 - Third Song.opus");
    EXPECT_EQ(track->audioproperties.length, 200);
    EXPECT_EQ(track->audioproperties.bitrate, 0);
    EXPECT_THAT(track->metadata.toStdMap(),
                UnorderedElementsAre(
                    Pair("title", "Third Song"), Pair("album", "Split Album"),
                    Pair("artist", "Band A & Band B"), Pair("album artist", "Band A, Band B"),
                    Pair("track", "7"), Pair("disc", "2"), Pair("audio-format", "OPUS"),
                    Pair("REPLAYGAIN_TRACK_GAIN", "0.00 dB"),
                    Pair("_path", "Band A, Band B/Split Album/2-07 - Third Song.opus")));
}

TEST(NavidromeTrackTest, SynthesizedPathSkipsEmptyParts) {
    PTrack minimal = navidrome::trackFromSong(parseObject(R"({"id": "so-4"})"));
    ASSERT_TRUE(minimal);
    EXPECT_THAT(minimal->metadata.toStdMap(), UnorderedElementsAre(Pair("_path", "so-4")));

    PTrack noAlbum = navidrome::trackFromSong(
        parseObject(R"({"id": "so-5", "title": "Loose", "artist": "Solo", "track": 4})"));
    ASSERT_TRUE(noAlbum);
    EXPECT_EQ(noAlbum->path(), "Solo/04 - Loose");
}

TEST(NavidromeTrackTest, DirectoriesAndSongsWithoutIdAreSkipped) {
    EXPECT_FALSE(navidrome::trackFromSong(
        parseObject(R"({"id": "al-1", "isDir": true, "title": "Example Album"})")));
    EXPECT_FALSE(navidrome::trackFromSong(parseObject(R"({"title": "No id"})")));

    QJsonArray songs{parseObject(kFlacSong), parseObject(R"({"id": "al-1", "isDir": true})"),
                     parseObject(kMp3Song), parseObject(R"({"title": "No id"})"),
                     parseObject(kSongWithoutPath)};
    QStringList locations;
    for (const PTrack &track : navidrome::tracksFromSongs(songs))
        locations << track->location;
    EXPECT_EQ(locations,
              QStringList({"navidrome:so-flac-1", "navidrome:so-mp3-2", "navidrome:so-3"}));
}

class NavidromeClientTest : public testing::Test {
protected:
    void SetUp() override {
        ASSERT_TRUE(server_.listen());
        client_.setConfig(testConfig(server_.url()));
    }

    void TearDown() override { server_.close(); }

    Result ping() {
        return call([&](NavidromeClient::Callback cb) { client_.get("ping", {}, &context_, cb); });
    }

    const FakeSubsonicServer::Request &lastRequest() {
        static FakeSubsonicServer::Request none;
        return server_.requests().isEmpty() ? none : server_.requests().last();
    }

    FakeSubsonicServer server_;
    NavidromeClient client_;
    QObject context_;
};

TEST_F(NavidromeClientTest, Ok) {
    server_.setResponse("ping", FakeSubsonicServer::ok());
    Result result = ping();
    ASSERT_TRUE(result.called);
    EXPECT_EQ(result.error, "");
    EXPECT_EQ(result.response.value("status").toString(), "ok");

    ASSERT_EQ(server_.requests().size(), 1);
    const FakeSubsonicServer::Request &request = lastRequest();
    EXPECT_EQ(request.path, "/rest/ping");
    EXPECT_EQ(request.method, "ping");
    EXPECT_EQ(request.value("u"), "listener");
    EXPECT_EQ(request.value("t"), kToken);
    EXPECT_EQ(request.value("s"), kSalt);
    EXPECT_EQ(request.value("v"), "1.16.1");
    EXPECT_EQ(request.value("c"), "fubar");
    EXPECT_EQ(request.value("f"), "json");
    EXPECT_FALSE(request.has("p"));
    EXPECT_EQ(request.headers.value("user-agent"), "fubar");
}

TEST_F(NavidromeClientTest, FailedResponse) {
    server_.setResponse("ping", FakeSubsonicServer::failed(40, "Wrong username or password"));
    Result result = ping();
    ASSERT_TRUE(result.called);
    EXPECT_EQ(result.error, "Wrong username or password");
    EXPECT_EQ(result.response.value("status").toString(), "failed");
    EXPECT_EQ(result.response.value("error").toObject().value("code").toInt(), 40);
}

TEST_F(NavidromeClientTest, FailedResponseWithoutMessage) {
    server_.setResponse("ping", FakeSubsonicServer::failed(70, ""));
    Result result = ping();
    ASSERT_TRUE(result.called);
    EXPECT_EQ(result.error, "Error 70");
}

TEST_F(NavidromeClientTest, HttpErrorHasNoUrl) {
    server_.setResponse("ping", FakeSubsonicServer::http(500, "boom"));
    MessageCapture log;
    Result result = ping();
    ASSERT_TRUE(result.called);
    EXPECT_TRUE(result.error.contains("500")) << qPrintable(result.error);
    QString logs = log.text();
    EXPECT_TRUE(logs.contains("[Navidrome]"));
    for (const char *secret : {kToken, kSalt, "rest/", "127.0.0.1", "listener"}) {
        EXPECT_FALSE(result.error.contains(secret)) << qPrintable(result.error);
        EXPECT_FALSE(logs.contains(secret)) << secret;
    }
}

TEST_F(NavidromeClientTest, InvalidResponse) {
    FakeSubsonicServer::Response notJson;
    notJson.body = "<html>not json</html>";
    server_.setResponse("ping", notJson);
    EXPECT_EQ(ping().error, "Invalid response from server");

    FakeSubsonicServer::Response notSubsonic;
    notSubsonic.body = R"({"status": "ok"})";
    server_.setResponse("ping", notSubsonic);
    EXPECT_EQ(ping().error, "Invalid response from server");
}

TEST_F(NavidromeClientTest, ConnectionRefusedHasNoUrl) {
    QTcpServer unused;
    ASSERT_TRUE(unused.listen(QHostAddress::LocalHost));
    QString url = QString("http://127.0.0.1:%1").arg(unused.serverPort());
    unused.close();
    client_.setConfig(testConfig(url));

    MessageCapture log;
    Result result = ping();
    ASSERT_TRUE(result.called);
    EXPECT_EQ(result.error, "Connection refused");
    QString logs = log.text();
    EXPECT_TRUE(logs.contains("[Navidrome]"));
    for (const char *secret : {kToken, kSalt, "rest/", "127.0.0.1", "listener"})
        EXPECT_FALSE(logs.contains(secret)) << secret;
}

TEST_F(NavidromeClientTest, CrossOriginRedirectIsNotFollowed) {
    FakeSubsonicServer other;
    ASSERT_TRUE(other.listen());
    other.setResponse("ping", FakeSubsonicServer::ok());
    server_.setResponse("ping", FakeSubsonicServer::redirect(other.url() + "/rest/ping"));

    Result result = ping();
    ASSERT_TRUE(result.called);
    EXPECT_TRUE(result.error.contains("redirected")) << qPrintable(result.error);
    for (const char *secret : {kToken, kSalt, "rest/", "127.0.0.1"})
        EXPECT_FALSE(result.error.contains(secret)) << qPrintable(result.error);
    QTest::qWait(50);
    EXPECT_EQ(server_.requests().size(), 1);
    EXPECT_TRUE(other.requests().isEmpty());
    other.close();
}

TEST_F(NavidromeClientTest, DestroyedContextAbortsRequest) {
    server_.setResponse("ping", FakeSubsonicServer::held());
    auto context = std::make_unique<QObject>();
    auto result =
        start([&](NavidromeClient::Callback cb) { client_.get("ping", {}, context.get(), cb); });
    ASSERT_TRUE(QTest::qWaitFor([&] { return server_.requests().size() == 1; }, 5000));
    QList<QNetworkReply *> replies = client_.findChildren<QNetworkReply *>();
    ASSERT_EQ(replies.size(), 1);
    QPointer<QNetworkReply> reply = replies.first();

    context.reset();
    EXPECT_TRUE(QTest::qWaitFor([&] { return reply.isNull(); }, 5000));
    EXPECT_TRUE(QTest::qWaitFor([&] { return server_.openConnections() == 0; }, 5000));
    QTest::qWait(50);
    EXPECT_FALSE(result->called);
}

TEST_F(NavidromeClientTest, NullContextMeansClient) {
    server_.setResponse("ping", FakeSubsonicServer::ok());
    Result result =
        call([&](NavidromeClient::Callback cb) { client_.get("ping", {}, nullptr, cb); });
    ASSERT_TRUE(result.called);
    EXPECT_EQ(result.error, "");
}

TEST_F(NavidromeClientTest, NotConfiguredIsAsyncError) {
    NavidromeClient client;
    auto result =
        start([&](NavidromeClient::Callback cb) { client.get("ping", {}, &context_, cb); });
    EXPECT_FALSE(result->called);
    ASSERT_TRUE(QTest::qWaitFor([&] { return result->called; }, 5000));
    EXPECT_EQ(result->error, "Navidrome is not configured");
    EXPECT_TRUE(server_.requests().isEmpty());

    auto context = std::make_unique<QObject>();
    auto dropped =
        start([&](NavidromeClient::Callback cb) { client.get("ping", {}, context.get(), cb); });
    context.reset();
    QTest::qWait(50);
    EXPECT_FALSE(dropped->called);
}

TEST_F(NavidromeClientTest, ExplicitConfigOverridesClientConfig) {
    server_.setResponse("ping", FakeSubsonicServer::ok());
    NavidromeClient client;
    NavidromeConfig config = testConfig(server_.url() + "/base");
    Result result = call([&](NavidromeClient::Callback cb) {
        client.get(config, "ping", {}, &context_, cb);
    });
    ASSERT_TRUE(result.called);
    EXPECT_EQ(lastRequest().path, "/base/rest/ping");
}

TEST_F(NavidromeClientTest, SpecialCharactersReachServer) {
    server_.setHandler(
        [](const FakeSubsonicServer::Request &) { return FakeSubsonicServer::ok(); });
    NavidromeConfig config = testConfig(server_.url());
    config.username = "a b@c&d+e%f";
    client_.setConfig(config);
    NavidromeClient::Params params{{"query", "x & y + z % @ #=?"},
                                   {"artistCount", "0"},
                                   {"albumCount", "0"},
                                   {"songCount", "500"},
                                   {"songOffset", "1000"}};
    Result result =
        call([&](NavidromeClient::Callback cb) { client_.get("search3", params, &context_, cb); });
    ASSERT_TRUE(result.called);
    EXPECT_EQ(result.error, "");
    const FakeSubsonicServer::Request &request = lastRequest();
    EXPECT_EQ(request.method, "search3");
    EXPECT_EQ(request.value("u"), "a b@c&d+e%f");
    EXPECT_EQ(request.value("query"), "x & y + z % @ #=?");
    EXPECT_EQ(request.value("artistCount"), "0");
    EXPECT_EQ(request.value("albumCount"), "0");
    EXPECT_EQ(request.value("songCount"), "500");
    EXPECT_EQ(request.value("songOffset"), "1000");
}

TEST_F(NavidromeClientTest, TypedHelpersSendParams) {
    server_.setHandler(
        [](const FakeSubsonicServer::Request &) { return FakeSubsonicServer::ok(); });
    using Request = std::function<void(NavidromeClient::Callback)>;
    struct Expected {
        Request request;
        QString method;
        QList<QPair<QString, QString>> params;
    };
    QList<Expected> cases{
        {[&](auto cb) { client_.getAlbum("al-1", &context_, cb); }, "getAlbum", {{"id", "al-1"}}},
        {[&](auto cb) { client_.scrobble("so-1", false, 1234, &context_, cb); },
         "scrobble",
         {{"id", "so-1"}, {"submission", "false"}}},
        {[&](auto cb) { client_.scrobble("so-1", true, 1700000000123, &context_, cb); },
         "scrobble",
         {{"id", "so-1"}, {"submission", "true"}, {"time", "1700000000123"}}},
    };
    for (const Expected &expected : cases) {
        Result result = call(expected.request);
        ASSERT_TRUE(result.called) << qPrintable(expected.method);
        EXPECT_EQ(result.error, "") << qPrintable(expected.method);
        const FakeSubsonicServer::Request &request = lastRequest();
        EXPECT_EQ(request.method, expected.method);
        // The 6 auth/format parameters come first.
        EXPECT_EQ(request.query.mid(6), expected.params) << qPrintable(expected.method);
    }
}

TEST_F(NavidromeClientTest, SetConfigSignalsConnectionChangesOnly) {
    NavidromeClient client;
    QSignalSpy spy(&client, &NavidromeClient::connectionChanged);
    NavidromeConfig config = testConfig();
    client.setConfig(config);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_TRUE(client.isConfigured());
    client.setConfig(config);
    EXPECT_EQ(spy.count(), 1);
    config.scrobble = false;
    client.setConfig(config);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_FALSE(client.config().scrobble);
    config.setPassword("other");
    client.setConfig(config);
    EXPECT_EQ(spy.count(), 2);
    client.setConfig(NavidromeConfig());
    EXPECT_EQ(spy.count(), 3);
    EXPECT_FALSE(client.isConfigured());
}

TEST_F(NavidromeClientTest, SetConfigSignalsAccountChangesFirst) {
    NavidromeClient client;
    QStringList emitted;
    QObject::connect(&client, &NavidromeClient::accountChanged, [&]() { emitted << "account"; });
    QObject::connect(&client, &NavidromeClient::connectionChanged,
                     [&]() { emitted << "connection"; });
    NavidromeConfig config = testConfig();
    client.setConfig(config);
    EXPECT_EQ(emitted, (QStringList{"account", "connection"}));
    config.scrobble = false;
    config.setPassword("other");
    client.setConfig(config);
    EXPECT_EQ(emitted, (QStringList{"account", "connection", "connection"}));
    config.username = "someone";
    client.setConfig(config);
    EXPECT_EQ(emitted.mid(3), (QStringList{"account", "connection"}));
    config.serverUrl = "http://other.example:4533";
    client.setConfig(config);
    EXPECT_EQ(emitted.mid(5), (QStringList{"account", "connection"}));
    client.setConfig(NavidromeConfig());
    EXPECT_EQ(emitted.mid(7), (QStringList{"account", "connection"}));
}

namespace {

QJsonObject song(const QString &id, const QString &title = QString()) {
    return QJsonObject{{"id", id},
                       {"title", title.isEmpty() ? "Song " + id : title},
                       {"album", "Example Album"},
                       {"artist", "Example Artist"},
                       {"suffix", "flac"},
                       {"duration", 200},
                       {"path", "Example Artist/Example Album/" + id + ".flac"}};
}

// Reads a library cache file: its header fields (magic, version, server URL, username, last scan,
// scan count) into `header`, then the serialized proto::Library after them.
QByteArray readCacheFile(const QString &path, QStringList *header) {
    QFile file(path);
    EXPECT_TRUE(file.open(QIODevice::ReadOnly));
    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_5_15);
    QByteArray magic(int(sizeof("fubar-navidrome")) - 1, Qt::Uninitialized);
    stream.readRawData(magic.data(), magic.size());
    qint32 version = 0;
    qint32 scanCount = 0;
    QString serverUrl, username, lastScan;
    quint32 size = 0;
    stream >> version >> serverUrl >> username >> lastScan >> scanCount >> size;
    *header = QStringList{QString::fromLatin1(magic), QString::number(version), serverUrl,
                          username, lastScan, QString::number(scanCount)};
    // A null QByteArray
    EXPECT_NE(size, 0xffffffffu);
    QByteArray library(size == 0xffffffffu ? 0 : int(size), Qt::Uninitialized);
    EXPECT_EQ(stream.readRawData(library.data(), library.size()), library.size());
    EXPECT_TRUE(stream.atEnd());
    return library;
}

struct LibrarySpies {
    explicit LibrarySpies(NavidromeLibrary *library)
        : updated(library, &NavidromeLibrary::tracksUpdated),
          reset(library, &NavidromeLibrary::tracksReset),
          started(library, &NavidromeLibrary::syncStarted),
          finished(library, &NavidromeLibrary::syncFinished),
          failed(library, &NavidromeLibrary::syncFailed) {}

    bool isValid() const {
        return updated.isValid() && reset.isValid() && started.isValid() && finished.isValid() &&
               failed.isValid();
    }
    QList<PTrack> updatedTracks(int i = 0) const {
        return updated.at(i).at(0).value<QList<PTrack>>();
    }

    QSignalSpy updated;
    QSignalSpy reset;
    QSignalSpy started;
    QSignalSpy finished;
    QSignalSpy failed;
};

} // namespace

class NavidromeLibraryTest : public testing::Test {
protected:
    void SetUp() override {
        ASSERT_TRUE(server_.listen());
        ASSERT_TRUE(dir_.isValid());
        cacheFile_ = dir_.filePath("cache/navidrome_library");
        client_.setConfig(testConfig(server_.url()));
        for (int i = 1; i <= 5; ++i)
            songs_.append(song(QString("so-%1").arg(i)));
        server_.setHandler([this](const FakeSubsonicServer::Request &request) {
            return respond(request);
        });
    }

    void TearDown() override { server_.close(); }

    FakeSubsonicServer::Response respond(const FakeSubsonicServer::Request &request) {
        if (onRequest_)
            onRequest_(request);
        if (methodResponses_.contains(request.method))
            return methodResponses_.value(request.method);
        if (request.method == "getScanStatus") {
            QJsonObject status{{"scanning", scanning_},
                               {"count", scanCount_},
                               {"folderCount", 1},
                               {"lastScan", lastScan_}};
            return FakeSubsonicServer::ok({{"scanStatus", status}});
        }
        if (request.method != "search3")
            return FakeSubsonicServer::http(404);
        int offset = request.value("songOffset").toInt();
        if (pageResponses_.contains(offset))
            return pageResponses_.value(offset);
        int count = request.value("songCount").toInt();
        QJsonArray page;
        for (int i = offset; i < qMin(offset + count, songs_.size()); ++i)
            page.append(songs_[i]);
        return FakeSubsonicServer::ok({{"searchResult3", QJsonObject{{"song", page}}}});
    }

    int requestCount(const QString &method) const {
        const QList<FakeSubsonicServer::Request> &requests = server_.requests();
        return std::count_if(requests.begin(), requests.end(), [&](const auto &request) {
            return request.method == method;
        });
    }

    QList<int> pageOffsets() const {
        QList<int> offsets;
        for (const FakeSubsonicServer::Request &request : server_.requests()) {
            if (request.method == "search3")
                offsets << request.value("songOffset").toInt();
        }
        return offsets;
    }

    // True when no request is in flight and `library` is not syncing.
    bool idle(NavidromeLibrary &library) {
        return client_.findChildren<QNetworkReply *>().isEmpty() && !library.isSyncing();
    }

    bool syncAndWait(NavidromeLibrary &library) {
        QSignalSpy finished(&library, &NavidromeLibrary::syncFinished);
        QSignalSpy failed(&library, &NavidromeLibrary::syncFailed);
        library.sync();
        return QTest::qWaitFor([&] { return finished.count() + failed.count() > 0; }, 5000);
    }

    // Runs checkForUpdates() to the end; true if it started a sync.
    bool checkSyncs(NavidromeLibrary &library) {
        QSignalSpy started(&library, &NavidromeLibrary::syncStarted);
        int requests = server_.requests().size();
        library.checkForUpdates();
        EXPECT_TRUE(QTest::qWaitFor(
            [&] { return server_.requests().size() > requests && idle(library); }, 5000));
        return !started.isEmpty();
    }

    FakeSubsonicServer server_;
    NavidromeClient client_;
    QTemporaryDir dir_;
    QString cacheFile_;
    QJsonArray songs_;
    QHash<QString, FakeSubsonicServer::Response> methodResponses_;
    QHash<int, FakeSubsonicServer::Response> pageResponses_;
    std::function<void(const FakeSubsonicServer::Request &)> onRequest_;
    bool scanning_ = false;
    int scanCount_ = 5;
    QString lastScan_ = "2026-01-01T10:00:00Z";
};

TEST_F(NavidromeLibraryTest, SyncFetchesAllPages) {
    NavidromeLibrary library(client_, cacheFile_);
    library.setPageSize(2);
    LibrarySpies spies(&library);
    ASSERT_TRUE(spies.isValid());
    EXPECT_TRUE(library.tracks().isEmpty());

    library.sync();
    EXPECT_TRUE(library.isSyncing());
    EXPECT_EQ(spies.started.count(), 1);
    ASSERT_TRUE(QTest::qWaitFor([&] { return spies.finished.count() == 1; }, 5000));
    EXPECT_FALSE(library.isSyncing());
    EXPECT_EQ(spies.finished.at(0).at(0).toInt(), 5);
    EXPECT_TRUE(spies.failed.isEmpty());
    EXPECT_TRUE(spies.reset.isEmpty());

    QStringList expected = songLocations({"so-1", "so-2", "so-3", "so-4", "so-5"});
    ASSERT_EQ(spies.updated.count(), 1);
    EXPECT_EQ(locationsOf(spies.updatedTracks()), expected);
    QList<PTrack> tracks = library.tracks();
    EXPECT_EQ(locationsOf(tracks), expected);
    EXPECT_EQ(tracks.first()->metadata.value("title"), "Song so-1");
    EXPECT_EQ(tracks.first()->path(), "Example Artist/Example Album/so-1.flac");
    EXPECT_TRUE(QFile::exists(cacheFile_));

    ASSERT_FALSE(server_.requests().isEmpty());
    EXPECT_EQ(server_.requests().first().method, "getScanStatus");
    EXPECT_EQ(pageOffsets(), QList<int>({0, 2, 4}));
    for (const FakeSubsonicServer::Request &request : server_.requests()) {
        EXPECT_EQ(request.value("u"), "listener");
        if (request.method != "search3")
            continue;
        EXPECT_TRUE(request.has("query"));
        EXPECT_EQ(request.value("query"), "");
        EXPECT_EQ(request.value("songCount"), "2");
        EXPECT_EQ(request.value("artistCount"), "0");
        EXPECT_EQ(request.value("albumCount"), "0");
    }
}

TEST_F(NavidromeLibraryTest, SkipsDuplicatesAndDirectories) {
    songs_ = QJsonArray{song("so-1"), QJsonObject{{"id", "al-1"}, {"isDir", true}}, song("so-2"),
                        song("so-1", "Duplicate"), song("so-3")};
    NavidromeLibrary library(client_, cacheFile_);
    library.setPageSize(2);
    ASSERT_TRUE(syncAndWait(library));
    QList<PTrack> tracks = library.tracks();
    EXPECT_EQ(locationsOf(tracks), songLocations({"so-1", "so-2", "so-3"}));
    ASSERT_FALSE(tracks.isEmpty());
    EXPECT_EQ(tracks.first()->metadata.value("title"), "Song so-1");
    EXPECT_EQ(pageOffsets(), QList<int>({0, 2, 4}));
}

TEST_F(NavidromeLibraryTest, CacheIsLoadedByANewInstance) {
    {
        NavidromeLibrary library(client_, cacheFile_);
        ASSERT_TRUE(syncAndWait(library));
    }
    int requests = server_.requests().size();
    NavidromeLibrary library(client_, cacheFile_);
    QList<PTrack> tracks = library.tracks();
    EXPECT_EQ(locationsOf(tracks), songLocations({"so-1", "so-2", "so-3", "so-4", "so-5"}));
    ASSERT_FALSE(tracks.isEmpty());
    EXPECT_EQ(tracks.first()->metadata.value("title"), "Song so-1");
    EXPECT_EQ(tracks.first()->path(), "Example Artist/Example Album/so-1.flac");
    EXPECT_EQ(tracks.first()->audioproperties.length, 200);
    EXPECT_EQ(server_.requests().size(), requests);

    // The cached scan status matches the server's: nothing to sync.
    EXPECT_FALSE(checkSyncs(library));
    EXPECT_EQ(requestCount("search3"), 1);
}

TEST_F(NavidromeLibraryTest, CacheIsPrivate) {
    NavidromeLibrary library(client_, cacheFile_);
    ASSERT_TRUE(syncAndWait(library));
    EXPECT_TRUE(isPrivate(cacheFile_));
    // Also when it replaces a cache written before
    makeReadableByOthers(cacheFile_);
    ASSERT_FALSE(isPrivate(cacheFile_));
    scanCount_ = 6;
    songs_.append(song("so-6"));
    ASSERT_TRUE(syncAndWait(library));
    EXPECT_EQ(library.tracks().size(), 6);
    EXPECT_TRUE(isPrivate(cacheFile_));
}

TEST_F(NavidromeLibraryTest, IdenticalSyncEmitsNoUpdate) {
    NavidromeLibrary library(client_, cacheFile_);
    library.setPageSize(2);
    ASSERT_TRUE(syncAndWait(library));
    QList<PTrack> before = library.tracks();
    ASSERT_EQ(before.size(), 5);
    // Changes made by the UI to the shared tracks are not server changes.
    before.first()->metadata["lyrics"] = "La la la";
    before.last()->metadata.remove("title");

    LibrarySpies spies(&library);
    ASSERT_TRUE(syncAndWait(library));
    ASSERT_EQ(spies.finished.count(), 1);
    EXPECT_EQ(spies.finished.at(0).at(0).toInt(), 5);
    EXPECT_TRUE(spies.updated.isEmpty());
    EXPECT_TRUE(library.tracks() == before);
}

TEST_F(NavidromeLibraryTest, ReportsChangedRemovedAndAddedSongs) {
    NavidromeLibrary library(client_, cacheFile_);
    library.setPageSize(2);
    ASSERT_TRUE(syncAndWait(library));
    QList<PTrack> before = library.tracks();
    ASSERT_EQ(before.size(), 5);

    songs_[1] = song("so-2", "Renamed");
    songs_.removeAt(2);
    songs_.append(song("so-6"));
    LibrarySpies spies(&library);
    ASSERT_TRUE(syncAndWait(library));
    ASSERT_EQ(spies.updated.count(), 1);
    QList<PTrack> tracks = spies.updatedTracks();
    ASSERT_EQ(locationsOf(tracks), songLocations({"so-1", "so-2", "so-4", "so-5", "so-6"}));
    EXPECT_EQ(tracks[1]->metadata.value("title"), "Renamed");
    // Unchanged songs keep their track objects.
    EXPECT_TRUE(tracks[0] == before[0]);
    EXPECT_TRUE(tracks[2] == before[3]);
    EXPECT_TRUE(tracks[3] == before[4]);
    EXPECT_FALSE(tracks[1] == before[1]);
    EXPECT_TRUE(library.tracks() == tracks);
    EXPECT_EQ(spies.finished.at(0).at(0).toInt(), 5);

    NavidromeLibrary reloaded(client_, cacheFile_);
    QList<PTrack> cached = reloaded.tracks();
    EXPECT_EQ(locationsOf(cached), locationsOf(tracks));
    ASSERT_EQ(cached.size(), 5);
    EXPECT_EQ(cached[1]->metadata.value("title"), "Renamed");

    songs_.removeLast();
    LibrarySpies removal(&library);
    ASSERT_TRUE(syncAndWait(library));
    ASSERT_EQ(removal.updated.count(), 1);
    QList<PTrack> remaining = removal.updatedTracks();
    EXPECT_EQ(locationsOf(remaining), songLocations({"so-1", "so-2", "so-4", "so-5"}));
    // None changed
    EXPECT_TRUE(remaining == tracks.mid(0, 4));
}

TEST_F(NavidromeLibraryTest, PageErrorKeepsTracks) {
    NavidromeLibrary library(client_, cacheFile_);
    library.setPageSize(2);
    ASSERT_TRUE(syncAndWait(library));
    QList<PTrack> before = library.tracks();
    ASSERT_EQ(before.size(), 5);

    songs_.append(song("so-6"));
    pageResponses_.insert(2, FakeSubsonicServer::http(500, "boom"));
    LibrarySpies spies(&library);
    ASSERT_TRUE(syncAndWait(library));
    ASSERT_EQ(spies.failed.count(), 1);
    EXPECT_TRUE(spies.failed.at(0).at(0).toString().contains("500"));
    EXPECT_TRUE(spies.finished.isEmpty());
    EXPECT_TRUE(spies.updated.isEmpty());
    EXPECT_FALSE(library.isSyncing());
    EXPECT_TRUE(library.tracks() == before);
    EXPECT_EQ(pageOffsets(), QList<int>({0, 2, 4, 0, 2}));
    EXPECT_EQ(NavidromeLibrary(client_, cacheFile_).tracks().size(), 5);

    pageResponses_.clear();
    ASSERT_TRUE(syncAndWait(library));
    EXPECT_EQ(library.tracks().size(), 6);
}

TEST_F(NavidromeLibraryTest, ScanStatusErrorFailsSync) {
    methodResponses_.insert("getScanStatus", FakeSubsonicServer::failed(0, "Scanner unavailable"));
    NavidromeLibrary library(client_, cacheFile_);
    LibrarySpies spies(&library);
    ASSERT_TRUE(syncAndWait(library));
    ASSERT_EQ(spies.failed.count(), 1);
    EXPECT_EQ(spies.failed.at(0).at(0).toString(), "Scanner unavailable");
    EXPECT_EQ(requestCount("search3"), 0);
    EXPECT_FALSE(library.isSyncing());
    EXPECT_FALSE(QFile::exists(cacheFile_));
}

TEST_F(NavidromeLibraryTest, CacheOfAnotherAccountIsIgnored) {
    {
        NavidromeLibrary library(client_, cacheFile_);
        ASSERT_TRUE(syncAndWait(library));
        ASSERT_EQ(library.tracks().size(), 5);
    }
    auto cachedTracks = [&](const NavidromeConfig &config) {
        NavidromeClient client;
        client.setConfig(config);
        return NavidromeLibrary(client, cacheFile_).tracks().size();
    };
    NavidromeConfig otherUser = testConfig(server_.url());
    otherUser.username = "someone";
    EXPECT_EQ(cachedTracks(otherUser), 0);
    EXPECT_EQ(cachedTracks(testConfig("http://other.example:4533")), 0);
    EXPECT_EQ(cachedTracks(NavidromeConfig()), 0);
    NavidromeConfig newPassword = testConfig(server_.url());
    newPassword.setPassword("changed");
    EXPECT_EQ(cachedTracks(newPassword), 5);
}

TEST_F(NavidromeLibraryTest, InvalidCacheIsIgnored) {
    {
        NavidromeLibrary library(client_, cacheFile_);
        ASSERT_TRUE(syncAndWait(library));
    }
    QFile file(cacheFile_);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    QByteArray valid = file.readAll();
    file.close();
    auto cachedTracks = [&](const QByteArray &bytes) {
        QFile out(cacheFile_);
        EXPECT_TRUE(out.open(QIODevice::WriteOnly | QIODevice::Truncate));
        out.write(bytes);
        out.close();
        return NavidromeLibrary(client_, cacheFile_).tracks().size();
    };
    EXPECT_EQ(cachedTracks(valid), 5);
    EXPECT_EQ(cachedTracks(valid.left(valid.size() - 10)), 0);
    EXPECT_EQ(cachedTracks("garbage"), 0);
    EXPECT_EQ(cachedTracks(QByteArray()), 0);
    QByteArray otherMagic = valid;
    otherMagic[0] = 'F';
    EXPECT_EQ(cachedTracks(otherMagic), 0);
    QByteArray otherVersion = valid;
    otherVersion[int(sizeof("fubar-navidrome")) - 1 + 3] = 2;
    EXPECT_EQ(cachedTracks(otherVersion), 0);
}

// As written when the tracks were serialized one by one into a proto::Library
TEST_F(NavidromeLibraryTest, CacheFormatIsUnchanged) {
    NavidromeLibrary library(client_, cacheFile_);
    library.setPageSize(2);
    ASSERT_TRUE(syncAndWait(library));
    QList<PTrack> tracks = library.tracks();
    ASSERT_EQ(tracks.size(), 5);
    proto::Library expected;
    for (const PTrack &track : tracks)
        track->fillProtoTrack(*expected.add_tracks());
    QStringList header;
    QByteArray bytes = readCacheFile(cacheFile_, &header);
    EXPECT_EQ(header, (QStringList{"fubar-navidrome", "1", server_.url(), "listener",
                                   "2026-01-01T10:00:00Z", "5"}));
    EXPECT_TRUE(bytes == QByteArray::fromStdString(expected.SerializeAsString()));

    // Loaded, then written again for a new scan status
    NavidromeLibrary reloaded(client_, cacheFile_);
    QList<PTrack> cached = reloaded.tracks();
    ASSERT_EQ(locationsOf(cached), locationsOf(tracks));
    for (int i = 0; i < cached.size(); ++i) {
        EXPECT_EQ(cached[i]->metadata, tracks[i]->metadata);
        EXPECT_EQ(cached[i]->audioproperties.length, tracks[i]->audioproperties.length);
    }
    lastScan_ = "2026-01-02T10:00:00Z";
    ASSERT_TRUE(syncAndWait(reloaded));
    EXPECT_TRUE(readCacheFile(cacheFile_, &header) == bytes);
    EXPECT_EQ(header.value(4), "2026-01-02T10:00:00Z");

    songs_ = QJsonArray();
    scanCount_ = 0;
    ASSERT_TRUE(syncAndWait(reloaded));
    EXPECT_TRUE(readCacheFile(cacheFile_, &header).isEmpty());
    EXPECT_EQ(header.value(5), "0");
    EXPECT_TRUE(NavidromeLibrary(client_, cacheFile_).tracks().isEmpty());
}

TEST_F(NavidromeLibraryTest, AccountChangeDuringSyncResetsTracks) {
    NavidromeLibrary library(client_, cacheFile_);
    library.setPageSize(2);
    ASSERT_TRUE(syncAndWait(library));
    ASSERT_TRUE(QFile::exists(cacheFile_));

    songs_.append(song("so-6"));
    pageResponses_.insert(2, FakeSubsonicServer::held());
    LibrarySpies spies(&library);
    library.sync();
    ASSERT_TRUE(QTest::qWaitFor([&] { return pageOffsets().count(2) == 2; }, 5000));
    NavidromeConfig other = testConfig(server_.url());
    other.username = "someone";
    client_.setConfig(other);

    ASSERT_EQ(spies.reset.count(), 1);
    EXPECT_TRUE(spies.reset.at(0).at(0).value<QList<PTrack>>().isEmpty());
    EXPECT_FALSE(library.isSyncing());
    EXPECT_TRUE(library.tracks().isEmpty());
    EXPECT_FALSE(QFile::exists(cacheFile_));
    EXPECT_TRUE(QTest::qWaitFor([&] { return server_.openConnections() == 0; }, 5000));
    QTest::qWait(50);
    EXPECT_TRUE(spies.updated.isEmpty());
    EXPECT_TRUE(spies.finished.isEmpty());
    EXPECT_TRUE(spies.failed.isEmpty());

    pageResponses_.clear();
    ASSERT_TRUE(syncAndWait(library));
    EXPECT_EQ(library.tracks().size(), 6);
    EXPECT_EQ(server_.requests().last().value("u"), "someone");
    EXPECT_EQ(spies.reset.count(), 1);
}

TEST_F(NavidromeLibraryTest, PasswordChangeKeepsTracks) {
    NavidromeLibrary library(client_, cacheFile_);
    ASSERT_TRUE(syncAndWait(library));
    LibrarySpies spies(&library);
    NavidromeConfig config = client_.config();
    config.setPassword("changed");
    client_.setConfig(config);
    EXPECT_TRUE(spies.reset.isEmpty());
    EXPECT_EQ(library.tracks().size(), 5);
    EXPECT_TRUE(QFile::exists(cacheFile_));
}

TEST_F(NavidromeLibraryTest, CheckForUpdatesSyncsOnlyWhenScanStatusDiffers) {
    NavidromeLibrary library(client_, cacheFile_);
    library.setPageSize(2);
    EXPECT_TRUE(checkSyncs(library)); // nothing cached
    EXPECT_EQ(library.tracks().size(), 5);
    EXPECT_FALSE(checkSyncs(library));

    // A scan without song changes: synced once, and the new scan status is remembered.
    lastScan_ = "2026-01-02T10:00:00Z";
    LibrarySpies spies(&library);
    EXPECT_TRUE(checkSyncs(library));
    EXPECT_TRUE(spies.updated.isEmpty());
    EXPECT_FALSE(checkSyncs(library));
    NavidromeLibrary reloaded(client_, cacheFile_);
    EXPECT_FALSE(checkSyncs(reloaded));

    scanCount_ = 6;
    songs_.append(song("so-6"));
    EXPECT_TRUE(checkSyncs(library));
    EXPECT_EQ(library.tracks().size(), 6);

    scanning_ = true;
    lastScan_ = "2026-01-03T10:00:00Z";
    EXPECT_FALSE(checkSyncs(library));
    scanning_ = false;
    EXPECT_TRUE(checkSyncs(library));

    methodResponses_.insert("getScanStatus", FakeSubsonicServer::http(500));
    lastScan_ = "2026-01-04T10:00:00Z";
    EXPECT_FALSE(checkSyncs(library));
}

TEST_F(NavidromeLibraryTest, ASyncDuringAScanIsCheckedAgain) {
    NavidromeLibrary library(client_, cacheFile_);
    library.setPageSize(2);
    library.setRecheckDelay(100);
    LibrarySpies spies(&library);
    scanning_ = true;
    ASSERT_TRUE(syncAndWait(library));
    EXPECT_EQ(library.tracks().size(), 5);

    // The scan ends with a new song
    scanning_ = false;
    lastScan_ = "2026-01-02T10:00:00Z";
    scanCount_ = 6;
    songs_.append(song("so-6"));
    ASSERT_TRUE(QTest::qWaitFor([&] { return spies.finished.count() == 2; }, 5000));
    EXPECT_EQ(library.tracks().size(), 6);
    int requests = server_.requests().size();
    QTest::qWait(300);
    EXPECT_EQ(server_.requests().size(), requests);
}

TEST_F(NavidromeLibraryTest, ChecksWhileTheServerScansAddUpToOneRecheck) {
    NavidromeLibrary library(client_, cacheFile_);
    library.setRecheckDelay(300);
    LibrarySpies spies(&library);
    scanning_ = true;
    for (int i = 0; i < 3; ++i)
        EXPECT_FALSE(checkSyncs(library));
    scanning_ = false;
    ASSERT_TRUE(QTest::qWaitFor([&] { return spies.finished.count() == 1; }, 5000));
    QTest::qWait(500);
    // The three checks and the re-check, which starts the sync with its scan status
    EXPECT_EQ(requestCount("getScanStatus"), 4);
    EXPECT_EQ(requestCount("search3"), 1);
    EXPECT_EQ(library.tracks().size(), 5);
}

TEST_F(NavidromeLibraryTest, AFailedFirstSyncIsCheckedAgain) {
    NavidromeLibrary library(client_, cacheFile_);
    library.setRecheckDelay(100);
    LibrarySpies spies(&library);
    methodResponses_.insert("search3", FakeSubsonicServer::http(500));
    library.sync();
    ASSERT_TRUE(QTest::qWaitFor([&] { return spies.failed.count() == 1; }, 5000));
    methodResponses_.clear();
    ASSERT_TRUE(QTest::qWaitFor([&] { return spies.finished.count() == 1; }, 5000));
    EXPECT_EQ(library.tracks().size(), 5);

    // With a known scan status, the regular checks are enough
    lastScan_ = "2026-01-02T10:00:00Z";
    methodResponses_.insert("search3", FakeSubsonicServer::http(500));
    library.sync();
    ASSERT_TRUE(QTest::qWaitFor([&] { return spies.failed.count() == 2; }, 5000));
    int requests = server_.requests().size();
    QTest::qWait(300);
    EXPECT_EQ(server_.requests().size(), requests);
}

TEST_F(NavidromeLibraryTest, NotConfiguredDoesNothing) {
    NavidromeClient client;
    NavidromeLibrary library(client, cacheFile_);
    LibrarySpies spies(&library);
    library.sync();
    library.checkForUpdates();
    EXPECT_FALSE(library.isSyncing());
    QTest::qWait(50);
    EXPECT_TRUE(spies.started.isEmpty());
    EXPECT_TRUE(spies.failed.isEmpty());
    EXPECT_TRUE(server_.requests().isEmpty());
}

TEST_F(NavidromeLibraryTest, SyncWhileSyncingIsIgnored) {
    NavidromeLibrary library(client_, cacheFile_);
    LibrarySpies spies(&library);
    library.sync();
    library.sync();
    library.checkForUpdates();
    ASSERT_TRUE(QTest::qWaitFor([&] { return idle(library); }, 5000));
    EXPECT_EQ(spies.started.count(), 1);
    EXPECT_EQ(spies.finished.count(), 1);
    EXPECT_EQ(requestCount("getScanStatus"), 1);
    EXPECT_EQ(requestCount("search3"), 1);
}

TEST_F(NavidromeLibraryTest, SyncAfterCacheLoadEmitsNoUpdate) {
    {
        NavidromeLibrary library(client_, cacheFile_);
        library.setPageSize(2);
        ASSERT_TRUE(syncAndWait(library));
        library.tracks().first()->metadata["lyrics"] = "La la la";
        lastScan_ = "2026-01-02T10:00:00Z"; // scan-only change rewrites the cache
        ASSERT_TRUE(syncAndWait(library));
    }
    NavidromeLibrary library(client_, cacheFile_);
    library.setPageSize(2);
    QList<PTrack> before = library.tracks();
    ASSERT_EQ(before.size(), 5);
    EXPECT_FALSE(before.first()->metadata.contains("lyrics"));
    LibrarySpies spies(&library);
    ASSERT_TRUE(syncAndWait(library));
    ASSERT_EQ(spies.finished.count(), 1);
    EXPECT_EQ(spies.finished.at(0).at(0).toInt(), 5);
    EXPECT_TRUE(spies.updated.isEmpty());
    EXPECT_TRUE(library.tracks() == before);
}

TEST_F(NavidromeLibraryTest, PageWithoutSearchResultFailsSync) {
    NavidromeLibrary library(client_, cacheFile_);
    ASSERT_TRUE(syncAndWait(library));
    QList<PTrack> before = library.tracks();
    ASSERT_EQ(before.size(), 5);

    pageResponses_.insert(0, FakeSubsonicServer::ok());
    LibrarySpies spies(&library);
    ASSERT_TRUE(syncAndWait(library));
    ASSERT_EQ(spies.failed.count(), 1);
    EXPECT_EQ(spies.failed.at(0).at(0).toString(), "Invalid response from server");
    EXPECT_TRUE(spies.finished.isEmpty());
    EXPECT_TRUE(spies.updated.isEmpty());
    EXPECT_FALSE(library.isSyncing());
    EXPECT_TRUE(library.tracks() == before);
    EXPECT_EQ(NavidromeLibrary(client_, cacheFile_).tracks().size(), 5);

    // An empty result is an empty page.
    pageResponses_.insert(0, FakeSubsonicServer::ok({{"searchResult3", QJsonObject()}}));
    ASSERT_TRUE(syncAndWait(library));
    EXPECT_EQ(spies.failed.count(), 1);
    ASSERT_EQ(spies.updated.count(), 1);
    EXPECT_TRUE(spies.updatedTracks().isEmpty());
    EXPECT_TRUE(library.tracks().isEmpty());
}

TEST_F(NavidromeLibraryTest, SyncUsesItsConfigSnapshotForAllPages) {
    NavidromeLibrary library(client_, cacheFile_);
    library.setPageSize(2);
    LibrarySpies spies(&library);
    onRequest_ = [this](const FakeSubsonicServer::Request &request) {
        if (request.method == "search3" && request.value("songOffset") == "2") {
            NavidromeConfig config = client_.config();
            config.setPassword("changed");
            client_.setConfig(config);
        }
    };
    ASSERT_TRUE(syncAndWait(library));
    EXPECT_NE(client_.config().token, kToken);
    ASSERT_EQ(spies.finished.count(), 1);
    EXPECT_EQ(spies.finished.at(0).at(0).toInt(), 5);
    EXPECT_TRUE(spies.reset.isEmpty());
    EXPECT_EQ(pageOffsets(), QList<int>({0, 2, 4}));
    for (const FakeSubsonicServer::Request &request : server_.requests()) {
        EXPECT_EQ(request.value("t"), kToken);
        EXPECT_EQ(request.value("s"), kSalt);
    }
}

TEST_F(NavidromeLibraryTest, RepliesQueuedBeforeAnAccountChangeAreDropped) {
    NavidromeLibrary library(client_, cacheFile_);
    library.setPageSize(2);
    LibrarySpies spies(&library);
    NavidromeConfig first = client_.config();
    NavidromeConfig second = testConfig(server_.url());
    second.username = "someone";
    // Answers the held request and gives Qt's network thread time to queue the finished reply
    // before the account changes, so the reply is delivered while the sync's context still lives.
    auto answerThenSwitchTo = [&](const NavidromeConfig &account) {
        FakeSubsonicServer::Request request = server_.requests().last();
        methodResponses_.clear();
        pageResponses_.clear();
        server_.release(respond(request));
        QTest::qSleep(100);
        client_.setConfig(account);
        EXPECT_TRUE(QTest::qWaitFor([&] { return idle(library); }, 5000));
        QTest::qWait(50);
    };

    methodResponses_.insert("getScanStatus", FakeSubsonicServer::held());
    library.sync();
    ASSERT_TRUE(QTest::qWaitFor([&] { return requestCount("getScanStatus") == 1; }, 5000));
    answerThenSwitchTo(second);
    EXPECT_EQ(spies.reset.count(), 1);
    EXPECT_EQ(requestCount("search3"), 0);

    pageResponses_.insert(2, FakeSubsonicServer::held());
    library.sync();
    ASSERT_TRUE(QTest::qWaitFor([&] { return pageOffsets().contains(2); }, 5000));
    answerThenSwitchTo(first);
    EXPECT_EQ(spies.reset.count(), 2);
    EXPECT_FALSE(pageOffsets().contains(4));

    EXPECT_TRUE(spies.updated.isEmpty());
    EXPECT_TRUE(spies.finished.isEmpty());
    EXPECT_TRUE(spies.failed.isEmpty());
    EXPECT_TRUE(library.tracks().isEmpty());
    EXPECT_FALSE(QFile::exists(cacheFile_));
}

class FakePlayer : public QObject {
    Q_OBJECT
signals:
    void trackPlaying(PTrack track);
    void trackPositionChanged(qint64 position, bool userSeek);
    void stopped(qint64 finalPosition, qint64 trackLength);
};

class NavidromeScrobblerTest : public testing::Test {
protected:
    void SetUp() override {
        ASSERT_TRUE(server_.listen());
        ASSERT_TRUE(dir_.isValid());
        settings_ = std::make_unique<QSettings>(dir_.filePath("fubar.conf"), QSettings::IniFormat);
        client_.setConfig(testConfig(server_.url()));
        server_.setHandler([this](const FakeSubsonicServer::Request &request) {
            if (request.value("submission") == "true")
                return submissionResponse_;
            return FakeSubsonicServer::ok();
        });
        scrobbler_ = std::make_unique<NavidromeScrobbler>(client_, &player_, settings_.get());
    }

    void TearDown() override {
        scrobbler_.reset();
        server_.close();
    }

    static PTrack song(const QString &id, int seconds) {
        PTrack track = std::make_shared<Track>();
        track->location = navidrome::locationForSong(id);
        track->audioproperties.length = seconds;
        return track;
    }

    void play(const PTrack &track) {
        emit player_.trackPlaying(track);
        emit player_.trackPositionChanged(0, true);
    }

    // Plays from `from` to `to` (ms) in 1 s ticks.
    void advance(qint64 from, qint64 to) {
        for (qint64 position = from + 1000; position <= to; position += 1000)
            emit player_.trackPositionChanged(position, false);
    }

    void stop() { emit player_.stopped(0, 0); }

    bool idle() {
        return QTest::qWaitFor([&] { return client_.findChildren<QNetworkReply *>().isEmpty(); },
                               5000);
    }

    QList<FakeSubsonicServer::Request> scrobbles(const QString &submission) const {
        QList<FakeSubsonicServer::Request> result;
        for (const FakeSubsonicServer::Request &request : server_.requests()) {
            if (request.method == "scrobble" && request.value("submission") == submission)
                result << request;
        }
        return result;
    }

    QStringList pending() const {
        return settings_->value("navidrome/pendingScrobbles").toStringList();
    }

    FakeSubsonicServer server_;
    NavidromeClient client_;
    QTemporaryDir dir_;
    std::unique_ptr<QSettings> settings_;
    FakePlayer player_;
    std::unique_ptr<NavidromeScrobbler> scrobbler_;
    FakeSubsonicServer::Response submissionResponse_ = FakeSubsonicServer::ok();
};

TEST_F(NavidromeScrobblerTest, NowPlayingOnStart) {
    play(song("so-1", 200));
    ASSERT_TRUE(QTest::qWaitFor([&] { return scrobbles("false").size() == 1; }, 5000));
    FakeSubsonicServer::Request request = scrobbles("false").first();
    EXPECT_EQ(request.value("id"), "so-1");
    EXPECT_FALSE(request.has("time"));
    ASSERT_TRUE(idle());
    EXPECT_TRUE(scrobbles("true").isEmpty());
    EXPECT_TRUE(pending().isEmpty());
}

TEST_F(NavidromeScrobblerTest, SubmitsAfterHalfTheTrack) {
    play(song("so-1", 200));
    advance(0, 99000);
    stop();
    ASSERT_TRUE(idle());
    EXPECT_TRUE(scrobbles("true").isEmpty());

    qint64 before = QDateTime::currentMSecsSinceEpoch();
    play(song("so-2", 200));
    qint64 after = QDateTime::currentMSecsSinceEpoch();
    advance(0, 100000);
    stop();
    ASSERT_TRUE(QTest::qWaitFor([&] { return scrobbles("true").size() == 1; }, 5000));
    FakeSubsonicServer::Request request = scrobbles("true").first();
    EXPECT_EQ(request.value("id"), "so-2");
    qint64 time = request.value("time").toLongLong();
    EXPECT_GE(time, before);
    EXPECT_LE(time, after);
    EXPECT_TRUE(QTest::qWaitFor([&] { return pending().isEmpty(); }, 5000));
}

TEST_F(NavidromeScrobblerTest, FourMinutesAreEnoughForLongTracks) {
    play(song("so-long-1", 3600));
    advance(0, 239000);
    play(song("so-long-2", 3600)); // the previous track is evaluated first
    advance(0, 240000);
    stop();
    ASSERT_TRUE(QTest::qWaitFor([&] { return scrobbles("true").size() == 1; }, 5000));
    ASSERT_TRUE(idle());
    QList<FakeSubsonicServer::Request> submissions = scrobbles("true");
    ASSERT_EQ(submissions.size(), 1);
    EXPECT_EQ(submissions.first().value("id"), "so-long-2");
    EXPECT_EQ(scrobbles("false").size(), 2);
}

TEST_F(NavidromeScrobblerTest, SeekForwardThenProgressIsSubmitted) {
    play(song("so-1", 200));
    advance(0, 10000);
    emit player_.trackPositionChanged(60000, true);
    advance(60000, 150000);
    stop();
    ASSERT_TRUE(QTest::qWaitFor([&] { return scrobbles("true").size() == 1; }, 5000));
    EXPECT_EQ(scrobbles("true").first().value("id"), "so-1");
}

TEST_F(NavidromeScrobblerTest, JumpsAreNotCounted) {
    play(song("so-1", 200));
    advance(0, 50000);
    emit player_.trackPositionChanged(120000, false);
    advance(120000, 169000);
    stop();
    ASSERT_TRUE(idle());
    EXPECT_TRUE(scrobbles("true").isEmpty());
}

TEST_F(NavidromeScrobblerTest, IgnoresLocalShortAndDisabled) {
    PTrack local = song("so-1", 200);
    local->location = "/music/so-1.flac";
    play(local);
    advance(0, 200000);
    stop();
    play(nullptr);
    stop();
    ASSERT_TRUE(idle());
    EXPECT_TRUE(server_.requests().isEmpty());

    play(song("so-short", 30));
    advance(0, 30000);
    stop();
    ASSERT_TRUE(idle());
    EXPECT_EQ(scrobbles("false").size(), 1);
    EXPECT_TRUE(scrobbles("true").isEmpty());

    NavidromeConfig config = client_.config();
    config.scrobble = false;
    client_.setConfig(config);
    int requests = server_.requests().size();
    play(song("so-2", 200));
    advance(0, 200000);
    stop();
    ASSERT_TRUE(idle());
    EXPECT_EQ(server_.requests().size(), requests);
    EXPECT_TRUE(pending().isEmpty());
}

TEST_F(NavidromeScrobblerTest, FailedSubmissionStaysQueuedAndIsRetried) {
    submissionResponse_ = FakeSubsonicServer::http(500, "boom");
    play(song("so-1", 200));
    advance(0, 100000);
    stop();
    ASSERT_TRUE(QTest::qWaitFor([&] { return scrobbles("true").size() == 1; }, 5000));
    ASSERT_TRUE(idle());
    QStringList queued = pending();
    ASSERT_EQ(queued.size(), 1);
    EXPECT_TRUE(queued.first().startsWith("so-1 "));
    EXPECT_EQ(queued.first().section(' ', 1), scrobbles("true").first().value("time"));

    scrobbler_->flushPending();
    ASSERT_TRUE(QTest::qWaitFor([&] { return scrobbles("true").size() == 2; }, 5000));
    ASSERT_TRUE(idle());
    EXPECT_EQ(pending(), queued);

    // Next start, with the server answering again.
    submissionResponse_ = FakeSubsonicServer::ok();
    scrobbler_ = std::make_unique<NavidromeScrobbler>(client_, &player_, settings_.get());
    scrobbler_->flushPending();
    ASSERT_TRUE(QTest::qWaitFor([&] { return scrobbles("true").size() == 3; }, 5000));
    EXPECT_EQ(scrobbles("true").last().value("id"), "so-1");
    EXPECT_EQ(scrobbles("true").last().value("time"), queued.first().section(' ', 1));
    EXPECT_TRUE(QTest::qWaitFor([&] { return pending().isEmpty(); }, 5000));
}

TEST_F(NavidromeScrobblerTest, SuccessFlushesOtherPendingSubmissions) {
    settings_->setValue("navidrome/pendingScrobbles", QStringList({"so-old 1700000000000"}));
    play(song("so-1", 200));
    advance(0, 100000);
    stop();
    ASSERT_TRUE(QTest::qWaitFor([&] { return scrobbles("true").size() == 2; }, 5000));
    EXPECT_EQ(scrobbles("true").last().value("id"), "so-old");
    EXPECT_EQ(scrobbles("true").last().value("time"), "1700000000000");
    EXPECT_TRUE(QTest::qWaitFor([&] { return pending().isEmpty(); }, 5000));
}

TEST_F(NavidromeScrobblerTest, FlushSkipsSubmissionsInFlight) {
    settings_->setValue("navidrome/pendingScrobbles",
                        QStringList({"so-old-1 1700000000000", "so-old-2 1700000000001"}));
    submissionResponse_ = FakeSubsonicServer::held();
    scrobbler_->flushPending();
    scrobbler_->flushPending();
    ASSERT_TRUE(QTest::qWaitFor([&] { return scrobbles("true").size() == 2; }, 5000));
    QTest::qWait(50);
    EXPECT_EQ(scrobbles("true").size(), 2);
}

TEST_F(NavidromeScrobblerTest, QueueIsCapped) {
    QStringList entries;
    for (int i = 0; i < 500; ++i)
        entries << QString("so-old-%1 %2").arg(i).arg(1700000000000 + i);
    settings_->setValue("navidrome/pendingScrobbles", entries);
    submissionResponse_ = FakeSubsonicServer::http(500);
    play(song("so-1", 200));
    advance(0, 100000);
    stop();
    ASSERT_TRUE(QTest::qWaitFor([&] { return scrobbles("true").size() == 1; }, 5000));
    ASSERT_TRUE(idle());
    QStringList queued = pending();
    ASSERT_EQ(queued.size(), 500);
    EXPECT_EQ(queued.first(), "so-old-1 1700000000001");
    EXPECT_TRUE(queued.last().startsWith("so-1 "));
}

TEST_F(NavidromeScrobblerTest, FlushIsDisabledWithoutScrobbling) {
    QStringList entries({"so-old 1700000000000"});
    settings_->setValue("navidrome/pendingScrobbles", entries);
    NavidromeConfig config = client_.config();
    config.scrobble = false;
    client_.setConfig(config);
    scrobbler_->flushPending();

    NavidromeClient unconfigured;
    NavidromeScrobbler other(unconfigured, &player_, settings_.get());
    other.flushPending();

    QTest::qWait(50);
    EXPECT_TRUE(server_.requests().isEmpty());
    EXPECT_EQ(pending(), entries);
}

TEST_F(NavidromeScrobblerTest, SubmissionOfAnUnknownSongIsDropped) {
    submissionResponse_ = FakeSubsonicServer::failed(40, "Wrong username or password");
    play(song("so-1", 200));
    advance(0, 100000);
    stop();
    ASSERT_TRUE(QTest::qWaitFor([&] { return scrobbles("true").size() == 1; }, 5000));
    ASSERT_TRUE(idle());
    EXPECT_EQ(pending().size(), 1);

    submissionResponse_ = FakeSubsonicServer::failed(70, "Song not found");
    scrobbler_->flushPending();
    ASSERT_TRUE(QTest::qWaitFor([&] { return scrobbles("true").size() == 2; }, 5000));
    ASSERT_TRUE(idle());
    EXPECT_TRUE(pending().isEmpty());
}

TEST_F(NavidromeScrobblerTest, FlushDropsMalformedEntries) {
    settings_->setValue("navidrome/pendingScrobbles",
                        QStringList({"garbage", "so-old 1700000000000", " 1700000000001",
                                     "so-new later", ""}));
    submissionResponse_ = FakeSubsonicServer::http(500);
    scrobbler_->flushPending();
    EXPECT_EQ(pending(), QStringList({"so-old 1700000000000"}));
    ASSERT_TRUE(QTest::qWaitFor([&] { return scrobbles("true").size() == 1; }, 5000));
    ASSERT_TRUE(idle());
    EXPECT_EQ(scrobbles("true").size(), 1);
    EXPECT_EQ(scrobbles("true").first().value("id"), "so-old");
    EXPECT_EQ(pending(), QStringList({"so-old 1700000000000"}));
}

TEST_F(NavidromeScrobblerTest, AccountChangeDropsThePlaysOfTheOldAccount) {
    submissionResponse_ = FakeSubsonicServer::http(500);
    play(song("so-1", 200));
    advance(0, 100000);
    stop();
    ASSERT_TRUE(QTest::qWaitFor([&] { return scrobbles("true").size() == 1; }, 5000));
    ASSERT_TRUE(idle());
    ASSERT_EQ(pending().size(), 1);

    NavidromeConfig config = client_.config();
    config.setPassword("changed");
    client_.setConfig(config);
    EXPECT_EQ(pending().size(), 1);

    play(song("so-2", 200));
    advance(0, 100000);
    config.username = "someone";
    client_.setConfig(config);
    EXPECT_TRUE(pending().isEmpty());
    stop();
    ASSERT_TRUE(idle());
    EXPECT_EQ(scrobbles("true").size(), 1);
    EXPECT_TRUE(pending().isEmpty());
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    QCoreApplication app(argc, argv);
    return RUN_ALL_TESTS();
}

#include "navidrometest.moc"
