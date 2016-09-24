set(ui_SOURCES
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
    queue.cpp)
qt4_wrap_ui(ui_UI mainwindow.ui playlist.ui librarypreferences.ui plugins.ui configwindow.ui)

include_directories(${GLYR_INCLUDE_DIRS})
add_library(ui ${ui_UI} ${ui_SOURCES})
target_link_libraries(ui player Qt4::QtGui ${GLYR_LIBRARIES})
