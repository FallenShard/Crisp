add_library(vulkan SHARED IMPORTED GLOBAL)
set_target_properties(vulkan PROPERTIES IMPORTED_IMPLIB
    ${PROJECT_SOURCE_DIR}/Externals/vulkan/vulkan-1.lib)
set_target_properties(vulkan PROPERTIES IMPORTED_LOCATION 
    ${PROJECT_SOURCE_DIR}/Externals/vulkan/vulkan-1.dll)
target_include_directories(vulkan INTERFACE ".")