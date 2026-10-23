add_library(navidrome
    navidromeconfig.cpp
    navidrometrack.cpp
    navidromeclient.cpp
    navidromelibrary.cpp
    navidromescrobbler.cpp)
target_link_libraries(navidrome
    PUBLIC Qt::Core Qt::Network
    PRIVATE library util protobuf::libprotobuf)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
