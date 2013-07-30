#pragma once
#include <QString>
#include <QMap>
#include <cstdio>

struct Cd;

class CueFile
{
public:
    CueFile(QString path);
    ~CueFile();

    QMap<QString, QString> getMetadata();

    int tracks();
    QString getLocation(int trackno);
    // TODO work with ms
    int getLength(int trackno); // in seconds
    QMap<QString, QString> getMetadata(int trackno);

private:
    FILE* f_;
    Cd* cd_;
};
