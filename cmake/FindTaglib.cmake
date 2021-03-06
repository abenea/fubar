if(NOT WIN32)
    find_program(TAGLIBCONFIG_EXECUTABLE NAMES taglib-config PATHS
       ${BIN_INSTALL_DIR}
    )
endif(NOT WIN32)

# if taglib-config has been found
if(TAGLIBCONFIG_EXECUTABLE)

  exec_program(${TAGLIBCONFIG_EXECUTABLE} ARGS --version RETURN_VALUE _return_VALUE OUTPUT_VARIABLE TAGLIB_VERSION)
  if(TAGLIB_VERSION VERSION_LESS Taglib_FIND_VERSION)
    message(FATAL_ERROR "Could not a compatible Taglib version (found ${TAGLIB_VERSION})")
  endif(TAGLIB_VERSION VERSION_LESS Taglib_FIND_VERSION)
  exec_program(${TAGLIBCONFIG_EXECUTABLE} ARGS --prefix RETURN_VALUE _return_VALUE OUTPUT_VARIABLE TAGLIB_LIBRARY_PREFIX)

  find_path(TAGLIB_INCLUDE_DIR NAMES tag.h
   HINTS
   ${TAGLIB_LIBRARY_PREFIX}/include
   PATH_SUFFIXES taglib
  )

  find_library(TAGLIB_LIBRARY NAMES tag
    HINTS
    ${TAGLIB_LIBRARY_PREFIX}/lib
  )

#   if(TAGLIB_VERSION STRLESS "${TAGLIB_MIN_VERSION}")
#      message(STATUS "TagLib version too old: version searched :${TAGLIB_MIN_VERSION}, found ${TAGLIB_VERSION}")
#      set(TAGLIB_FOUND FALSE)
#   else(TAGLIB_VERSION STRLESS "${TAGLIB_MIN_VERSION}")
# 
#      if(TAGLIB_LIBRARY AND TAGLIB_INCLUDE_DIR)
#         set(TAGLIB_FOUND TRUE)
#      endif(TAGLIB_LIBRARY AND TAGLIB_CFLAGS)
#   endif(TAGLIB_VERSION STRLESS "${TAGLIB_MIN_VERSION}") 
     if(TAGLIB_LIBRARY AND TAGLIB_INCLUDE_DIR)
        set(TAGLIB_FOUND TRUE)
     endif(TAGLIB_LIBRARY AND TAGLIB_INCLUDE_DIR)
  mark_as_advanced(TAGLIB_LIBRARY TAGLIB_INCLUDE_DIR)

else(TAGLIBCONFIG_EXECUTABLE)

  find_path(TAGLIB_INCLUDE_DIR
    NAMES
    tag.h
    PATH_SUFFIXES taglib
    PATHS
    ${KDE4_INCLUDE_DIR}
    ${INCLUDE_INSTALL_DIR}
  )

    IF(NOT WIN32)
      # on non-win32 we don't need to take care about WIN32_DEBUG_POSTFIX

      FIND_LIBRARY(TAGLIB_LIBRARY tag PATHS ${KDE4_LIB_DIR} ${LIB_INSTALL_DIR})

    ELSE(NOT WIN32)

      # 1. get all possible libnames
      SET(args PATHS ${KDE4_LIB_DIR} ${LIB_INSTALL_DIR})             
      SET(newargs "")               
      SET(libnames_release "")      
      SET(libnames_debug "")        

      LIST(LENGTH args listCount)

        # just one name
        LIST(APPEND libnames_release "tag")
        LIST(APPEND libnames_debug   "tagd")

        SET(newargs ${args})

      # search the release lib
      FIND_LIBRARY(TAGLIB_LIBRARY_RELEASE
                   NAMES ${libnames_release}
                   ${newargs}
      )

      # search the debug lib
      FIND_LIBRARY(TAGLIB_LIBRARY_DEBUG
                   NAMES ${libnames_debug}
                   ${newargs}
      )

      IF(TAGLIB_LIBRARY_RELEASE AND TAGLIB_LIBRARY_DEBUG)

        # both libs found
        SET(TAGLIB_LIBRARY optimized ${TAGLIB_LIBRARY_RELEASE}
                        debug     ${TAGLIB_LIBRARY_DEBUG})

      ELSE(TAGLIB_LIBRARY_RELEASE AND TAGLIB_LIBRARY_DEBUG)

        IF(TAGLIB_LIBRARY_RELEASE)

          # only release found
          SET(TAGLIB_LIBRARY ${TAGLIB_LIBRARY_RELEASE})

        ELSE(TAGLIB_LIBRARY_RELEASE)

          # only debug (or nothing) found
          SET(TAGLIB_LIBRARY ${TAGLIB_LIBRARY_DEBUG})

        ENDIF(TAGLIB_LIBRARY_RELEASE)

      ENDIF(TAGLIB_LIBRARY_RELEASE AND TAGLIB_LIBRARY_DEBUG)

      MARK_AS_ADVANCED(TAGLIB_LIBRARY_RELEASE)
      MARK_AS_ADVANCED(TAGLIB_LIBRARY_DEBUG)

    ENDIF(NOT WIN32)
  
  INCLUDE(FindPackageMessage)
  INCLUDE(FindPackageHandleStandardArgs)
  FIND_PACKAGE_HANDLE_STANDARD_ARGS(Taglib DEFAULT_MSG TAGLIB_INCLUDE_DIR TAGLIB_LIBRARY)

endif(TAGLIBCONFIG_EXECUTABLE)


if(TAGLIB_FOUND)
  if(NOT Taglib_FIND_QUIETLY AND TAGLIBCONFIG_EXECUTABLE)
    message(STATUS "Taglib found: ${TAGLIB_VERSION}")
  endif(NOT Taglib_FIND_QUIETLY AND TAGLIBCONFIG_EXECUTABLE)
else(TAGLIB_FOUND)
  if(Taglib_FIND_REQUIRED)
    message(FATAL_ERROR "Could not find Taglib")
  endif(Taglib_FIND_REQUIRED)
endif(TAGLIB_FOUND)

