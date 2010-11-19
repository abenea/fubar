#ifndef FILTERTEST_H
#define FILTERTEST_H

#include <QtTest/QTest>

class FilterTest : public QObject
{
    Q_OBJECT
private slots:
    void filterPlaylist();
};

#endif // FILTERTEST_H
