add_library(player
    audiooutput.cpp
    mpvaudiooutput.cpp
    playlist.cpp
    playlistmimedata.cpp)
target_link_libraries(player PRIVATE library Qt5::Core MPV Taglib)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
