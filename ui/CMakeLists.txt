include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_BINARY_DIR})

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
    queue.cpp)
qt4_wrap_ui(ui_UI mainwindow.ui playlist.ui librarypreferences.ui plugins.ui configwindow.ui)
qt4_automoc(${ui_SOURCES})

add_library(ui ${ui_UI} ${ui_SOURCES})
target_link_libraries(ui player ${QT_LIBRARIES})
