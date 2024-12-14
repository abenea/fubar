add_library(player
    audiooutput.cpp
    mpvaudiooutput.cpp
    playlist.cpp
    playlistmimedata.cpp
    jsonprocess.cpp)
target_link_libraries(player PUBLIC Qt::Core PRIVATE library MPV Taglib)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
