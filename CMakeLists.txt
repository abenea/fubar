cmake_minimum_required(VERSION 3.13)
project(fubar)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
find_package(Qt5 5.0 REQUIRED Core Gui Widgets)
find_package(KF5WindowSystem)
find_package(KF5GlobalAccel)
find_package(Cue REQUIRED)
find_package(Boost REQUIRED)
find_package(Taglib 1.8.0 REQUIRED)
find_package(Protobuf REQUIRED)
find_package(MPV REQUIRED)
find_package(Glyr REQUIRED)
include(CTest)
if(BUILD_TESTING)
  find_package(GTest REQUIRED)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

add_compile_options(-Wall)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_library(util util.cpp)
target_link_libraries(util PUBLIC Qt5::Core)

add_subdirectory(library)
add_subdirectory(player)
add_subdirectory(ui)
add_subdirectory(plugins)

qt5_add_resources(ui_RC ui/icons/icons.qrc)
add_executable(fubar ${ui_RC} main.cpp)
target_link_libraries(fubar PRIVATE util ui player library Qt5::Gui)

install(TARGETS fubar DESTINATION bin/)
