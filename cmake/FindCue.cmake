# Defines IMPORTED library Cue

find_package(PkgConfig)
pkg_check_modules(Cue QUIET libcue IMPORTED_TARGET GLOBAL)
add_library(Cue ALIAS PkgConfig::Cue)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Cue
    VERSION_VAR Cue_VERSION
    REQUIRED_VARS Cue_LIBRARIES Cue_INCLUDEDIR)
