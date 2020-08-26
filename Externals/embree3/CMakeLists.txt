add_library(embree SHARED IMPORTED GLOBAL)
set_target_properties(embree PROPERTIES IMPORTED_IMPLIB
    ${PROJECT_SOURCE_DIR}/Externals/embree3/embree3.lib)
set_target_properties(embree PROPERTIES IMPORTED_LOCATION 
    ${PROJECT_SOURCE_DIR}/Externals/embree3/embree3.dll)
target_include_directories(embree INTERFACE ".")