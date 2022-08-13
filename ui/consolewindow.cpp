#include "ui/consolewindow.h"
#include "ui/console.h"
#include <QPlainTextEdit>

std::size_t MAX_MESSAGES = 500;

ConsoleWindow::ConsoleWindow(QWidget *parent) : QDockWidget("Console", parent) {
    QObject::connect(::Console::instance(), SIGNAL(updated(QtMsgType, QString)), this,
                     SLOT(updated(QtMsgType, QString)));
    text_ = new QPlainTextEdit(this);
    text_->setReadOnly(true);
    setWidget(text_);
}

void ConsoleWindow::updated(QtMsgType type, QString message) {
    messages_.push_back({type, message});
    if (messages_.size() > MAX_MESSAGES)
        messages_.pop_front();
    if (isVisible())
        text_->appendPlainText(message);
    setCursorToEnd();
}

void ConsoleWindow::show() {
    text_->setPlainText("");
    for (const auto &p : messages_) {
        text_->appendPlainText(p.second);
    }
    setCursorToEnd();
    QDockWidget::show();
}

void ConsoleWindow::setCursorToEnd() {
    if (!isVisible())
        return;
    text_->moveCursor(QTextCursor::End);
    text_->moveCursor(QTextCursor::StartOfLine);
}
