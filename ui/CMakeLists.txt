include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(ui_SOURCES librarypreferencesdialog.cpp playlisttab.cpp playlistitemdelegate.cpp mainwindow.cpp playlistview.cpp playlistmodel.cpp playlistfilter.cpp)
qt4_automoc(${ui_SOURCES})
qt4_wrap_ui(ui_UI mainwindow.ui playlist.ui librarypreferences.ui)

add_library(ui ${ui_SOURCES} ${ui_UI})
target_link_libraries(ui library ${QT_LIBRARIES})

add_subdirectory(tests)
