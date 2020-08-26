add_library(glfw STATIC IMPORTED GLOBAL)
set_target_properties(glfw PROPERTIES IMPORTED_LOCATION_DEBUG
    ${PROJECT_SOURCE_DIR}/Externals/glfw/glfw3d.lib)
set_target_properties(glfw PROPERTIES IMPORTED_LOCATION_RELEASE
    ${PROJECT_SOURCE_DIR}/Externals/glfw/glfw3.lib)
target_include_directories(glfw INTERFACE ".")