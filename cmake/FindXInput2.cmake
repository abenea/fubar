#
# Try to find XINPUT2 library and include path.
# Once done this will define
#
# XINPUT2_FOUND
# XINPUT2_INCLUDE_PATH
# XINPUT2_LIBRARY
#

FIND_PATH( XINPUT2_INCLUDE_PATH X11/extensions/XInput2.h
    /usr/include
    /usr/local/include
    /sw/include
    /opt/local/include
    DOC "The directory where X11/extensions/XInput2.h resides")
FIND_LIBRARY( XINPUT2_LIBRARY
    NAMES Xi
    PATHS
    /usr/lib64
    /usr/lib
    /usr/local/lib64
    /usr/local/lib
    /sw/lib
    /opt/local/lib
    DOC "The XInput2 library")

IF (XINPUT2_INCLUDE_PATH AND XINPUT2_LIBRARY)
    SET( XINPUT2_FOUND 1)
ELSE (XINPUT2_INCLUDE_PATH AND XINPUT2_LIBRARY)
    SET( XINPUT2_FOUND 0)
ENDIF (XINPUT2_INCLUDE_PATH AND XINPUT2_LIBRARY)

