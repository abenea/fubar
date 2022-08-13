# Defines IMPORTED library MPV

find_package(PkgConfig)
pkg_check_modules(MPV QUIET mpv IMPORTED_TARGET GLOBAL)
add_library(MPV ALIAS PkgConfig::MPV)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MPV 
    VERSION_VAR MPV_VERSION
    REQUIRED_VARS MPV_LIBRARIES MPV_INCLUDEDIR)
