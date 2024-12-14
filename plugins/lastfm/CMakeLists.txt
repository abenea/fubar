project(lastfm)

find_package(LibLastFM5 REQUIRED)

add_library(lastfm SHARED lastfmplugin.cpp preferences.cpp)
target_link_libraries(lastfm PRIVATE Qt::Network Qt::Gui Qt::Widgets Qt::Xml LibLastFM5)

install(TARGETS lastfm DESTINATION lib/fubar/)
