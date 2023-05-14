add_cpp_static_library(CrispGltfLoader
    "GltfLoader.cpp"
    "GltfLoader.hpp"
)
target_link_libraries(CrispGltfLoader
    PUBLIC CrispResult
    PUBLIC CrispHashMap
    PUBLIC CrispMaterial
    PUBLIC CrispMesh
    PRIVATE CrispLogger
    PRIVATE CrispImageIo
    PRIVATE tinygltf
)

add_cpp_static_library(CrispWavefrontObjLoader
    "WavefrontObjLoader.cpp"
    "WavefrontObjLoader.hpp"
)
target_link_libraries(CrispWavefrontObjLoader
    PUBLIC CrispResult
    PUBLIC CrispHashMap
    PUBLIC CrispMaterial
    PUBLIC CrispMesh
    PRIVATE CrispLogger
    PRIVATE CrispStringUtils
    PRIVATE CrispImageIo
)

add_cpp_static_library(CrispMeshIo
    "MeshLoader.cpp"
    "MeshLoader.hpp"
)
target_link_libraries(CrispMeshIo
    PUBLIC CrispGltfLoader
    PUBLIC CrispWavefrontObjLoader
    PRIVATE CrispLogger
)

add_cpp_test(CrispWavefrontObjLoaderTest "Test/WavefrontObjLoaderTest.cpp")
target_link_libraries(CrispWavefrontObjLoaderTest PRIVATE CrispWavefrontObjLoader)

add_cpp_test(CrispGltfLoaderTest "Test/GltfLoaderTest.cpp")
target_link_libraries(CrispGltfLoaderTest PRIVATE CrispGltfLoader)