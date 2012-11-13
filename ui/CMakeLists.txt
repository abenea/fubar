include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(ui_SOURCES
    mainwindow.cpp
    seekslider.cpp
    statusbar.cpp
    librarypreferencesdialog.cpp
    pluginspreferences.cpp
    playlisttab.cpp
    playlistitemdelegate.cpp
    playlistview.cpp
    playlistmodel.cpp
    playlistfilter.cpp)
qt4_wrap_ui(ui_UI mainwindow.ui playlist.ui librarypreferences.ui plugins.ui)
qt4_automoc(${ui_SOURCES})

add_library(ui ${ui_UI} ${ui_SOURCES})
target_link_libraries(ui library ${QT_LIBRARIES})

add_subdirectory(tests)
