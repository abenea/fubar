#ifndef GLOBALSHORTCUTENGINE_H
#define GLOBALSHORTCUTENGINE_H

#include "mainwindow.h"
#include <QObject>
#include <QSet>
#include <QX11Info>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/Xutil.h>
#include <map>
#include <set>
#include <functional>

class GlobalShortcutEngine : public QObject {
private:
    Q_OBJECT
    Q_DISABLE_COPY(GlobalShortcutEngine)

public:
    GlobalShortcutEngine(MainWindow& mainWindow);
    void queryXIMasterList();
    void handleButton(int button, bool down);

    Display *display;
    QSet<Window> qsRootWindows;
    int iXIopcode;
    QSet<int> qsMasterDevices;

private:
    void SetShortcuts();
    typedef std::set<int> Shortcut;
    std::map<Shortcut, std::function<void (void)>> shortcuts_;
    MainWindow& mainWindow_;
    std::set<int> down_keys_;

public slots:
    void displayReadyRead(int);
};

#endif // GLOBALSHORTCUTENGINE_H
