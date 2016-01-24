include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${MPV_INCLUDE_DIR})

set(player_SOURCES
    audiooutput.cpp
    phononaudiooutput.cpp
    mpvaudiooutput.cpp
    playlist.cpp
    playlistmimedata.cpp)

qt4_automoc(${player_SOURCES})

add_library(player ${player_SOURCES})
target_link_libraries(player library ${QT_LIBRARIES} ${MPV_LIBRARY})

add_subdirectory(tests)