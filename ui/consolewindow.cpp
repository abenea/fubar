#include "consolewindow.h"
#include "console.h"

std::size_t MAX_MESSAGES = 500;

ConsoleWindow::ConsoleWindow(QWidget* parent): QDialog(parent)
{
    setupUi(this);
    QObject::connect(::Console::instance(), SIGNAL(updated(QtMsgType,QString)), this, SLOT(updated(QtMsgType,QString)));
}

void ConsoleWindow::updated(QtMsgType type, QString message)
{
    messages_.push_back({type, message});
    if (messages_.size() > MAX_MESSAGES)
        messages_.pop_front();
    if (isVisible())
        text->append(message);
    setCursorToEnd();
}

void ConsoleWindow::show()
{
    text->setPlainText("");
    for (const auto& p : messages_) {
        text->append(p.second);
    }
    setCursorToEnd();
    if (geometry_.size())
        restoreGeometry(geometry_);
    QDialog::show();
}

void ConsoleWindow::hide()
{
    geometry_ = saveGeometry();
    QDialog::hide();
}

void ConsoleWindow::setCursorToEnd()
{
    if (!isVisible())
        return;
    text->moveCursor(QTextCursor::End);
    text->moveCursor(QTextCursor::StartOfLine);
}

#include "consolewindow.moc"