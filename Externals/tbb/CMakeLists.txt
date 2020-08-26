add_library(tbb SHARED IMPORTED GLOBAL)

# set_target_properties(tbb PROPERTIES IMPORTED_IMPLIB_DEBUG
#     ${PROJECT_SOURCE_DIR}/Externals/tbb/tbb_debug.lib)
# set_target_properties(tbb PROPERTIES IMPORTED_IMPLIB_RELEASE
#     ${PROJECT_SOURCE_DIR}/Externals/tbb/tbb.lib)

# set_target_properties(tbb PROPERTIES IMPORTED_LOCATION_DEBUG
#     ${PROJECT_SOURCE_DIR}/Externals/tbb/tbb_debug.dll)
# set_target_properties(tbb PROPERTIES IMPORTED_LOCATION_RELEASE
#     ${PROJECT_SOURCE_DIR}/Externals/tbb/tbb.dll)

    
set_target_properties(tbb PROPERTIES IMPORTED_IMPLIB
    ${PROJECT_SOURCE_DIR}/Externals/tbb/tbb.lib)
set_target_properties(tbb PROPERTIES IMPORTED_LOCATION
    ${PROJECT_SOURCE_DIR}/Externals/tbb/tbb.dll)

target_include_directories(tbb INTERFACE ".")

add_library(tbbMalloc SHARED IMPORTED GLOBAL)

# set_target_properties(tbbMalloc PROPERTIES IMPORTED_IMPLIB_DEBUG
#     ${PROJECT_SOURCE_DIR}/Externals/tbb/tbbmalloc_debug.lib)
# set_target_properties(tbbMalloc PROPERTIES IMPORTED_IMPLIB_RELEASE
#     ${PROJECT_SOURCE_DIR}/Externals/tbb/tbbmalloc.lib)

# set_target_properties(tbbMalloc PROPERTIES IMPORTED_LOCATION_DEBUG
#     ${PROJECT_SOURCE_DIR}/Externals/tbb/tbbmalloc_debug.dll)
# set_target_properties(tbbMalloc PROPERTIES IMPORTED_LOCATION_RELEASE
#     ${PROJECT_SOURCE_DIR}/Externals/tbb/tbbmalloc.dll)


set_target_properties(tbbMalloc PROPERTIES IMPORTED_IMPLIB
    ${PROJECT_SOURCE_DIR}/Externals/tbb/tbbmalloc.lib)
set_target_properties(tbbMalloc PROPERTIES IMPORTED_LOCATION
    ${PROJECT_SOURCE_DIR}/Externals/tbb/tbbmalloc.dll)

target_include_directories(tbbMalloc INTERFACE ".")