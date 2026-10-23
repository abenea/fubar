#include "library/library.h"
#include "library/track.h"
#include "navidrome/navidromeclient.h"
#include "navidrome/navidromeconfig.h"
#include "navidrome/navidromelibrary.h"
#include "navidrome/navidrometrack.h"
#include "navidrome/tests/fakesubsonicserver.h"
#include "navidrome/tests/testsupport.h"
#include "player/playlist.h"
#include "player/tests/mockaudiooutput.h"
#include "ui/audioplayer.h"
#include "ui/elidedlabel.h"
#include "ui/mainwindow.h"
#include "ui/navidromepreferences.h"
#include "ui/playlistmodel.h"
#include "ui/playlisttab.h"
#include "util.h"
#include "gtest/gtest.h"
#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QSortFilterProxyModel>
#include <QStatusBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <memory>

namespace {

constexpr const char *kInvalidUrl = "Enter a valid http:// or https:// server URL";
constexpr const char *kNotConfigured = "Navidrome is not configured (Preferences → Navidrome…)";

PTrack song(const QString &id, const QString &title, const QString &artist = QString()) {
    auto track = std::make_shared<Track>();
    track->location = navidrome::locationForSong(id);
    track->metadata["_path"] = "Artist/Album/" + id + ".flac";
    if (!title.isEmpty())
        track->metadata["title"] = title;
    if (!artist.isEmpty())
        track->metadata["artist"] = artist;
    return track;
}

// Every test gets its own settings file for QSettings() and settingsDirFilePath().
class SettingsTest : public testing::Test {
protected:
    void SetUp() override {
        ASSERT_TRUE(dir_.isValid());
        QCoreApplication::setOrganizationName("fubar-test");
        QCoreApplication::setApplicationName("navidromeuitest");
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, dir_.path());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir_.path());
    }

    static void seed(const NavidromeConfig &config, const QStringList &pendingScrobbles = {}) {
        QSettings settings;
        config.save(settings);
        if (!pendingScrobbles.isEmpty())
            settings.setValue("navidrome/pendingScrobbles", pendingScrobbles);
    }

    static QByteArray settingsFile() {
        QSettings settings;
        settings.sync();
        QFile file(settings.fileName());
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

    QTemporaryDir dir_;
};

} // namespace

TEST(NavidromePreferencesRulesTest, CanReuseToken) {
    NavidromeConfig saved = testConfig();
    EXPECT_TRUE(
        NavidromePreferences::canReuseToken(saved, "http://music.example:4533", "listener"));
    // Form values are normalized and trimmed first
    EXPECT_TRUE(
        NavidromePreferences::canReuseToken(saved, " music.example:4533/rest/ ", " listener "));
    EXPECT_FALSE(
        NavidromePreferences::canReuseToken(saved, "https://music.example:4533", "listener"));
    EXPECT_FALSE(
        NavidromePreferences::canReuseToken(saved, "http://music.example:4533", "someone"));
    EXPECT_FALSE(NavidromePreferences::canReuseToken(saved, "", "listener"));
    NavidromeConfig noToken = saved;
    noToken.token.clear();
    EXPECT_FALSE(
        NavidromePreferences::canReuseToken(noToken, "http://music.example:4533", "listener"));
    EXPECT_FALSE(NavidromePreferences::canReuseToken(NavidromeConfig(), "", ""));
}

TEST(NavidromePreferencesRulesTest, IsUnencrypted) {
    for (const char *url :
         {"http://music.example.com", "music.example.com:4533/navidrome", "http://nas:4533",
          "http://nas.lan", "http://8.8.8.8:4533", "http://172.32.0.1", "http://192.169.0.1",
          "http://[2001:db8::1]:4533", "http://[::ffff:8.8.8.8]"}) {
        EXPECT_TRUE(NavidromePreferences::isUnencrypted(url)) << url;
    }
    for (const char *url :
         {"https://music.example.com", "http://localhost:4533", "http://LOCALHOST",
          "http://app.localhost", "http://nas.local", "http://music.home.arpa",
          "http://box.internal", "http://127.0.0.1:4533", "http://10.1.2.3", "http://172.16.0.1",
          "http://172.31.255.255", "http://192.168.1.10:4533", "http://169.254.1.1",
          "http://[::1]:4533", "http://[fd12:3456::1]", "http://[fe80::1]",
          "http://[::ffff:192.168.1.1]", "http://", ""}) {
        EXPECT_FALSE(NavidromePreferences::isUnencrypted(url)) << url;
    }
}

TEST(NavidromePreferencesRulesTest, EmptyUrlGivesAnEmptyConfig) {
    NavidromeConfig config = testConfig();
    EXPECT_EQ(NavidromePreferences::configFromForm(testConfig(), "  ", "listener", "sesame",
                                                   false, &config),
              "");
    EXPECT_TRUE(config.serverUrl.isEmpty());
    EXPECT_TRUE(config.username.isEmpty());
    EXPECT_TRUE(config.salt.isEmpty());
    EXPECT_TRUE(config.token.isEmpty());
    EXPECT_FALSE(config.scrobble);
}

TEST(NavidromePreferencesRulesTest, PasswordMakesANewToken) {
    NavidromeConfig config;
    EXPECT_EQ(NavidromePreferences::configFromForm(NavidromeConfig(), " music.example:4533/ ",
                                                   " listener ", "pass word", true, &config),
              "");
    EXPECT_EQ(config.serverUrl, "http://music.example:4533");
    EXPECT_EQ(config.username, "listener");
    EXPECT_FALSE(config.salt.isEmpty());
    EXPECT_EQ(config.token, NavidromeConfig::makeToken("pass word", config.salt));
    EXPECT_TRUE(config.scrobble);

    // Also when the saved token could be reused
    EXPECT_EQ(NavidromePreferences::configFromForm(testConfig(), "http://music.example:4533",
                                                   "listener", "sesame", true, &config),
              "");
    EXPECT_NE(config.salt, kSalt);
    EXPECT_EQ(config.token, NavidromeConfig::makeToken("sesame", config.salt));
}

TEST(NavidromePreferencesRulesTest, EmptyPasswordReusesTheSavedToken) {
    NavidromeConfig config;
    EXPECT_EQ(NavidromePreferences::configFromForm(testConfig(), "music.example:4533",
                                                   "listener", "", false, &config),
              "");
    EXPECT_EQ(config.serverUrl, "http://music.example:4533");
    EXPECT_EQ(config.salt, kSalt);
    EXPECT_EQ(config.token, kToken);
    EXPECT_FALSE(config.scrobble);
}

TEST(NavidromePreferencesRulesTest, EmptyPasswordForAnotherAccountIsRejected) {
    NavidromeConfig config;
    EXPECT_EQ(NavidromePreferences::configFromForm(testConfig(), "http://other.example:4533",
                                                   "listener", "", true, &config),
              "Enter the password");
    EXPECT_EQ(NavidromePreferences::configFromForm(testConfig(), "http://music.example:4533",
                                                   "someone", "", true, &config),
              "Enter the password");
    EXPECT_EQ(NavidromePreferences::configFromForm(NavidromeConfig(), "http://music.example:4533",
                                                   "listener", "", true, &config),
              "Enter the password");
}

TEST(NavidromePreferencesRulesTest, InvalidUrlOrMissingUsernameIsRejected) {
    NavidromeConfig config;
    for (const char *url : {"ftp://music.example", "http://", "http://exa mple.com"}) {
        EXPECT_EQ(NavidromePreferences::configFromForm(NavidromeConfig(), url, "listener",
                                                       "sesame", true, &config),
                  kInvalidUrl)
            << url;
    }
    EXPECT_EQ(NavidromePreferences::configFromForm(NavidromeConfig(), "http://music.example",
                                                   "  ", "sesame", true, &config),
              "Enter the username");
}

class NavidromePreferencesTest : public SettingsTest {
protected:
    void SetUp() override {
        SettingsTest::SetUp();
        ASSERT_TRUE(server_.listen());
    }

    void TearDown() override {
        delete dialog_;
        server_.close();
    }

    // Saves `config` the way the application does and applies it to the client.
    void configure(const NavidromeConfig &config) {
        seed(config);
        client_.setConfig(config);
    }

    void open() {
        dialog_ = new NavidromePreferences(client_);
        saved_ = std::make_unique<QSignalSpy>(dialog_.data(), &NavidromePreferences::configSaved);
        dialog_->show();
    }

    template <typename T>
    T *child(const char *name) {
        T *widget = dialog_ ? dialog_->findChild<T *>(name) : nullptr;
        EXPECT_NE(widget, nullptr) << name;
        return widget;
    }

    QLineEdit *edit(const char *name) { return child<QLineEdit>(name); }
    QPushButton *testButton() { return child<QPushButton>("testConnection"); }
    QString status() { return child<QLabel>("status")->text(); }

    void fill(const QString &serverUrl, const QString &username, const QString &password) {
        edit("serverUrl")->setText(serverUrl);
        edit("username")->setText(username);
        edit("password")->setText(password);
    }

    void clickOk() {
        child<QDialogButtonBox>("buttonBox")->button(QDialogButtonBox::Ok)->click();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    // Clicks Test connection and waits for the result.
    QString testConnection() {
        testButton()->click();
        EXPECT_FALSE(testButton()->isEnabled());
        EXPECT_TRUE(QTest::qWaitFor([&] { return testButton()->isEnabled(); }, 5000));
        return status();
    }

    FakeSubsonicServer server_;
    NavidromeClient client_;
    QPointer<NavidromePreferences> dialog_;
    std::unique_ptr<QSignalSpy> saved_;
};

TEST_F(NavidromePreferencesTest, OkSavesTheTokenNotThePassword) {
    open();
    EXPECT_FALSE(dialog_->isModal());
    EXPECT_TRUE(dialog_->testAttribute(Qt::WA_DeleteOnClose));
    EXPECT_EQ(edit("serverUrl")->placeholderText(), "http://localhost:4533");
    EXPECT_EQ(edit("password")->echoMode(), QLineEdit::Password);
    EXPECT_TRUE(child<QLabel>("scrobbleHint")->text().contains("Last.fm"));
    fill(server_.url() + "/", " listener ", "Pass-Word 123");
    child<QCheckBox>("scrobble")->setChecked(false);
    clickOk();

    EXPECT_TRUE(dialog_.isNull());
    ASSERT_EQ(saved_->size(), 1);
    EXPECT_TRUE(saved_->at(0).at(0).toBool());
    const NavidromeConfig &config = client_.config();
    EXPECT_EQ(config.serverUrl, server_.url());
    EXPECT_EQ(config.username, "listener");
    EXPECT_EQ(config.token, NavidromeConfig::makeToken("Pass-Word 123", config.salt));
    EXPECT_FALSE(config.scrobble);
    QSettings settings;
    NavidromeConfig loaded = NavidromeConfig::load(settings);
    EXPECT_TRUE(loaded.sameConnection(config));
    EXPECT_FALSE(loaded.scrobble);
    QByteArray file = settingsFile();
    EXPECT_TRUE(file.contains(config.token.toLatin1()));
    EXPECT_FALSE(file.contains("Pass-Word"));
    EXPECT_TRUE(server_.requests().isEmpty());
}

TEST_F(NavidromePreferencesTest, InvalidUrlKeepsTheDialogOpen) {
    open();
    fill("ftp://music.example", "listener", "sesame");
    clickOk();
    ASSERT_FALSE(dialog_.isNull());
    EXPECT_TRUE(dialog_->isVisible());
    EXPECT_EQ(status(), kInvalidUrl);
    EXPECT_EQ(saved_->size(), 0);
    EXPECT_FALSE(client_.isConfigured());
    EXPECT_FALSE(QSettings().contains("navidrome/serverUrl"));
    // Editing clears the error
    edit("serverUrl")->setText("https://music.example");
    EXPECT_EQ(status(), "");
}

TEST_F(NavidromePreferencesTest, EmptyUrlClearsTheConfig) {
    configure(testConfig(server_.url()));
    open();
    EXPECT_EQ(edit("serverUrl")->text(), server_.url());
    EXPECT_EQ(edit("username")->text(), "listener");
    EXPECT_EQ(edit("password")->text(), "");
    edit("serverUrl")->setText("  ");
    clickOk();

    EXPECT_TRUE(dialog_.isNull());
    ASSERT_EQ(saved_->size(), 1);
    EXPECT_FALSE(saved_->at(0).at(0).toBool());
    EXPECT_FALSE(client_.isConfigured());
    EXPECT_TRUE(client_.config().serverUrl.isEmpty());
    QSettings settings;
    NavidromeConfig loaded = NavidromeConfig::load(settings);
    EXPECT_TRUE(loaded.serverUrl.isEmpty());
    EXPECT_TRUE(loaded.salt.isEmpty());
    EXPECT_TRUE(loaded.token.isEmpty());
    EXPECT_FALSE(settingsFile().contains(kToken));
}

TEST_F(NavidromePreferencesTest, PasswordIsRequiredUnlessTheTokenIsReusable) {
    configure(testConfig(server_.url()));
    open();
    EXPECT_EQ(edit("password")->placeholderText(), "Unchanged");
    edit("username")->setText("someone");
    EXPECT_EQ(edit("password")->placeholderText(), "");
    clickOk();
    ASSERT_FALSE(dialog_.isNull());
    EXPECT_EQ(status(), "Enter the password");
    EXPECT_EQ(saved_->size(), 0);
    EXPECT_EQ(client_.config().username, "listener");

    edit("username")->setText("listener");
    EXPECT_EQ(edit("password")->placeholderText(), "Unchanged");
    clickOk();
    EXPECT_TRUE(dialog_.isNull());
    ASSERT_EQ(saved_->size(), 1);
    EXPECT_FALSE(saved_->at(0).at(0).toBool());
    EXPECT_TRUE(client_.config().sameConnection(testConfig(server_.url())));
}

TEST_F(NavidromePreferencesTest, AccountChangeIsReported) {
    configure(testConfig(server_.url()));
    open();
    fill(server_.url(), "someone", "secret");
    clickOk();
    ASSERT_EQ(saved_->size(), 1);
    EXPECT_TRUE(saved_->at(0).at(0).toBool());
    EXPECT_EQ(client_.config().username, "someone");
}

TEST_F(NavidromePreferencesTest, WarnsAboutUnencryptedRemoteServers) {
    open();
    QLabel *warning = child<QLabel>("insecureWarning");
    EXPECT_TRUE(warning->isHidden());
    edit("serverUrl")->setText("music.example.com:4533");
    EXPECT_FALSE(warning->isHidden());
    edit("serverUrl")->setText("https://music.example.com");
    EXPECT_TRUE(warning->isHidden());
    edit("serverUrl")->setText("http://192.168.1.10:4533");
    EXPECT_TRUE(warning->isHidden());
}

TEST_F(NavidromePreferencesTest, TestConnectionUsesTheFormValues) {
    server_.setResponse("ping", FakeSubsonicServer::ok());
    open();
    fill(server_.url(), "listener", "sesame");
    EXPECT_EQ(testConnection(), "Connected to Navidrome 0.0.1");
    ASSERT_EQ(server_.requests().size(), 1);
    const FakeSubsonicServer::Request &request = server_.requests().first();
    EXPECT_EQ(request.method, "ping");
    EXPECT_EQ(request.value("u"), "listener");
    EXPECT_EQ(request.value("t"), NavidromeConfig::makeToken("sesame", request.value("s")));
    // Nothing is saved
    EXPECT_FALSE(client_.isConfigured());
    EXPECT_FALSE(QSettings().contains("navidrome/token"));
    EXPECT_FALSE(dialog_.isNull());
}

TEST_F(NavidromePreferencesTest, TestConnectionReportsFailures) {
    server_.setResponse("ping", FakeSubsonicServer::failed(40, "Wrong username or password"));
    open();
    fill(server_.url(), "listener", "wrong");
    EXPECT_EQ(testConnection(), "Connection failed: Wrong username or password");
}

TEST_F(NavidromePreferencesTest, TestConnectionExplainsBlockedRedirects) {
    FakeSubsonicServer other;
    ASSERT_TRUE(other.listen());
    other.setResponse("ping", FakeSubsonicServer::ok());
    server_.setResponse("ping", FakeSubsonicServer::redirect(other.url() + "/rest/ping"));
    open();
    fill(server_.url(), "listener", "sesame");
    EXPECT_EQ(testConnection(),
              "Connection failed: The server redirected to another address; enter that address "
              "(e.g. https://…)");
    EXPECT_TRUE(other.requests().isEmpty());
    other.close();
}

TEST_F(NavidromePreferencesTest, TestConnectionWithoutPassword) {
    configure(testConfig(server_.url()));
    server_.setResponse("ping", FakeSubsonicServer::ok());
    open();
    edit("username")->setText("someone");
    testButton()->click();
    EXPECT_EQ(status(), "Enter the password");
    EXPECT_TRUE(testButton()->isEnabled());
    QTest::qWait(50);
    EXPECT_TRUE(server_.requests().isEmpty());

    // The saved token is used for the saved account
    edit("username")->setText("listener");
    EXPECT_EQ(testConnection(), "Connected to Navidrome 0.0.1");
    ASSERT_EQ(server_.requests().size(), 1);
    EXPECT_EQ(server_.requests().first().value("t"), kToken);
}

TEST_F(NavidromePreferencesTest, EditingDropsARunningTest) {
    server_.setResponse("ping", FakeSubsonicServer::held());
    open();
    fill(server_.url(), "listener", "sesame");
    testButton()->click();
    ASSERT_TRUE(QTest::qWaitFor([&] { return server_.requests().size() == 1; }, 5000));
    EXPECT_FALSE(testButton()->isEnabled());
    edit("username")->setText("someone");
    EXPECT_TRUE(testButton()->isEnabled());
    server_.release(FakeSubsonicServer::ok());
    QTest::qWait(100);
    EXPECT_EQ(status(), "");
}

class MainWindowNavidromeTest : public SettingsTest {
protected:
    void SetUp() override {
        SettingsTest::SetUp();
        ASSERT_TRUE(server_.listen());
        // Asked after a load failure: the song is fine
        server_.setResponse("getSong", FakeSubsonicServer::ok());
        // An empty local library, for library views. Created now: it reads the settings.
        library_ = std::make_unique<Library>();
        player_ = std::make_unique<AudioPlayer>(library_.get(), &output_, /*testing=*/true);
    }

    void TearDown() override {
        window_.reset();
        player_.reset();
        library_.reset();
        server_.close();
    }

    // A configured account on the fake server that does not scrobble.
    NavidromeConfig fakeServerConfig() {
        NavidromeConfig config = testConfig(server_.url());
        config.scrobble = false;
        return config;
    }

    void createWindow() { window_ = std::make_unique<MainWindow>(*player_); }

    PModel playlist(const QList<PTrack> &tracks) {
        PModel model = player_->createPlaylist(false);
        window_->addPlaylist(model, "Test");
        model->addTracks(tracks);
        return model;
    }

    void play(PModel model, int row) { player_->play(model, model->index(row, 0)); }

    QAction *action(const char *name) {
        QAction *action = window_->findChild<QAction *>(name);
        EXPECT_NE(action, nullptr) << name;
        return action;
    }

    QLabel *statusLabel() {
        QLabel *label = window_->findChild<QLabel *>("StatusLabel");
        EXPECT_NE(label, nullptr);
        return label;
    }

    // The full text: the label shows it elided to its width
    QString status() {
        QLabel *label = statusLabel();
        return label ? label->toolTip() : QString();
    }

    // Fills the open Navidrome preferences (scrobbling off) and clicks OK.
    void savePreferences(const QString &serverUrl,
                         const QString &username,
                         const QString &password) {
        auto *dialog = window_->findChild<NavidromePreferences *>();
        ASSERT_NE(dialog, nullptr);
        dialog->findChild<QLineEdit *>("serverUrl")->setText(serverUrl);
        dialog->findChild<QLineEdit *>("username")->setText(username);
        dialog->findChild<QLineEdit *>("password")->setText(password);
        dialog->findChild<QCheckBox *>("scrobble")->setChecked(false);
        dialog->findChild<QDialogButtonBox *>("buttonBox")->button(QDialogButtonBox::Ok)->click();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        ASSERT_EQ(window_->findChild<NavidromePreferences *>(), nullptr);
    }

    FakeSubsonicServer server_;
    MockAudioOutput output_;
    std::unique_ptr<Library> library_;
    std::unique_ptr<AudioPlayer> player_;
    std::unique_ptr<MainWindow> window_;
};

TEST_F(MainWindowNavidromeTest, NavidromeTracksStreamFromTheServer) {
    seed(fakeServerConfig());
    createWindow();
    PModel model = playlist({song("so-1", "First")});
    output_.setLoadOnPlay(false);
    play(model, 0);
    // Only parts of the URL are compared: it carries the token
    QUrl source(output_.currentSource());
    QUrl server(server_.url());
    EXPECT_EQ(source.scheme(), "http");
    EXPECT_EQ(source.host(), server.host());
    EXPECT_EQ(source.port(), server.port());
    EXPECT_EQ(source.path(), "/rest/stream");
    EXPECT_EQ(QUrlQuery(source).queryItemValue("id"), "so-1");
    EXPECT_EQ(status(), "");
}

TEST_F(MainWindowNavidromeTest, UnconfiguredNavidromeTrackShowsHowToConfigure) {
    createWindow();
    auto local = std::make_shared<Track>();
    local->location = "/music/local.flac";
    local->metadata["title"] = "Local";
    PModel model = playlist({song("so-1", "First"), local});
    play(model, 0);
    EXPECT_TRUE(output_.received().isEmpty());
    EXPECT_EQ(status(), QString("Cannot play First: ") + kNotConfigured);
    // Repeated errors only replace the text
    play(model, 0);
    EXPECT_EQ(status(), QString("Cannot play First: ") + kNotConfigured);
    // Until a track plays
    play(model, 1);
    EXPECT_EQ(output_.currentSource(), "/music/local.flac");
    EXPECT_EQ(status(), "");
    QTest::qWait(50);
    EXPECT_TRUE(server_.requests().isEmpty());
}

TEST_F(MainWindowNavidromeTest, LoadFailureNamesTheTrack) {
    seed(fakeServerConfig());
    createWindow();
    PModel model = playlist({song("so-1", "Title", "Artist"), song("so-2", "")});
    output_.setLoadOnPlay(false);
    play(model, 0);
    output_.triggerLoadFailed("loading failed");
    EXPECT_EQ(status(), "Cannot play Artist – Title: loading failed");
    play(model, 1);
    output_.triggerLoadFailed("loading failed");
    EXPECT_EQ(status(), "Cannot play so-2: loading failed");
    // A stale failure without a pending load
    output_.triggerLoadFailed("loading failed");
    EXPECT_EQ(status(), "Cannot play so-2: loading failed");

    output_.setLoadOnPlay(true);
    play(model, 1);
    EXPECT_EQ(status(), "");
}

TEST_F(MainWindowNavidromeTest, LoadFailureShowsWhatTheServerSays) {
    seed(fakeServerConfig());
    server_.setResponse("getSong", FakeSubsonicServer::failed(40, "Wrong username or password"));
    createWindow();
    PModel model = playlist({song("so-1", "Title", "Artist"), song("so-2", "Other")});
    output_.setLoadOnPlay(false);
    play(model, 0);
    output_.triggerLoadFailed("unrecognized file format");
    EXPECT_EQ(status(), "Cannot play Artist – Title: unrecognized file format");
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return status() == "Cannot play Artist – Title: Wrong username or password"; },
        5000));
    ASSERT_EQ(server_.requests().size(), 1);
    EXPECT_EQ(server_.requests()[0].method, "getSong");
    EXPECT_EQ(server_.requests()[0].value("id"), "so-1");

    // The server has no objection
    server_.setResponse("getSong", FakeSubsonicServer::ok());
    play(model, 1);
    output_.triggerLoadFailed("loading failed");
    ASSERT_TRUE(QTest::qWaitFor([&] { return server_.requests().size() == 2; }, 5000));
    QTest::qWait(100);
    EXPECT_EQ(status(), "Cannot play Other: loading failed");

    // A late answer does not replace a newer status
    server_.setResponse("getSong", FakeSubsonicServer::held());
    play(model, 0);
    output_.triggerLoadFailed("loading failed");
    ASSERT_TRUE(QTest::qWaitFor([&] { return server_.requests().size() == 3; }, 5000));
    output_.setLoadOnPlay(true);
    play(model, 1);
    EXPECT_EQ(status(), "");
    server_.release(FakeSubsonicServer::failed(40, "Wrong username or password"));
    QTest::qWait(100);
    EXPECT_EQ(status(), "");
}

TEST_F(MainWindowNavidromeTest, ErrorsShowNoCredentials) {
    createWindow();
    auto stream = std::make_shared<Track>();
    stream->location = "http://h:4533/rest/stream?u=a&t=SECRET&s=b&id=x";
    PModel model = playlist({stream});
    output_.setLoadOnPlay(false);
    play(model, 0);
    output_.triggerLoadFailed("loading failed");
    QLabel *label = statusLabel();
    ASSERT_NE(label, nullptr);
    EXPECT_TRUE(label->toolTip().startsWith("Cannot play stream?u=***&t=***&s=***"));
    EXPECT_FALSE(label->toolTip().contains("SECRET"));
    EXPECT_FALSE(label->text().contains("SECRET"));
}

TEST_F(MainWindowNavidromeTest, LongErrorsDoNotWidenTheWindow) {
    seed(fakeServerConfig());
    // Else the window maximizes itself a few ms after the constructor (offscreen: 796 px wide)
    QSettings().setValue("mainwindow/maximized", false);
    createWindow();
    window_->resize(800, 600);
    window_->show();
    QString title = QString("Allegro ma non troppo ").repeated(20).trimmed();
    PModel model = playlist({song("so-1", title), song("so-2", "Short")});
    QTest::qWait(10);
    int width = window_->width();
    int minimumWidth = window_->minimumSizeHint().width();
    output_.setLoadOnPlay(false);
    play(model, 0);
    output_.triggerLoadFailed("loading failed");
    QTest::qWait(10);

    QLabel *label = statusLabel();
    ASSERT_NE(label, nullptr);
    QString message = "Cannot play " + title + ": loading failed";
    EXPECT_EQ(label->toolTip(), message);
    EXPECT_NE(label->text(), message);
    EXPECT_TRUE(label->text().startsWith("Cannot play Allegro"));
    EXPECT_TRUE(label->text().endsWith(": loading failed"));
    EXPECT_EQ(window_->width(), width);
    EXPECT_EQ(window_->minimumSizeHint().width(), minimumWidth);
    // The playback message on the left keeps room
    EXPECT_GT(label->x(), window_->statusBar()->width() / 4);

    // Short texts are shown in full
    play(model, 1);
    output_.triggerLoadFailed("loading failed");
    QTest::qWait(10);
    EXPECT_EQ(label->toolTip(), "Cannot play Short: loading failed");
    EXPECT_EQ(label->text(), label->toolTip());
}

TEST_F(MainWindowNavidromeTest, ResolverIsUnregisteredWithTheWindow) {
    seed(fakeServerConfig());
    createWindow();
    window_.reset();
    PModel model = player_->createReadOnlyPlaylist();
    model->addTracks({song("so-1", "First")});
    // A load would make the player ask the (destroyed) window for the next track
    output_.setLoadOnPlay(false);
    play(model, 0);
    // Not EXPECT_EQ: a resolved source would carry the token
    EXPECT_TRUE(output_.currentSource() == "navidrome:so-1");
}

TEST_F(MainWindowNavidromeTest, IncompleteConfigSendsNothing) {
    NavidromeConfig config = testConfig(server_.url());
    config.token.clear();
    seed(config, {"so-1 1700000000000"});
    createWindow();
    QTest::qWait(200);
    EXPECT_TRUE(server_.requests().isEmpty());
}

// The client is configured before the library exists: no account change drops the cache
TEST_F(MainWindowNavidromeTest, StartupKeepsTheLibraryCache) {
    seed(fakeServerConfig());
    QString cache = settingsDirFilePath("navidrome_library");
    ASSERT_TRUE(cache.startsWith(dir_.path()));
    QFile file(cache);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("cache");
    file.close();
    createWindow();
    QTest::qWait(50);
    EXPECT_TRUE(QFile::exists(cache));
    EXPECT_TRUE(server_.requests().isEmpty());
}

TEST_F(MainWindowNavidromeTest, PendingScrobblesAreRetriedAfterStartup) {
    seed(testConfig(server_.url()), {"so-1 1700000000000"});
    server_.setResponse("scrobble", FakeSubsonicServer::ok());
    createWindow();
    ASSERT_TRUE(QTest::qWaitFor([&] { return server_.requests().size() == 1; }, 5000));
    const FakeSubsonicServer::Request &request = server_.requests().first();
    EXPECT_EQ(request.method, "scrobble");
    EXPECT_EQ(request.value("id"), "so-1");
    EXPECT_EQ(request.value("submission"), "true");
    EXPECT_TRUE(QTest::qWaitFor(
        [&] { return !QSettings().contains("navidrome/pendingScrobbles"); }, 5000));
}

TEST_F(MainWindowNavidromeTest, PreferencesActionOpensOneDialog) {
    createWindow();
    QAction *action = window_->findChild<QAction *>("navidromePreferencesAction");
    ASSERT_NE(action, nullptr);
    EXPECT_EQ(action->text(), "&Navidrome…");
    action->trigger();
    action->trigger();
    QList<NavidromePreferences *> dialogs = window_->findChildren<NavidromePreferences *>();
    ASSERT_EQ(dialogs.size(), 1);
    EXPECT_TRUE(dialogs[0]->isVisible());
    // Closing it allows a new one
    dialogs[0]->reject();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_TRUE(window_->findChildren<NavidromePreferences *>().isEmpty());
    action->trigger();
    EXPECT_EQ(window_->findChildren<NavidromePreferences *>().size(), 1);
    // Still open when the window is destroyed
}

TEST_F(MainWindowNavidromeTest, SavingThePreferencesEnablesPlayback) {
    createWindow();
    PModel model = playlist({song("so-1", "First")});
    play(model, 0);
    EXPECT_EQ(status(), QString("Cannot play First: ") + kNotConfigured);

    window_->findChild<QAction *>("navidromePreferencesAction")->trigger();
    auto *dialog = window_->findChild<NavidromePreferences *>();
    ASSERT_NE(dialog, nullptr);
    dialog->findChild<QLineEdit *>("serverUrl")->setText(server_.url());
    dialog->findChild<QLineEdit *>("username")->setText("listener");
    dialog->findChild<QLineEdit *>("password")->setText("sesame");
    dialog->findChild<QCheckBox *>("scrobble")->setChecked(false);
    dialog->findChild<QDialogButtonBox *>("buttonBox")->button(QDialogButtonBox::Ok)->click();
    // The error is gone; the new account's library is being fetched for its new tab
    EXPECT_EQ(status(), "Navidrome: syncing…");

    output_.setLoadOnPlay(false);
    play(model, 0);
    QUrl source(output_.currentSource());
    EXPECT_EQ(source.path(), "/rest/stream");
    EXPECT_EQ(QUrlQuery(source).queryItemValue("id"), "so-1");
    QSettings settings;
    EXPECT_TRUE(NavidromeConfig::load(settings).isComplete());
}

TEST(ElidedLabelTest, TooltipShowsTheTextAsIs) {
    ElidedLabel label(500);
    label.setFullText("Simon & Garfunkel");
    EXPECT_EQ(label.toolTip(), "Simon & Garfunkel");
    // Tooltips that look like HTML are shown as rich text
    QString text = "Cannot play <b>Bold</b> & <i>Co</i>: loading failed";
    label.setFullText(text);
    EXPECT_EQ(label.textFormat(), Qt::PlainText);
    QString tooltip = label.toolTip();
    ASSERT_TRUE(Qt::mightBeRichText(tooltip));
    QTextDocument shown;
    shown.setHtml(tooltip);
    EXPECT_EQ(shown.toPlainText(), text);
    EXPECT_FALSE(tooltip.contains("<b>"));
}

TEST(DisplayTextTest, FormatDuration) {
    EXPECT_EQ(formatDuration(0), "0:00");
    EXPECT_EQ(formatDuration(5), "0:05");
    EXPECT_EQ(formatDuration(61), "1:01");
    EXPECT_EQ(formatDuration(245), "4:05");
    // No hours
    EXPECT_EQ(formatDuration(3659), "60:59");
}

TEST(DisplayTextTest, CountText) {
    EXPECT_EQ(countText(1, "track", "tracks"), "1 track");
    EXPECT_EQ(countText(0, "track", "tracks"), "0 tracks");
    EXPECT_EQ(countText(2, "album", "albums"), "2 albums");
    EXPECT_EQ(countText(12345, "song", "songs"), QLocale().toString(12345) + " songs");
}

TEST(DisplayTextTest, DisplayTitle) {
    Track track;
    track.location = "/music/Artist/01 - Song.live.flac";
    EXPECT_EQ(track.displayTitle(), "01 - Song.live");
    track.metadata["_path"] = "Artist/Album/02 - Other.mp3";
    EXPECT_EQ(track.displayTitle(), "02 - Other");
    track.metadata["title"] = "Title";
    EXPECT_EQ(track.displayTitle(), "Title");
    Track url;
    url.location = "http://host/stream?u=user&t=ab/cd&s=ef/gh";
    EXPECT_FALSE(url.displayTitle().contains("cd"));
    EXPECT_FALSE(url.displayTitle().contains("gh"));
}

namespace {

QJsonObject songJson(const QString &id, const QString &title) {
    // No artist: a track with an artist and a title that starts playing looks up lyrics
    return QJsonObject{{"id", id},
                       {"title", title},
                       {"album", "Album"},
                       {"suffix", "flac"},
                       {"duration", 200},
                       {"path", "Album/" + id + ".flac"}};
}

QStringList locations(PlaylistModel *model) { return locationsOf(model->playlist().tracks); }

QByteArray playlistData(const QList<PTrack> &tracks) {
    Playlist playlist;
    playlist.tracks = tracks;
    QByteArray data;
    playlist.serialize(data);
    return data;
}

} // namespace

// The fake server holds a library of `songs_`.
class NavidromeTabsTest : public MainWindowNavidromeTest {
protected:
    void SetUp() override {
        MainWindowNavidromeTest::SetUp();
        setSongs({"so-1", "so-2", "so-3"});
        server_.setHandler([this](const FakeSubsonicServer::Request &request) {
            return respond(request);
        });
    }

    FakeSubsonicServer::Response respond(const FakeSubsonicServer::Request &request) {
        if (request.method == "getScanStatus") {
            QJsonObject status{{"scanning", false},
                               {"count", songs_.size()},
                               {"folderCount", 1},
                               {"lastScan", lastScan_}};
            return FakeSubsonicServer::ok({{"scanStatus", status}});
        }
        if (request.method != "search3")
            return FakeSubsonicServer::http(404);
        if (!searchResponse_.body.isEmpty() || searchResponse_.hold)
            return searchResponse_;
        int offset = request.value("songOffset").toInt();
        int count = request.value("songCount").toInt();
        QJsonArray page;
        for (int i = offset; i < qMin(offset + count, songs_.size()); ++i)
            page.append(songs_[i]);
        return FakeSubsonicServer::ok({{"searchResult3", QJsonObject{{"song", page}}}});
    }

    void setSongs(const QStringList &ids) {
        songs_.clear();
        for (const QString &id : ids)
            songs_.append(songJson(id, "Song " + id));
    }

    // Writes the library cache of fakeServerConfig() holding `songs_`, as an earlier run did.
    void seedCache() {
        NavidromeClient client;
        client.setConfig(fakeServerConfig());
        NavidromeLibrary library(client, QString());
        QSignalSpy finished(&library, &NavidromeLibrary::syncFinished);
        library.sync();
        ASSERT_TRUE(QTest::qWaitFor([&] { return finished.count() == 1; }, 5000));
        ASSERT_TRUE(QFile::exists(settingsDirFilePath("navidrome_library")));
        seededRequests_ = server_.requests().size();
    }

    // Tabs saved by an earlier run; an invalid QVariant in `data` is a read-only view.
    static void seedTabs(const QVariantList &names,
                         const QVariantList &data,
                         const QStringList &kinds = QStringList()) {
        QSettings settings;
        settings.setValue("mainwindow/saveTabs", true);
        settings.setValue("mainwindow/tabsNames", names);
        settings.setValue("mainwindow/tabsData", data);
        if (!kinds.isEmpty())
            settings.setValue("mainwindow/tabsKinds", kinds);
    }

    // Methods of the requests the window sent (seedCache() ones excluded).
    QStringList requested() const {
        QStringList methods;
        for (int i = seededRequests_; i < server_.requests().size(); ++i)
            methods << server_.requests()[i].method;
        return methods;
    }

    QTabWidget *tabs() { return window_->findChild<QTabWidget *>("playlistTabs"); }

    PlaylistTab *tab(int index) { return qobject_cast<PlaylistTab *>(tabs()->widget(index)); }

    QStringList tabNames() {
        QStringList names;
        for (int i = 0; i < tabs()->count(); ++i)
            names << tabs()->tabText(i);
        return names;
    }

    // The model shown by tab `index`
    PlaylistModel *model(int index) {
        auto *view = tab(index)->findChild<QAbstractItemView *>("playlist");
        auto *filter = qobject_cast<QSortFilterProxyModel *>(view->model());
        return qobject_cast<PlaylistModel *>(filter->sourceModel());
    }

    QTimer *updateTimer() { return window_->findChild<QTimer *>("NavidromeUpdateTimer"); }

    bool waitForLocations(int tabIndex, const QStringList &expected) {
        return QTest::qWaitFor([&] { return locations(model(tabIndex)) == expected; }, 5000);
    }

    QList<QJsonObject> songs_;
    QString lastScan_ = "2026-01-01T00:00:00Z";
    // Answers every search3 if set
    FakeSubsonicServer::Response searchResponse_;
    int seededRequests_ = 0;
};

TEST_F(NavidromeTabsTest, TabsKeepTheirKindsAcrossRestarts) {
    seed(fakeServerConfig());
    QSettings().setValue("mainwindow/saveTabs", true);
    ASSERT_NO_FATAL_FAILURE(seedCache());
    createWindow();
    ASSERT_EQ(tabs()->count(), 0);
    playlist({song("so-9", "Mix")});
    action("newLibraryViewAction")->trigger();
    action("newNavidromeLibraryViewAction")->trigger();
    ASSERT_EQ(tabNames(), (QStringList{"Test", "All", "Navidrome"}));
    EXPECT_EQ(tabs()->currentIndex(), 2);
    // Filled from the cache; no sync started
    EXPECT_EQ(status(), "");
    PModel navidromeModel = window_->getActivePlaylistModel();
    EXPECT_EQ(locations(navidromeModel.get()), songLocations({"so-1", "so-2", "so-3"}));
    player_->setLastPlayed(navidromeModel, navidromeModel->index(1, 0));
    window_->close();
    QSettings settings;
    EXPECT_EQ(settings.value("mainwindow/tabsKinds").toStringList(),
              (QStringList{"playlist", "library", "navidrome"}));
    EXPECT_FALSE(settings.value("mainwindow/tabsData").toList().value(2).isValid());
    window_.reset();

    createWindow();
    ASSERT_EQ(tabNames(), (QStringList{"Test", "All", "Navidrome"}));
    // The cached tracks were there in time to restore the last played track
    EXPECT_EQ(tabs()->currentIndex(), 2);
    auto lastPlayed = player_->getLastPlayed();
    EXPECT_EQ(lastPlayed.first.get(), model(2));
    ASSERT_TRUE(lastPlayed.second.isValid());
    EXPECT_EQ(lastPlayed.second.data(TrackRole).value<PTrack>()->location,
              navidrome::locationForSong("so-2"));
    EXPECT_TRUE(tab(0)->isEditable());
    EXPECT_EQ(locations(model(0)), songLocations({"so-9"}));
    EXPECT_FALSE(tab(1)->isEditable());
    EXPECT_EQ(locations(model(1)), QStringList());
    EXPECT_FALSE(tab(2)->isEditable());
    EXPECT_EQ(locations(model(2)), songLocations({"so-1", "so-2", "so-3"}));
    EXPECT_TRUE(updateTimer()->isActive());
}

TEST_F(NavidromeTabsTest, SettingsWithoutKindsRestoreLibraryViews) {
    seed(fakeServerConfig());
    ASSERT_NO_FATAL_FAILURE(seedCache());
    seedTabs({"Mix", "All", "Navidrome"}, {playlistData({song("so-9", "Mix")}), QVariant(), {}});
    createWindow();
    ASSERT_EQ(tabNames(), (QStringList{"Mix", "All", "Navidrome"}));
    EXPECT_TRUE(tab(0)->isEditable());
    EXPECT_EQ(locations(model(0)), songLocations({"so-9"}));
    // Both show the (empty) local library
    EXPECT_EQ(locations(model(1)), QStringList());
    EXPECT_EQ(locations(model(2)), QStringList());
    EXPECT_FALSE(updateTimer()->isActive());
    QTest::qWait(100);
    EXPECT_EQ(requested(), QStringList());
}

TEST_F(NavidromeTabsTest, KindsAreUsedOnlyForTabsWithoutData) {
    seed(fakeServerConfig());
    ASSERT_NO_FATAL_FAILURE(seedCache());
    seedTabs({"Mix", "Navidrome"}, {playlistData({song("so-9", "Mix")}), QVariant()},
             {"navidrome", "navidrome"});
    createWindow();
    ASSERT_EQ(tabNames(), (QStringList{"Mix", "Navidrome"}));
    EXPECT_TRUE(tab(0)->isEditable());
    EXPECT_EQ(locations(model(0)), songLocations({"so-9"}));
    EXPECT_EQ(locations(model(1)), songLocations({"so-1", "so-2", "so-3"}));
    window_.reset();

    // Kinds that do not match the tabs are ignored
    seedTabs({"Mix", "Navidrome"}, {playlistData({song("so-9", "Mix")}), QVariant()},
             {"playlist", "navidrome", "library"});
    createWindow();
    ASSERT_EQ(tabNames(), (QStringList{"Mix", "Navidrome"}));
    EXPECT_EQ(locations(model(0)), songLocations({"so-9"}));
    EXPECT_EQ(locations(model(1)), QStringList());
}

TEST_F(NavidromeTabsTest, NavidromeTabsFollowTheLibrary) {
    seed(fakeServerConfig());
    createWindow();
    ASSERT_EQ(tabNames(), QStringList{"All"});
    // The library is empty: the first tab syncs it
    action("newNavidromeLibraryViewAction")->trigger();
    ASSERT_TRUE(waitForLocations(1, songLocations({"so-1", "so-2", "so-3"})));
    ASSERT_TRUE(QTest::qWaitFor([&] { return status() == "Navidrome: 3 tracks"; }, 5000));
    action("newNavidromeLibraryViewAction")->trigger();
    ASSERT_EQ(tabNames(), (QStringList{"All", "Navidrome", "Navidrome"}));
    EXPECT_EQ(locations(model(2)), songLocations({"so-1", "so-2", "so-3"}));
    EXPECT_EQ(requested().count("search3"), 1);

    // One changed, one removed, one added
    setSongs({"so-1", "so-2", "so-4"});
    songs_[1] = songJson("so-2", "Renamed");
    action("refreshNavidromeLibraryAction")->trigger();
    for (int i : {1, 2}) {
        ASSERT_TRUE(waitForLocations(i, songLocations({"so-1", "so-2", "so-4"})));
        EXPECT_EQ(model(i)->playlist().tracks[1]->metadata.value("title"), "Renamed");
    }
    EXPECT_TRUE(locations(model(0)).isEmpty());

    // Another account: the tabs are emptied, then filled from its library
    action("navidromePreferencesAction")->trigger();
    ASSERT_NO_FATAL_FAILURE(savePreferences(server_.url(), "someone", "secret"));
    EXPECT_TRUE(locations(model(1)).isEmpty());
    EXPECT_TRUE(locations(model(2)).isEmpty());
    for (int i : {1, 2})
        ASSERT_TRUE(waitForLocations(i, songLocations({"so-1", "so-2", "so-4"})));
    EXPECT_EQ(server_.requests().last().method, "search3");
    EXPECT_EQ(server_.requests().last().value("u"), "someone");
    EXPECT_EQ(tabNames(), (QStringList{"All", "Navidrome", "Navidrome"}));

    // Closing a tab leaves the other one updated
    window_->removePlaylistTab(1);
    EXPECT_TRUE(updateTimer()->isActive());
    setSongs({"so-1"});
    action("refreshNavidromeLibraryAction")->trigger();
    ASSERT_TRUE(waitForLocations(1, songLocations({"so-1"})));
    window_->removePlaylistTab(1);
    EXPECT_FALSE(updateTimer()->isActive());
}

// Without saved tabs, the menu is how a Navidrome tab is made
TEST_F(NavidromeTabsTest, NewNavidromeViewChecksTheCacheForUpdates) {
    seed(fakeServerConfig());
    ASSERT_NO_FATAL_FAILURE(seedCache());
    lastScan_ = "2026-02-01T00:00:00Z";
    setSongs({"so-1", "so-2", "so-3", "so-4"});
    createWindow();
    action("newNavidromeLibraryViewAction")->trigger();
    ASSERT_EQ(tabNames(), (QStringList{"All", "Navidrome"}));
    EXPECT_EQ(locations(model(1)), songLocations({"so-1", "so-2", "so-3"}));
    ASSERT_TRUE(QTest::qWaitFor([&] { return !requested().isEmpty(); }, 1000));
    EXPECT_EQ(requested().first(), "getScanStatus");
    EXPECT_TRUE(waitForLocations(1, songLocations({"so-1", "so-2", "so-3", "so-4"})));
    // The sync starts with the scan status of the check
    EXPECT_EQ(requested(), (QStringList{"getScanStatus", "search3"}));
}

TEST_F(NavidromeTabsTest, NewNavidromeViewAsksForTheServerFirst) {
    createWindow();
    QAction *newView = action("newNavidromeLibraryViewAction");
    EXPECT_EQ(newView->text(), "New &Navidrome library view");
    newView->trigger();
    auto *dialog = window_->findChild<NavidromePreferences *>();
    ASSERT_NE(dialog, nullptr);
    EXPECT_TRUE(dialog->isVisible());
    EXPECT_EQ(tabNames(), QStringList{"All"});
    dialog->reject();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_EQ(tabNames(), QStringList{"All"});

    newView->trigger();
    ASSERT_NO_FATAL_FAILURE(savePreferences(server_.url(), "listener", "sesame"));
    ASSERT_EQ(tabNames(), (QStringList{"All", "Navidrome"}));
    EXPECT_EQ(tabs()->currentIndex(), 1);
    EXPECT_FALSE(tab(1)->isEditable());
    EXPECT_TRUE(waitForLocations(1, songLocations({"so-1", "so-2", "so-3"})));
    EXPECT_TRUE(updateTimer()->isActive());
}

// E.g. after the server URL was cleared
TEST_F(NavidromeTabsTest, RestoredTabWaitsForAnAccount) {
    seedTabs({"Navidrome"}, {QVariant()}, {"navidrome"});
    createWindow();
    ASSERT_EQ(tabNames(), QStringList{"Navidrome"});
    EXPECT_FALSE(tab(0)->isEditable());
    EXPECT_EQ(locations(model(0)), QStringList());
    QMetaObject::invokeMethod(updateTimer(), "timeout");
    QTest::qWait(100);
    EXPECT_EQ(requested(), QStringList());
    EXPECT_EQ(status(), "");
    // The periodic save keeps its kind
    QSettings().remove("mainwindow/tabsKinds");
    QMetaObject::invokeMethod(window_.get(), "timeoutFired");
    EXPECT_EQ(QSettings().value("mainwindow/tabsKinds").toStringList(), QStringList{"navidrome"});

    // The new view asked for is added next to it (navidromeTabRequested_: a tab exists already)
    action("newNavidromeLibraryViewAction")->trigger();
    ASSERT_NO_FATAL_FAILURE(savePreferences(server_.url(), "listener", "sesame"));
    ASSERT_EQ(tabNames(), (QStringList{"Navidrome", "Navidrome"}));
    EXPECT_EQ(tabs()->currentIndex(), 1);
    for (int i : {0, 1})
        EXPECT_TRUE(waitForLocations(i, songLocations({"so-1", "so-2", "so-3"})));
    EXPECT_EQ(requested().count("search3"), 1);
}

TEST_F(NavidromeTabsTest, ConfiguringAnAccountAddsANavidromeTab) {
    createWindow();
    action("navidromePreferencesAction")->trigger();
    ASSERT_NO_FATAL_FAILURE(savePreferences(server_.url(), "listener", "sesame"));
    ASSERT_EQ(tabNames(), (QStringList{"All", "Navidrome"}));
    EXPECT_EQ(tabs()->currentIndex(), 1);
    EXPECT_TRUE(waitForLocations(1, songLocations({"so-1", "so-2", "so-3"})));

    // Saving the same account changes nothing
    window_->removePlaylistTab(1);
    int requests = server_.requests().size();
    action("navidromePreferencesAction")->trigger();
    ASSERT_NO_FATAL_FAILURE(savePreferences(server_.url(), "listener", ""));
    EXPECT_EQ(tabNames(), QStringList{"All"});
    QTest::qWait(100);
    EXPECT_EQ(server_.requests().size(), requests);
}

TEST_F(NavidromeTabsTest, NewPasswordSyncsAnEmptyLibrary) {
    createWindow();
    searchResponse_ = FakeSubsonicServer::failed(40, "Wrong username or password");
    action("navidromePreferencesAction")->trigger();
    ASSERT_NO_FATAL_FAILURE(savePreferences(server_.url(), "listener", "wrong"));
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return status() == "Navidrome: sync failed: Wrong username or password"; }, 5000));
    ASSERT_EQ(tabNames(), (QStringList{"All", "Navidrome"}));

    // The same account
    searchResponse_ = FakeSubsonicServer::Response();
    action("navidromePreferencesAction")->trigger();
    ASSERT_NO_FATAL_FAILURE(savePreferences(server_.url(), "listener", "sesame"));
    EXPECT_TRUE(waitForLocations(1, songLocations({"so-1", "so-2", "so-3"})));
    EXPECT_EQ(tabNames(), (QStringList{"All", "Navidrome"}));

    // A filled library is not fetched again
    ASSERT_TRUE(QTest::qWaitFor([&] { return status() == "Navidrome: 3 tracks"; }, 5000));
    int requests = server_.requests().size();
    action("navidromePreferencesAction")->trigger();
    ASSERT_NO_FATAL_FAILURE(savePreferences(server_.url(), "listener", ""));
    EXPECT_NE(status(), "Navidrome: syncing…");
    QTest::qWait(100);
    EXPECT_EQ(server_.requests().size(), requests);
}

TEST_F(NavidromeTabsTest, RefreshNeedsAConfiguredAccount) {
    createWindow();
    QAction *refresh = action("refreshNavidromeLibraryAction");
    EXPECT_EQ(refresh->text(), "Refresh Navidrome library");
    EXPECT_FALSE(refresh->isEnabled());
    refresh->trigger();
    QTest::qWait(100);
    EXPECT_EQ(requested(), QStringList());

    action("navidromePreferencesAction")->trigger();
    ASSERT_NO_FATAL_FAILURE(savePreferences(server_.url(), "listener", "sesame"));
    EXPECT_TRUE(refresh->isEnabled());
    ASSERT_TRUE(QTest::qWaitFor([&] { return status() == "Navidrome: 3 tracks"; }, 5000));
    refresh->trigger();
    EXPECT_EQ(status(), "Navidrome: syncing…");
    ASSERT_TRUE(QTest::qWaitFor([&] { return requested().count("search3") == 2; }, 5000));

    action("navidromePreferencesAction")->trigger();
    ASSERT_NO_FATAL_FAILURE(savePreferences("", "", ""));
    EXPECT_FALSE(refresh->isEnabled());
}

TEST_F(NavidromeTabsTest, StartupSyncsAnEmptyLibraryRightAway) {
    seed(fakeServerConfig());
    seedTabs({"Navidrome"}, {QVariant()}, {"navidrome"});
    createWindow();
    // Started by the constructor
    EXPECT_EQ(status(), "Navidrome: syncing…");
    EXPECT_TRUE(waitForLocations(0, songLocations({"so-1", "so-2", "so-3"})));
    EXPECT_EQ(requested(), (QStringList{"getScanStatus", "search3"}));
}

TEST_F(NavidromeTabsTest, StartupChecksACachedLibraryForUpdates) {
    seed(fakeServerConfig());
    ASSERT_NO_FATAL_FAILURE(seedCache());
    seedTabs({"Navidrome"}, {QVariant()}, {"navidrome"});
    QElapsedTimer elapsed;
    elapsed.start();
    createWindow();
    EXPECT_EQ(locations(model(0)), songLocations({"so-1", "so-2", "so-3"}));
    EXPECT_EQ(status(), "");
    ASSERT_TRUE(QTest::qWaitFor([&] { return !requested().isEmpty(); }, 5000));
    EXPECT_GE(elapsed.elapsed(), 2500);
    EXPECT_EQ(requested(), QStringList{"getScanStatus"});
    // The scan status matches the cache
    QTest::qWait(200);
    EXPECT_EQ(requested(), QStringList{"getScanStatus"});

    // Then hourly
    QTimer *timer = updateTimer();
    ASSERT_NE(timer, nullptr);
    EXPECT_TRUE(timer->isActive());
    EXPECT_EQ(timer->interval(), 60 * 60 * 1000);
    lastScan_ = "2026-02-01T00:00:00Z";
    setSongs({"so-1", "so-2"});
    QMetaObject::invokeMethod(timer, "timeout");
    EXPECT_TRUE(waitForLocations(0, songLocations({"so-1", "so-2"})));
    EXPECT_EQ(requested(), (QStringList{"getScanStatus", "getScanStatus", "search3"}));

    window_->removePlaylistTab(0);
    EXPECT_FALSE(timer->isActive());
}

TEST_F(NavidromeTabsTest, StatusShowsTheSyncState) {
    seed(fakeServerConfig());
    createWindow();
    searchResponse_ = FakeSubsonicServer::held();
    action("refreshNavidromeLibraryAction")->trigger();
    EXPECT_EQ(status(), "Navidrome: syncing…");
    ASSERT_TRUE(QTest::qWaitFor([&] { return requested().contains("search3"); }, 5000));
    EXPECT_EQ(status(), "Navidrome: syncing…");
    searchResponse_ = FakeSubsonicServer::Response();
    server_.release(FakeSubsonicServer::ok(
        {{"searchResult3", QJsonObject{{"song", QJsonArray{songJson("so-1", "Only")}}}}}));
    ASSERT_TRUE(QTest::qWaitFor([&] { return status() == "Navidrome: 1 track"; }, 5000));
    QElapsedTimer elapsed;
    elapsed.start();
    // Shown for a while only
    ASSERT_TRUE(QTest::qWaitFor([&] { return status().isEmpty(); }, 7000));
    EXPECT_GE(elapsed.elapsed(), 4000);

    searchResponse_ = FakeSubsonicServer::failed(0, "Library unavailable");
    action("refreshNavidromeLibraryAction")->trigger();
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return status() == "Navidrome: sync failed: Library unavailable"; }, 5000));

    // Dropping the account drops a running sync and its status
    searchResponse_ = FakeSubsonicServer::held();
    action("refreshNavidromeLibraryAction")->trigger();
    EXPECT_EQ(status(), "Navidrome: syncing…");
    action("navidromePreferencesAction")->trigger();
    ASSERT_NO_FATAL_FAILURE(savePreferences("", "", ""));
    EXPECT_EQ(status(), "");
}

TEST_F(NavidromeTabsTest, SyncErrorsStayWhenATrackPlays) {
    seed(fakeServerConfig());
    createWindow();
    searchResponse_ = FakeSubsonicServer::failed(0, "Library unavailable");
    action("refreshNavidromeLibraryAction")->trigger();
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return status() == "Navidrome: sync failed: Library unavailable"; }, 5000));
    PModel model = playlist({song("so-9", "Mix")});
    play(model, 0);
    ASSERT_TRUE(player_->getCurrentTrack());
    EXPECT_EQ(status(), "Navidrome: sync failed: Library unavailable");

    // Until the next sync
    searchResponse_ = FakeSubsonicServer::Response();
    action("refreshNavidromeLibraryAction")->trigger();
    EXPECT_EQ(status(), "Navidrome: syncing…");
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    QApplication app(argc, argv);
    return RUN_ALL_TESTS();
}
