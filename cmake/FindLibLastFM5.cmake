# Defines IMPORTED library LibLastFM5

find_path(
  LibLastFM5_INCLUDE_DIR
  NAMES global.h
  PATH_SUFFIXES lastfm5)
find_library(LibLastFM5_LIBRARY NAMES lastfm5)
mark_as_advanced(LibLastFM5_INCLUDE_DIR LibLastFM5_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  LibLastFM5
  VERSION_VAR LibLastFM5_VERSION
  REQUIRED_VARS LibLastFM5_LIBRARY LibLastFM5_INCLUDE_DIR)

if(LibLastFM5_INCLUDE_DIR)
  set(regex "#define LASTFM_VERSION_STRING \"(.*)\"")
  file(STRINGS "${LibLastFM5_INCLUDE_DIR}/global.h" LibLastFM5_VERSION
       REGEX ${regex})
  if(${LibLastFM5_VERSION} MATCHES ${regex})
    set(LibLastFM5_VERSION ${CMAKE_MATCH_1})
  endif()
  unset(regex)
endif()

if(LibLastFM5_FOUND)
  add_library(LibLastFM5 UNKNOWN IMPORTED)
  set_target_properties(
    LibLastFM5
    PROPERTIES IMPORTED_LOCATION ${LibLastFM5_LIBRARY}
               INTERFACE_INCLUDE_DIRECTORIES ${LibLastFM5_INCLUDE_DIR})
endif()
