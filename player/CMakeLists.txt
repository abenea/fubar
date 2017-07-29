include_directories(${MPV_INCLUDE_DIR})

set(player_SOURCES
    audiooutput.cpp
    mpvaudiooutput.cpp
    playlist.cpp
    playlistmimedata.cpp)

add_library(player ${player_SOURCES})
target_link_libraries(player library Qt5::Core ${MPV_LIBRARY} ${TAGLIB_LIBRARY})

add_subdirectory(tests)
