project(lastfm)

find_package(LibLastFm REQUIRED)
find_package(Qt4 4.5 REQUIRED QtCore QtGui QtXml QtNetwork)
SET(LASTFMPLUGIN_SOURCES lastfmplugin.cpp preferences.cpp)

include_directories(${LIBLASTFM_INCLUDE_DIR} ${QT_QTXML_INCLUDE_DIR})
qt4_wrap_ui(lastfm_UI config.ui)

add_library(lastfm SHARED ${LASTFMPLUGIN_SOURCES} ${lastfm_UI})
target_link_libraries(lastfm Qt4::QtNetwork Qt4::QtGui ${LIBLASTFM_LIBRARY})

install(TARGETS lastfm DESTINATION lib/fubar/)
