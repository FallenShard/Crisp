add_library(freetype SHARED IMPORTED GLOBAL)
set_target_properties(freetype PROPERTIES IMPORTED_IMPLIB
    ${PROJECT_SOURCE_DIR}/Externals/freetype/freetype27.lib)
set_target_properties(freetype PROPERTIES IMPORTED_LOCATION 
    ${PROJECT_SOURCE_DIR}/Externals/freetype/freetype27.dll)
target_include_directories(freetype INTERFACE ".")