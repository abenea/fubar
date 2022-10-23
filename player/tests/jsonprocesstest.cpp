#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "player/jsonprocess.h"

using ::testing::ElementsAre;

class JsonReceiver : public QObject {
    Q_OBJECT
public:
    JsonReceiver(JsonProcess *jp) {
        connect(jp, &JsonProcess::document, this, &JsonReceiver::document);
        connect(jp, &JsonProcess::parseError, this, &JsonReceiver::parseError);
        connect(jp, &JsonProcess::finished, this, &JsonReceiver::finished);
        connect(jp, &JsonProcess::errorOccurred, this, &JsonReceiver::errorOccurred);
    }

    int exitCode_ = 0;
    std::vector<QJsonDocument> documents_;
    QJsonParseError parseError_{.error = QJsonParseError::NoError};
    QProcess::ProcessError error_;

public slots:
    void document(const QJsonDocument &doc) { documents_.push_back(doc); }
    void parseError(QJsonParseError parseError) { parseError_ = parseError; }
    void finished(int exitCode) { exitCode_ = exitCode; }
    void errorOccurred(QProcess::ProcessError error) { error_ = error; }
};

TEST(JsonProcessTest, ProcessError) {
    JsonProcess jp;
    JsonReceiver receiver(&jp);
    jp.start("", {"1"});
    ASSERT_FALSE(jp.waitForFinished());
}

TEST(JsonProcessTest, ParseError) {
    JsonProcess jp;
    JsonReceiver receiver(&jp);
    jp.start("echo", {"1"});
    ASSERT_TRUE(jp.waitForFinished());
    EXPECT_THAT(receiver.parseError_.error, QJsonParseError::IllegalValue);
}

TEST(JsonProcessTest, OneDoc) {
    JsonProcess jp;
    JsonReceiver receiver(&jp);
    jp.start("echo", {"{}"});
    ASSERT_TRUE(jp.waitForFinished());
    EXPECT_THAT(receiver.documents_, ElementsAre(QJsonDocument::fromJson("{}")));
    EXPECT_THAT(receiver.parseError_.error, QJsonParseError::NoError);
}

TEST(JsonProcessTest, TwoDocs) {
    JsonProcess jp;
    JsonReceiver receiver(&jp);
    jp.start("echo", {"{}\n{}"});
    ASSERT_TRUE(jp.waitForFinished());
    EXPECT_THAT(receiver.documents_,
                ElementsAre(QJsonDocument::fromJson("{}"), QJsonDocument::fromJson("{}")));
    EXPECT_THAT(receiver.parseError_.error, QJsonParseError::NoError);
}

TEST(JsonProcessTest, TwoDocsSameLine) {
    JsonProcess jp;
    JsonReceiver receiver(&jp);
    jp.start("echo", {"{}{}"});
    ASSERT_TRUE(jp.waitForFinished());
    EXPECT_THAT(receiver.parseError_.error, QJsonParseError::GarbageAtEnd);
}

TEST(JsonProcessTest, TwoDocsError) {
    JsonProcess jp;
    JsonReceiver receiver(&jp);
    jp.start("echo", {"{}\n{"});
    ASSERT_TRUE(jp.waitForFinished());
    EXPECT_THAT(receiver.parseError_.error, QJsonParseError::UnterminatedObject);
}

#include "jsonprocesstest.moc"
