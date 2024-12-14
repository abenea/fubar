if (Qt5_FOUND)
  # The official liblastfm doesn't work with Qt6 but there's a fork https://github.com/drfiemost/liblastfm
  add_subdirectory(lastfm)
endif()
add_subdirectory(screensaver)
