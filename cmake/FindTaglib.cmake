# Defines IMPORTED library Taglib

find_package(PkgConfig)
pkg_check_modules(Taglib QUIET taglib IMPORTED_TARGET GLOBAL)
add_library(Taglib ALIAS PkgConfig::Taglib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Taglib 
    VERSION_VAR Taglib_VERSION
    REQUIRED_VARS Taglib_LIBRARIES Taglib_INCLUDEDIR)
