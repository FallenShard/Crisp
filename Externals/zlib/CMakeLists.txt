add_library(zlib SHARED IMPORTED GLOBAL)
set_target_properties(zlib PROPERTIES IMPORTED_IMPLIB
    ${PROJECT_SOURCE_DIR}/Externals/zlib/zlib.lib)
set_target_properties(zlib PROPERTIES IMPORTED_LOCATION 
    ${PROJECT_SOURCE_DIR}/Externals/zlib/zlib1.dll)
target_include_directories(zlib INTERFACE ".")