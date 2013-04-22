include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

set(player_SOURCES
    audiooutput.cpp
    audioplayer.cpp
    phononaudiooutput.cpp
    playlistmodel.cpp
    playlist.cpp
    queue.cpp)

qt4_automoc(${player_SOURCES})

add_library(player ${player_SOURCES})
target_link_libraries(player ${QT_LIBRARIES})

add_subdirectory(tests)