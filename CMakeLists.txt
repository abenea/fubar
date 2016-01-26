cmake_minimum_required(VERSION 2.6)
project(fubar)
enable_testing()
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

find_package(Cue REQUIRED)
find_package(Boost REQUIRED)
find_package(Taglib 1.8.0 REQUIRED)
find_package(ProtocolBuffers REQUIRED)
find_package(Qt4 4.5 REQUIRED QtCore QtGui QtTest Phonon)
include(${QT_USE_FILE})
find_package(KDE4 REQUIRED)
find_package(MPV REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++0x -fexceptions -lpthread")

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
function(add_qtest name src libs)
    qt4_automoc(${${src}})
    add_executable(${name} EXCLUDE_FROM_ALL ${${src}})
    target_link_libraries(${name} ${${libs}})
    add_test(${name} ${name})
endfunction()

add_library(util util.cpp)

add_subdirectory(ui)
add_subdirectory(library)
add_subdirectory(player)
add_subdirectory(plugins)

qt4_add_resources(ui_RC ui/icons/icons.qrc)
add_executable(fubar main.cpp ${ui_RC})
target_link_libraries(fubar util ui player library Qt4::QtGui ${TAGLIB_LIBRARY} ${KDE4_KDEUI_LIBRARY})

install(TARGETS fubar DESTINATION bin/)
