#include "consolewindow.h"
#include "console.h"

ConsoleWindow::ConsoleWindow(QWidget* parent): QDialog(parent)
{
    setupUi(this);
    QObject::connect(::Console::instance(), SIGNAL(updated()), this, SLOT(updated()));
    updated();
}

void ConsoleWindow::updated()
{
    text->setPlainText(::Console::instance()->text());
    text->moveCursor(QTextCursor::End);
    text->moveCursor(QTextCursor::StartOfLine);
}

#include "consolewindow.moc"