project(lastfm)

find_package(LibLastFm REQUIRED)
find_package(Qt4 4.5 REQUIRED QtCore QtGui QtXml QtNetwork)
SET(LASTFMPLUGIN_HEADERS lastfmplugin.h)
SET(LASTFMPLUGIN_SOURCES lastfmplugin.cpp preferences.cpp)

INCLUDE(${QT_USE_FILE})

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${LIBLASTFM_INCLUDE_DIR} Qt4::QtXml)
qt4_wrap_ui(lastfm_UI config.ui)
qt4_automoc(${LASTFMPLUGIN_SOURCES})
QT4_WRAP_CPP(LASTFMPLUGIN_MOC ${LASTFMPLUGIN_HEADERS})

add_library(lastfm SHARED ${LASTFMPLUGIN_MOC} ${LASTFMPLUGIN_SOURCES} ${lastfm_UI})
target_link_libraries(lastfm Qt4::QtNetwork Qt4::QtGui ${LIBLASTFM_LIBRARY})

install(TARGETS lastfm DESTINATION lib/fubar/)
