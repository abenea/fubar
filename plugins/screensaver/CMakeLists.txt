project(screensaver)

add_library(screensaver SHARED screensaverplugin.cpp)
target_link_libraries(screensaver PRIVATE Qt::DBus)

install(TARGETS screensaver DESTINATION lib/fubar/)
