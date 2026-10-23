#include "library/track.h"
#include "navidrome/tests/fakesubsonicserver.h"
#include "navidrome/tests/testsupport.h"
#include "player/mpvaudiooutput.h"
#include "ui/audioplayer.h"
#include "ui/mainwindow.h"
#include "ui/playlistmodel.h"
#include "gtest/gtest.h"
#include <QApplication>
#include <QDataStream>
#include <QFile>
#include <QStringList>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <algorithm>
#include <atomic>

namespace {

class EnvironmentOverride {
public:
    EnvironmentOverride(const char *name, const QByteArray &value)
        : name_(name), wasSet_(qEnvironmentVariableIsSet(name)), old_(qgetenv(name)) {
        qputenv(name, value);
    }
    ~EnvironmentOverride() {
        if (wasSet_)
            qputenv(name_, old_);
        else
            qunsetenv(name_);
    }

private:
    const char *name_;
    bool wasSet_;
    QByteArray old_;
};

// Writes `ms` of silence as mono 16-bit PCM.
bool writeSilentWav(const QString &path, int ms) {
    const quint32 rate = 8000;
    const quint32 dataSize = rate * ms / 1000 * 2;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData("RIFF", 4);
    out << quint32(36 + dataSize);
    out.writeRawData("WAVEfmt ", 8);
    out << quint32(16) << quint16(1) << quint16(1) << rate << quint32(rate * 2) << quint16(2)
        << quint16(16);
    out.writeRawData("data", 4);
    out << dataSize;
    out.writeRawData(QByteArray(int(dataSize), '\0').constData(), int(dataSize));
    return out.status() == QDataStream::Ok;
}

bool usesNullAudioOutput(const MpvAudioOutput &output) {
    const QVariantList ao = output.get_property("ao").toList();
    return ao.size() == 1 && ao[0].toMap().value("name").toString() == "null";
}

PTrack trackAt(const QString &location) {
    auto track = std::make_shared<Track>();
    track->location = location;
    return track;
}

// Records the output's signals in the order they are delivered.
class SignalRecorder {
public:
    explicit SignalRecorder(AudioOutput &output) {
        QObject::connect(&output, &AudioOutput::currentSourceChanged, &context_, [this] {
            events.append("currentSourceChanged");
        });
        QObject::connect(
            &output, &AudioOutput::finished, &context_, [this] { events.append("finished"); });
        QObject::connect(&output, &AudioOutput::loadFailed, &context_, [this](QString) {
            events.append("loadFailed");
        });
    }

    bool waitFor(const QString &event) {
        return QTest::qWaitFor([&]() { return events.contains(event); }, 5000);
    }

    // The idle output reports itself (finished()) once at startup, possibly before this recorder
    // was connected. Lets that report pass so it can't be taken for the end of a later load.
    void skipStartupReport() {
        if (QTest::qWaitFor([&]() { return events.contains("finished"); }, 1000))
            events.clear();
    }

    QStringList events;

private:
    QObject context_;
};

} // namespace

// libmpv only ever runs with the null audio output.
class MpvAudioOutputTest : public testing::Test {
protected:
    MpvAudioOutputTest() : ao_("FUBAR_MPV_AO", "null") {
        qRegisterMetaType<AudioState>("AudioState");
    }

    void SetUp() override { ASSERT_TRUE(dir_.isValid()); }

    EnvironmentOverride ao_;
    QTemporaryDir dir_;
};

class AudioPlayerWithMpvTest : public MpvAudioOutputTest {};

TEST_F(MpvAudioOutputTest, LogMessageRedactsCredentials) {
    EXPECT_EQ(MpvAudioOutput::logMessage(
                  "stream", "Failed to open http://h/rest/stream?u=me&t=SECRET&id=1&s=SALT.\n"),
              "mpv [stream] Failed to open http://h/rest/stream?u=***&t=***&id=1&s=***");
    EXPECT_EQ(MpvAudioOutput::logMessage("cplayer", "Playing: /music/a.flac\n"),
              "mpv [cplayer] Playing: /music/a.flac");
}

TEST_F(MpvAudioOutputTest, DebugStringRedactsCredentials) {
    QString text = MpvAudioOutput::debugString(
        QVariantList{"loadfile", "http://h/rest/stream?u=me&t=SECRET&s=SALT", "append"});
    EXPECT_TRUE(text.contains("loadfile")) << qPrintable(text);
    EXPECT_TRUE(text.contains("/rest/stream?u=***&t=***&s=***")) << qPrintable(text);
    EXPECT_FALSE(text.contains("SECRET"));
    EXPECT_FALSE(text.contains("SALT"));
}

// Stream URLs carry credentials.
TEST_F(MpvAudioOutputTest, TlsCertificatesAreVerified) {
    MpvAudioOutput output;
    const QVariant verify = output.get_property("tls-verify");
    EXPECT_EQ(verify.userType(), int(QMetaType::Bool));
    EXPECT_TRUE(verify.toBool());
}

// Against a local server answering 404.
TEST_F(MpvAudioOutputTest, FailedSubsonicStreamIsReportedOnceRedactedAndNotHandedToYoutubeDl) {
    // Every name mpv's ytdl hook looks for records its arguments and fails
    const QString record = dir_.filePath("invocations");
    for (const char *name : {"yt-dlp", "yt-dlp_x86", "youtube-dl"}) {
        QFile script(dir_.filePath(name));
        ASSERT_TRUE(script.open(QIODevice::WriteOnly));
        script.write("#!/bin/sh\nprintf '%s\\n' \"$*\" >> \"" + QFile::encodeName(record) +
                     "\"\nexit 1\n");
        script.close();
        script.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    }
    EnvironmentOverride path("PATH", QFile::encodeName(dir_.path()) + ":" + qgetenv("PATH"));

    FakeSubsonicServer server;
    ASSERT_TRUE(server.listen());
    const QString stream =
        server.url() + "/rest/stream?u=me&t=SECRET&v=1.16.1&c=fubar&id=song-1&s=SALTLAST";
    const QString control = server.url() + "/watch?v=control";

    QStringList failures;
    bool nullAudioOutput = false;
    QStringList logged;
    {
        MessageCapture capture;
        MpvAudioOutput output;
        nullAudioOutput = usesNullAudioOutput(output);
        if (nullAudioOutput) {
            QObject receiver;
            QObject::connect(&output, &AudioOutput::loadFailed, &receiver, [&](QString reason) {
                failures.append(reason);
            });
            output.setCurrentSource(stream);
            EXPECT_TRUE(QTest::qWaitFor([&]() { return failures.size() == 1; }, 5000));
            QTest::qWait(500);
            EXPECT_EQ(failures.size(), 1);
            // Other http URLs still go through the ytdl hook
            output.setCurrentSource(control);
            EXPECT_TRUE(QTest::qWaitFor([&]() { return failures.size() == 2; }, 5000));
            QTest::qWait(500);
        }
        logged = capture.messages();
    }
    ASSERT_TRUE(nullAudioOutput);

    QStringList invocations;
    QFile recorded(record);
    if (recorded.open(QIODevice::ReadOnly))
        invocations = QString::fromUtf8(recorded.readAll()).split('\n', Qt::SkipEmptyParts);
    auto invoked = [&](const QString &needle) {
        return std::any_of(invocations.begin(), invocations.end(), [&](const QString &line) {
            return line.contains(needle);
        });
    };
    EXPECT_TRUE(invoked("/watch?v=control"));
    EXPECT_FALSE(invoked("/rest/stream"));
    EXPECT_FALSE(invoked("SECRET"));

    auto streamRequests =
        std::count_if(server.requests().begin(), server.requests().end(),
                      [](const FakeSubsonicServer::Request &request) {
                          return request.path == "/rest/stream";
                      });
    EXPECT_EQ(streamRequests, 1);

    ASSERT_EQ(failures.size(), 2);
    for (const QString &reason : failures) {
        EXPECT_FALSE(reason.isEmpty());
        EXPECT_FALSE(reason.contains("http")) << qPrintable(reason);
    }

    bool redactedUrlLogged = false;
    for (const QString &message : logged) {
        EXPECT_FALSE(message.contains("SECRET")) << qPrintable(message.left(30));
        EXPECT_FALSE(message.contains("SALTLAST")) << qPrintable(message.left(30));
        redactedUrlLogged |= message.contains("/rest/stream?u=***&t=***");
    }
    EXPECT_TRUE(redactedUrlLogged);
}

TEST_F(MpvAudioOutputTest, FinishedFollowsTheEndOfPlaybackAndLoadFailures) {
    const QString silence = dir_.filePath("silence.wav");
    ASSERT_TRUE(writeSilentWav(silence, 500));

    MpvAudioOutput output;
    // Checked before anything is loaded, so a real audio device is never opened
    ASSERT_TRUE(usesNullAudioOutput(output));
    SignalRecorder recorder(output);
    recorder.skipStartupReport();

    output.setCurrentSource(silence);
    output.play();
    ASSERT_TRUE(recorder.waitFor("currentSourceChanged"));
    EXPECT_EQ(output.get_property("current-ao").toString(), "null");
    ASSERT_TRUE(recorder.waitFor("finished"));
    QTest::qWait(300);
    EXPECT_EQ(recorder.events, QStringList({"currentSourceChanged", "finished"}));
    EXPECT_EQ(output.state(), AudioState::Stopped);

    recorder.events.clear();
    output.setCurrentSource(dir_.filePath("missing.wav"));
    ASSERT_TRUE(recorder.waitFor("finished"));
    QTest::qWait(300);
    EXPECT_EQ(recorder.events, QStringList({"loadFailed", "finished"}));
    EXPECT_EQ(output.state(), AudioState::Stopped);
}

// Like a Navidrome server that is slow to answer, then fails.
TEST_F(MpvAudioOutputTest, ASlowFailingStreamIsNeverPlaying) {
    FakeSubsonicServer server;
    ASSERT_TRUE(server.listen());
    server.setHandler([](const FakeSubsonicServer::Request &) {
        return FakeSubsonicServer::held();
    });

    MpvAudioOutput output;
    ASSERT_TRUE(usesNullAudioOutput(output));
    SignalRecorder recorder(output);
    recorder.skipStartupReport();
    QList<AudioState> states;
    QObject context;
    QObject::connect(&output, &AudioOutput::stateChanged, &context, [&](AudioState state) {
        states.append(state);
    });

    // Not handed to youtube-dl
    output.setCurrentSource(server.url() + "/rest/stream?id=song-1");
    output.play();
    ASSERT_TRUE(QTest::qWaitFor([&]() { return server.requests().size() == 1; }, 5000));
    QTest::qWait(1000);
    EXPECT_NE(output.state(), AudioState::Playing);
    server.release(FakeSubsonicServer::http(404));
    ASSERT_TRUE(recorder.waitFor("finished"));
    QTest::qWait(300);
    EXPECT_EQ(recorder.events, QStringList({"loadFailed", "finished"}));
    EXPECT_FALSE(states.contains(AudioState::Playing));
    EXPECT_EQ(output.state(), AudioState::Stopped);
}

// Like a radio .pls/.m3u listing mirrors: a later entry that plays clears the earlier failure.
TEST_F(MpvAudioOutputTest, AFailedEntryFollowedByAPlayableOneIsNotReported) {
    const QString silence = dir_.filePath("silence.wav");
    ASSERT_TRUE(writeSilentWav(silence, 1500));
    const QString list = dir_.filePath("list.m3u");
    QFile m3u(list);
    ASSERT_TRUE(m3u.open(QIODevice::WriteOnly));
    m3u.write(QFile::encodeName(dir_.filePath("missing.wav")) + "\n" +
              QFile::encodeName(silence) + "\n");
    m3u.close();

    MessageCapture capture;
    MpvAudioOutput output;
    ASSERT_TRUE(usesNullAudioOutput(output));
    SignalRecorder recorder(output);
    recorder.skipStartupReport();

    output.setCurrentSource(list);
    output.play();
    ASSERT_TRUE(recorder.waitFor("currentSourceChanged"));
    EXPECT_EQ(output.get_property("path").toString(), silence);
    ASSERT_TRUE(recorder.waitFor("finished"));
    QTest::qWait(300);
    EXPECT_EQ(recorder.events, QStringList({"currentSourceChanged", "finished"}));
    EXPECT_EQ(output.state(), AudioState::Stopped);
    // The first entry did fail
    const QStringList messages = capture.messages();
    EXPECT_TRUE(std::any_of(messages.begin(), messages.end(), [](const QString &message) {
        return message.startsWith("Ended file:");
    }));
}

// The next track is buffered gaplessly; loading it fails once the first one ends.
TEST_F(AudioPlayerWithMpvTest, AFailingNextTrackStopsPlayback) {
    const QString first = dir_.filePath("first.wav");
    const QString third = dir_.filePath("third.wav");
    ASSERT_TRUE(writeSilentWav(first, 2000));
    ASSERT_TRUE(writeSilentWav(third, 500));
    const QList<PTrack> tracks{trackAt(first), trackAt("navidrome:song-2"), trackAt(third)};
    FakeSubsonicServer server;
    ASSERT_TRUE(server.listen());

    MessageCapture capture;
    MpvAudioOutput output;
    ASSERT_TRUE(usesNullAudioOutput(output));
    // The output's startup report (finished()) may arrive after play()
    AudioPlayer player(nullptr, &output, /*testing=*/true);
    MainWindow mainWindow(player);
    player.registerSourceResolver("navidrome:", [&](const QString &location, QString *) {
        return server.url() + "/rest/stream?u=me&t=SECRET&s=SALT&id=" + location.mid(10);
    });
    player.setVolume(0.5);
    QList<PTrack> playing;
    QList<std::pair<PTrack, QString>> errors;
    QObject context;
    QObject::connect(&player, &AudioPlayer::trackPlaying, &context, [&](PTrack track) {
        playing.append(track);
    });
    QObject::connect(
        &player, &AudioPlayer::playbackError, &context, [&](PTrack track, QString reason) {
            errors.append({track, reason});
        });

    PModel model = player.createPlaylist(false);
    mainWindow.addPlaylist(model, "", false);
    model->addTracks(tracks);
    player.play(model, model->index(0, 0));
    ASSERT_TRUE(QTest::qWaitFor([&]() { return !errors.isEmpty(); }, 10000));
    QTest::qWait(500);

    ASSERT_EQ(errors.size(), 1);
    EXPECT_EQ(errors[0].first, tracks[1]);
    EXPECT_FALSE(errors[0].second.isEmpty());
    EXPECT_FALSE(errors[0].second.contains("http")) << qPrintable(errors[0].second);
    ASSERT_FALSE(playing.isEmpty());
    EXPECT_EQ(playing.count(tracks[0]), playing.size());
    EXPECT_EQ(player.getCurrentTrack(), tracks[0]);
    EXPECT_EQ(output.state(), AudioState::Stopped);
    EXPECT_EQ(output.get_property("playlist-count").toInt(), 0);
    auto streamRequests =
        std::count_if(server.requests().begin(), server.requests().end(),
                      [](const FakeSubsonicServer::Request &request) {
                          return request.path == "/rest/stream";
                      });
    EXPECT_EQ(streamRequests, 1);
    for (const QString &message : capture.messages())
        EXPECT_FALSE(message.contains("SECRET")) << qPrintable(message.left(30));
}

TEST_F(AudioPlayerWithMpvTest, VolumeIsAppliedWhenTheFileIsLoaded) {
    const QString silence = dir_.filePath("silence.wav");
    ASSERT_TRUE(writeSilentWav(silence, 3000));
    // Declared before the output, whose destructor stops the thread using them
    std::atomic<double> volumeAtFirstTick{-1};
    QObject context;

    MpvAudioOutput output;
    ASSERT_TRUE(usesNullAudioOutput(output));
    AudioPlayer player(nullptr, &output, /*testing=*/true);
    MainWindow mainWindow(player);
    player.setVolume(0.25);
    QObject::connect(
        &output,
        &AudioOutput::tick,
        &context,
        [&](qint64) {
            if (volumeAtFirstTick < 0)
                volumeAtFirstTick = output.get_property("volume").toDouble();
        },
        Qt::DirectConnection);

    PModel model = player.createPlaylist(false);
    mainWindow.addPlaylist(model, "", false);
    model->addTracks({trackAt(silence)});
    player.play(model, model->index(0, 0));
    // The output applies the volume on its own, even while this thread is busy
    QThread::msleep(1000);
    ASSERT_TRUE(QTest::qWaitFor([&]() { return volumeAtFirstTick >= 0; }, 5000));
    EXPECT_EQ(volumeAtFirstTick.load(), 25);
    ASSERT_TRUE(QTest::qWaitFor([&]() { return player.state() == AudioState::Playing; }, 5000));
    EXPECT_EQ(output.get_property("volume").toDouble(), 25);
    player.stop();
}

// The finished() of the idle after stop() is delivered after the next play().
TEST_F(AudioPlayerWithMpvTest, StopThenPlayPlaysTheRequestedTrack) {
    const QString first = dir_.filePath("first.wav");
    const QString second = dir_.filePath("second.wav");
    ASSERT_TRUE(writeSilentWav(first, 3000));
    ASSERT_TRUE(writeSilentWav(second, 3000));
    const QList<PTrack> tracks{trackAt(first), trackAt(second)};

    MpvAudioOutput output;
    ASSERT_TRUE(usesNullAudioOutput(output));
    AudioPlayer player(nullptr, &output, /*testing=*/true);
    MainWindow mainWindow(player);
    QList<PTrack> playing;
    QObject context;
    QObject::connect(&player, &AudioPlayer::trackPlaying, &context, [&](PTrack track) {
        playing.append(track);
    });

    PModel model = player.createPlaylist(false);
    mainWindow.addPlaylist(model, "", false);
    model->addTracks(tracks);
    player.play(model, model->index(0, 0));
    ASSERT_TRUE(QTest::qWaitFor(
        [&]() { return !playing.isEmpty() && player.state() == AudioState::Playing; }, 5000));

    player.stop();
    // Busy while the output goes idle
    QThread::msleep(300);
    player.play(model, model->index(0, 0));
    QTest::qWait(1000);
    EXPECT_EQ(player.getCurrentTrack(), tracks[0]);
    EXPECT_EQ(playing.count(tracks[1]), 0);
    EXPECT_EQ(output.get_property("path").toString(), first);
    EXPECT_EQ(player.state(), AudioState::Playing);
    player.stop();
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    QApplication app(argc, argv);
    return RUN_ALL_TESTS();
}
