add_library(tinygltf STATIC
    "tiny_gltf/tiny_gltf.cc"
    "tiny_gltf/tiny_gltf.h"
)

target_include_directories(tinygltf PUBLIC ".")

target_compile_definitions(tinygltf PUBLIC TINYGLTF_NO_STB_IMAGE)
target_compile_definitions(tinygltf PUBLIC TINYGLTF_NO_STB_IMAGE_WRITE)

target_link_libraries(tinygltf PUBLIC json)
set_target_properties(tinygltf PROPERTIES
    CXX_STANDARD 20)