# TODO Use the dependencies
# set(Qxt_QxtGui_DEPENDSON QxtCore)
# set(Qxt_QxtWeb_DEPENDSON QxtCore QxtNetwork)
# set(Qxt_QxtZeroconf_DEPENDSON QxtCore QxtNetwork)
# set(Qxt_QxtNetwork_DEPENDSON QxtCore)
# set(Qxt_QxtQSql_DEPENDSON QxtCore)
# set(Qxt_QxtBerkeley_DEPENDSON QxtCore)

if(NOT Qxt_FIND_COMPONENTS)
  set(Qxt_FIND_COMPONENTS QxtGui QxtWeb QxtZeroConf QxtNetwork QxtSql QxtBerkeley QxtCore)
endif()

set(EXTRA_SEARCH_PATHS
  ~/Library/Frameworks/
  /Library/Frameworks/
  /sw
  /usr/local
  /usr
  /opt/local
  /opt/csw
  /opt
  "C:\\"
  "C:\\Program Files"
  "C:\\Program Files(x86)"
  )

set(Qxt_INCLUDE_DIRS)
foreach(mod ${Qxt_FIND_COMPONENTS})
  find_path(Qxt_${mod}_INCLUDE_DIR ${mod}
    PATHS ${EXTRA_SEARCH_PATHS}
    PATH_SUFFIXES ${mod} include/${mod} qxt/include/${mod} include/qxt/${mod})
  find_library(Qxt_${mod}_LIB NAME ${mod}
    PATH_SUFFIXES Qxt/lib64 Qxt/lib lib64 lib
    PATHS ${EXTRA_SEARCH_PATHS})
  # TODO debug libs?
  # find_library(Qxt_${mod}_LIB_DEBUG NAME ${mod}d
  #   PATH_SUFFIXES Qxt/lib64 Qxt/lib lib64 lib
  #   PATHS ${EXTRA_SEARCH_PATHS})
  mark_as_advanced(Qxt_${mod}_INCLUDE_DIR Qxt_${mod}_LIB)
  if(Qxt_${mod}_INCLUDE_DIR AND Qxt_${mod}_LIB)
    set(Qxt_${mod}_FOUND 1)
    list(APPEND Qxt_INCLUDE_DIRS ${Qxt_${mod}_INCLUDE_DIR})
    list(APPEND Qxt_LIBRARIES ${Qxt_${mod}_LIB})
  endif()
endforeach()

find_package_handle_standard_args(Qxt REQUIRED_VARS Qxt_INCLUDE_DIRS Qxt_LIBRARIES HANDLE_COMPONENTS)
