project(lastfm)

find_package(LibLastFm5 REQUIRED)
find_package(Qt5 5.0 REQUIRED Core Gui Network Widgets Xml)
SET(LASTFMPLUGIN_SOURCES lastfmplugin.cpp preferences.cpp)

include_directories(${LIBLASTFM_INCLUDE_DIR} ${Qt5Xml_INCLUDE_DIRS})
qt5_wrap_ui(lastfm_UI config.ui)

add_library(lastfm SHARED ${LASTFMPLUGIN_SOURCES} ${lastfm_UI})
target_link_libraries(lastfm Qt5::Network Qt5::Gui Qt5::Widgets ${LIBLASTFM_LIBRARY})

install(TARGETS lastfm DESTINATION lib/fubar/)
