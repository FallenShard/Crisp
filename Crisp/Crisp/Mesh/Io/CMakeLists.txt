add_cpp_interface_library(CrispModelData
    "ModelData.hpp"
)
target_link_libraries(CrispModelData
    INTERFACE Crisp::Material
    INTERFACE Crisp::Mesh
)

add_cpp_static_library(CrispGltfLoader
    "GltfLoader.cpp"
    "GltfLoader.hpp"
)
target_link_libraries(CrispGltfLoader
    PUBLIC Crisp::Result
    PUBLIC Crisp::HashMap
    PUBLIC Crisp::ModelData
    PRIVATE Crisp::Logger
    PRIVATE Crisp::ImageIo
    PRIVATE tinygltf
)

add_cpp_static_library(CrispWavefrontObjLoader
    "WavefrontObjLoader.cpp"
    "WavefrontObjLoader.hpp"
)
target_link_libraries(CrispWavefrontObjLoader
    PUBLIC Crisp::Result
    PUBLIC Crisp::HashMap
    PUBLIC Crisp::Material
    PUBLIC Crisp::Mesh
    PRIVATE Crisp::Logger
    PRIVATE Crisp::StringUtils
    PRIVATE Crisp::ImageIo
)

add_cpp_static_library(CrispMeshIo
    "MeshLoader.cpp"
    "MeshLoader.hpp"
)
target_link_libraries(CrispMeshIo
    PUBLIC Crisp::GltfLoader
    PUBLIC Crisp::WavefrontObjLoader
    PRIVATE Crisp::Logger
)

add_cpp_test(CrispWavefrontObjLoaderTest "Test/WavefrontObjLoaderTest.cpp")
target_link_libraries(CrispWavefrontObjLoaderTest PRIVATE Crisp::WavefrontObjLoader)

add_cpp_test(CrispGltfLoaderTest "Test/GltfLoaderTest.cpp")
target_link_libraries(CrispGltfLoaderTest PRIVATE Crisp::GltfLoader)