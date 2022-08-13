#pragma once

#include <QDockWidget>
#include <deque>

class QPlainTextEdit;

class ConsoleWindow : public QDockWidget {
    Q_OBJECT
public:
    ConsoleWindow(QWidget *parent = 0);

public slots:
    void show();

private slots:
    void updated(QtMsgType type, QString message);

private:
    void setCursorToEnd();

    std::deque<std::pair<QtMsgType, QString>> messages_;
    QPlainTextEdit *text_;
    QByteArray geometry_;
};
