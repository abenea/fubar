set(lib_DEPS library ui ${QT_LIBRARIES} ${KDE4_KDEUI_LIBS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(playertest_SRC
    mockaudiooutput.cpp
    ../audiooutput.cpp
    ../audioplayer.cpp
    ../playlistmodel.cpp
    ../playlist.cpp
    ../queue.cpp
    ../../util.cpp
    ../../plugins/pluginmanager.cpp
    ../../console.cpp
    playertest.cpp)
add_qtest(playertest playertest_SRC lib_DEPS)