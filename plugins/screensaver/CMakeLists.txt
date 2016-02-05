project(screensaver)

find_package(Qt4 4.5 REQUIRED QtCore QtDBus)

add_library(screensaver SHARED screensaverplugin.cpp)
target_link_libraries(screensaver Qt4::QtDBus)

install(TARGETS screensaver DESTINATION lib/fubar/)
