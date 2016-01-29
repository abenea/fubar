include_directories(${MPV_INCLUDE_DIR})

set(player_SOURCES
    audiooutput.cpp
    phononaudiooutput.cpp
    mpvaudiooutput.cpp
    playlist.cpp
    playlistmimedata.cpp)

add_library(player ${player_SOURCES})
target_link_libraries(player library Qt4::QtCore Qt4::phonon ${MPV_LIBRARY})

add_subdirectory(tests)
