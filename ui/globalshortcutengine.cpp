#include <QDebug>
#include "globalshortcutengine.h"
#include <QSocketNotifier>

GlobalShortcutEngine::GlobalShortcutEngine(MainWindow& mainWindow)
    : mainWindow_(mainWindow)
{
    SetShortcuts();
    int evt, error;
    display = NULL;

    display = XOpenDisplay(NULL);

    if (! display) {
        qWarning("GlobalShortcutX: Unable to open dedicated display connection.");
        return;
    }

    for (int i=0; i < ScreenCount(display); ++i)
        qsRootWindows.insert(RootWindow(display, i));
    qDebug() << qsRootWindows.size();

    if (XQueryExtension(display, "XInputExtension", &iXIopcode, &evt, &error)) {
        int major = 2;
        int minor = 0;
        int rc = XIQueryVersion(display, &major, &minor);
        if (rc != BadRequest) {
            qWarning("GlobalShortcutX: Using XI2 %d.%d", major, minor);

            queryXIMasterList();

            XIEventMask evmask;
            unsigned char mask[(XI_LASTEVENT + 7)/8];

            memset(&evmask, 0, sizeof(evmask));
            memset(mask, 0, sizeof(mask));

            XISetMask(mask, XI_RawButtonPress);
            XISetMask(mask, XI_RawButtonRelease);
            XISetMask(mask, XI_RawKeyPress);
            XISetMask(mask, XI_RawKeyRelease);
            XISetMask(mask, XI_HierarchyChanged);

            evmask.deviceid = XIAllDevices;
            evmask.mask_len = sizeof(mask);
            evmask.mask = mask;

            foreach(Window w, qsRootWindows)
                XISelectEvents(display, w, &evmask, 1);
            XFlush(display);

            connect(new QSocketNotifier(ConnectionNumber(display), QSocketNotifier::Read, this), SIGNAL(activated(int)), this, SLOT(displayReadyRead(int)));

            return;
        }
    }
}

void GlobalShortcutEngine::queryXIMasterList() {
    XIDeviceInfo *info, *dev;
    int ndevices;

    qsMasterDevices.clear();

    dev = info = XIQueryDevice(display, XIAllDevices, &ndevices);
    for (int i=0;i<ndevices;++i) {
        switch (dev->use) {
            case XIMasterPointer:
            case XIMasterKeyboard:
                qsMasterDevices.insert(dev->deviceid);
                break;
            default:
                break;
        }

        ++dev;
    }
    XIFreeDeviceInfo(info);
}

// XInput2 event is ready on socketnotifier.
void GlobalShortcutEngine::displayReadyRead(int)
{
    XEvent evt;

//    if (bNeedRemap)
//        remap();

    while (XPending(display)) {
        XNextEvent(display, &evt);
        XGenericEventCookie *cookie = & evt.xcookie;

        if ((cookie->type != GenericEvent) || (cookie->extension != iXIopcode) || !XGetEventData(display, cookie))
            continue;

        XIDeviceEvent *xide = reinterpret_cast<XIDeviceEvent *>(cookie->data);

        switch (cookie->evtype) {
            case XI_RawKeyPress:
            case XI_RawKeyRelease:
                if (! qsMasterDevices.contains(xide->deviceid))
                    handleButton(xide->detail, cookie->evtype == XI_RawKeyPress);
                break;
            case XI_RawButtonPress:
            case XI_RawButtonRelease:
                if (! qsMasterDevices.contains(xide->deviceid))
                    handleButton(xide->detail + 0x117, cookie->evtype == XI_RawButtonPress);
                break;
            case XI_HierarchyChanged:
                queryXIMasterList();
        }

        XFreeEventData(display, cookie);
    }
}

void GlobalShortcutEngine::handleButton(int button, bool down)
{
    if (down)
        down_keys_.insert(button);
    else
        down_keys_.erase(button);

    for (auto shortcut : shortcuts_) {
        bool on = true;
        for (auto key : shortcut.first) {
            if (down_keys_.find(key) == down_keys_.end()) {
                on = false;
                break;
            }
        }
        if (on) {
            shortcut.second();
        }
    }
}

void GlobalShortcutEngine::SetShortcuts()
{
    // win_l 133 up 111 down 116 a 38 z 52 x 39 c 54 v 55 w 25
    shortcuts_.insert({
        {{133, 54}, std::bind(&MainWindow::PlayPause, &mainWindow_)},
        {{133, 55}, std::bind(&MainWindow::Stop, &mainWindow_)},
        {{133, 38}, std::bind(&MainWindow::Prev, &mainWindow_)},
        {{133, 52}, std::bind(&MainWindow::Next, &mainWindow_)},
        {{133, 25}, std::bind(&MainWindow::ShowHide, &mainWindow_)},
        });
}

#include "globalshortcutengine.moc"