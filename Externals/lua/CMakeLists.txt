add_library(lua SHARED IMPORTED GLOBAL)
set_target_properties(lua PROPERTIES IMPORTED_IMPLIB 
    ${PROJECT_SOURCE_DIR}/Externals/lua/lua51.lib)
set_target_properties(lua PROPERTIES IMPORTED_LOCATION 
    ${PROJECT_SOURCE_DIR}/Externals/lua/lua51.dll)
target_include_directories(lua INTERFACE ".")