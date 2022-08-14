add_library(ui
    audioplayer.cpp
    mainwindow.cpp
    seekslider.cpp
    statusbar.cpp
    librarypreferencesdialog.cpp
    pluginspreferences.cpp
    consolewindow.cpp
    config.cpp
    configwindow.cpp
    tabwidget.cpp
    tabbar.cpp
    playlisttab.cpp
    playlistitemdelegate.cpp
    playlistmodel.cpp
    playlistview.cpp
    playlistfilter.cpp
    pluginmanager.cpp
    unixsignalshandler.cpp
    console.cpp
    lyricsthread.cpp
    lyricsthreaddeleter.cpp
    mprisplayer.cpp
    queue.cpp)

target_link_libraries(ui
    PUBLIC Qt5::Widgets
    PRIVATE player Glyr KF5::WindowSystem KF5::GlobalAccel)
