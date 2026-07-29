add_cpp_header_library(CrispModelData
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
    PUBLIC meshoptimizer
    PRIVATE Crisp::Logger
)

add_cpp_test(CrispWavefrontObjLoaderTest "Test/WavefrontObjLoaderTest.cpp")
target_link_libraries(CrispWavefrontObjLoaderTest PRIVATE Crisp::WavefrontObjLoader)
stage_test_file(CrispWavefrontObjLoaderTest "Test/Data/simple.obj" "simple.obj")

set(externalAssetConfig "${CMAKE_CURRENT_BINARY_DIR}/Generated/Crisp/Mesh/Io/ExternalAssetConfig.hpp")
configure_file("Test/ExternalAssetConfig.hpp.in" "${externalAssetConfig}" @ONLY)

add_cpp_test(CrispWavefrontObjLoaderExternalAssetTest "Test/WavefrontObjLoaderExternalAssetTest.cpp")
target_link_libraries(CrispWavefrontObjLoaderExternalAssetTest PRIVATE Crisp::WavefrontObjLoader)
target_include_directories(
    CrispWavefrontObjLoaderExternalAssetTest PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/Generated")

add_cpp_benchmark(CrispWavefrontObjLoaderBenchmark "Benchmark/WavefrontObjLoaderBenchmark.cpp")
target_link_libraries(CrispWavefrontObjLoaderBenchmark PRIVATE Crisp::WavefrontObjLoader)
target_include_directories(CrispWavefrontObjLoaderBenchmark PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/Generated")

add_cpp_test(CrispGltfLoaderTest "Test/GltfLoaderTest.cpp")
target_link_libraries(CrispGltfLoaderTest PRIVATE Crisp::GltfLoader)
target_include_directories(CrispGltfLoaderTest PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/Generated")
stage_test_file(CrispGltfLoaderTest "Test/Data/Triangle.gltf" "Triangle.gltf")
