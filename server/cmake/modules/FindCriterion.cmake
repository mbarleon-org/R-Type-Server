find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(CRITERION QUIET criterion)
endif()

find_path(Criterion_INCLUDE_DIR
  NAMES criterion/criterion.h
  HINTS ${CRITERION_INCLUDE_DIRS}
)

find_library(Criterion_LIBRARY
  NAMES criterion
  HINTS ${CRITERION_LIBRARY_DIRS}
)

set(Criterion_VERSION "${CRITERION_VERSION}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Criterion
  REQUIRED_VARS Criterion_LIBRARY Criterion_INCLUDE_DIR
  VERSION_VAR Criterion_VERSION)

if(Criterion_FOUND AND NOT TARGET Criterion::Criterion)
  add_library(Criterion::Criterion UNKNOWN IMPORTED)
  set_target_properties(Criterion::Criterion PROPERTIES
    IMPORTED_LOCATION "${Criterion_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${Criterion_INCLUDE_DIR}"
  )
endif()

mark_as_advanced(Criterion_INCLUDE_DIR Criterion_LIBRARY)
