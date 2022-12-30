add_cpp_static_library(CrispMeshIo
    "GltfReader.cpp"
    "GltfReader.hpp"
    "MeshLoader.cpp"
    "MeshLoader.hpp"
    "FbxImporter.cpp"
    "FbxImporter.hpp"
    "WavefrontObjReader.cpp"
    "WavefrontObjReader.hpp")
target_link_libraries(CrispMeshIo
    PUBLIC CrispResult
    PUBLIC CrispHashMap
    PUBLIC CrispMaterial
    PUBLIC CrispMesh
    PRIVATE CrispLogger
    PRIVATE CrispStringUtils
    PRIVATE CrispImageIo
    PRIVATE tinygltf
    PRIVATE gzip
    PRIVATE zlib)

add_cpp_test(CrispMeshIoTest
    "Test/WavefrontObjTest.cpp")
target_link_libraries(CrispMeshIoTest PRIVATE CrispMeshIo)