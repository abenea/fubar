# Defines IMPORTED library Glyr

find_package(PkgConfig)
pkg_check_modules(Glyr QUIET libglyr IMPORTED_TARGET GLOBAL)
add_library(Glyr ALIAS PkgConfig::Glyr)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Glyr
    VERSION_VAR Glyr_VERSION
    REQUIRED_VARS Glyr_LIBRARIES Glyr_INCLUDE_DIRS)
