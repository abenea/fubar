project(lastfm)

find_package(LibLastFM5 REQUIRED)
find_package(Qt5 5.0 REQUIRED Core Gui Network Widgets Xml)

add_library(lastfm SHARED lastfmplugin.cpp preferences.cpp)
target_link_libraries(lastfm PRIVATE Qt5::Network Qt5::Gui Qt5::Widgets Qt5::Xml LibLastFM5)

install(TARGETS lastfm DESTINATION lib/fubar/)
