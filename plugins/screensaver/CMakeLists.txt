project(screensaver)

find_package(Qt5 5.0 REQUIRED Core DBus)

add_library(screensaver SHARED screensaverplugin.cpp)
target_link_libraries(screensaver PRIVATE Qt5::DBus)

install(TARGETS screensaver DESTINATION lib/fubar/)
