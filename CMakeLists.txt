cmake_minimum_required(VERSION 2.6)
project(fubar)
enable_testing()
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)


set(TAGLIB_MIN_VERSION "1.6")
find_package(Boost REQUIRED)
find_package(Taglib REQUIRED)
find_package(ProtocolBuffers REQUIRED)
find_package(Qt4 4.5 COMPONENTS QtCore QtGui QtTest Phonon REQUIRED)
find_package(XInput2 REQUIRED)
find_package(KDE4 REQUIRED)
include(${QT_USE_FILE})

  FIND_LIBRARY(QXT_CORE_LIBRARY NAMES QxtCore
    PATHS
    /usr/lib/x86_64-linux-gnu/libQxtCore.so
  )

  FIND_LIBRARY(QXT_GUI_LIBRARY NAMES QxtGui
    PATHS
    /usr/lib/x86_64-linux-gnu/libQxtGui.so
  )

# find_package(KDE4) overrides ***REMOVED***
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++0x -fexceptions")

include_directories("/usr/include/qxt/QxtCore")
include_directories("/usr/include/qxt/QxtGui")
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
function(add_qtest name src libs)
    qt4_automoc(${${src}})
    add_executable(${name} ${${src}})
    target_link_libraries(${name} ${${libs}})
    add_test(${name} ${name})
endfunction()

add_subdirectory(ui)
add_subdirectory(library)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
qt4_add_resources(ui_RC ui/icons/icons.qrc)
add_executable(fubar main.cpp ${ui_RC})
add_executable(testlib testlib.cpp ${ui_RC})
target_link_libraries(testlib ui library ${QT_LIBRARIES})
target_link_libraries(fubar library ui ${QT_LIBRARIES} ${KDE4_KDEUI_LIBS} ${KDE4_KPARTS_LIBS}  ${QXT_CORE_LIBRARY} ${QXT_GUI_LIBRARY})

