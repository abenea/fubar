project(lastfm)

find_package(LibLastFm REQUIRED)
find_package(Qt4 4.5 REQUIRED QtCore QtXml QtNetwork)
message(${LIBLASTFM_INCLUDE_DIR})
SET(LASTFMPLUGIN_HEADERS lastfmplugin.h)
SET(LASTFMPLUGIN_SOURCES lastfmplugin.cpp)

INCLUDE(${QT_USE_FILE})
SET(QT_USE_QTPLUGIN TRUE)

include_directories(${LIBLASTFM_INCLUDE_DIR} ${LIBLASTFM_INCLUDE_DIR}/..)
QT4_AUTOMOC(${LASTFMPLUGIN_SOURCES})
QT4_WRAP_CPP(LASTFMPLUGIN_MOC ${LASTFMPLUGIN_HEADERS})

add_library(lastfm SHARED ${LASTFMPLUGIN_HEADERS} ${LASTFMPLUGIN_SOURCES} ${LASTFMPLUGIN_MOC})
target_link_libraries(lastfm ${QT_LIBRARIES} ${LIBLASTFM_LIBRARY})



# add_definitions(${QT_DEFINITIONS})
# #add_definitions(-DQT_PLUGIN)
# #add_definitions(-DQT_SHARED)
# #add_definitions(-DQT_NO_DEBUG)
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++0x -DQT_PLUGIN -DQT_SHARED")
# 
# automoc4(lastfm lastfmplugin.cpp)
# 
# add_library(lastfm SHARED lastfmplugin.cpp)
# target_link_libraries(lastfm ${QT_LIBRARIES})
# #file(COPY ${CMAKE_CURRENT_BINARY_DIR}/../../lib/liblastfm.so DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/..)
