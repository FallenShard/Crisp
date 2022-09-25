add_cpp_static_library(CrispImageIo
    "ImageLoader.cpp"
    "ImageLoader.hpp"
    "OpenEXRReader.cpp"
    "OpenEXRReader.hpp"
    "OpenEXRWriter.cpp"
    "OpenEXRWriter.hpp")
target_link_libraries(CrispImageIo
    PUBLIC CrispResult
    PUBLIC CrispImage
    PRIVATE CrispLogger
    PRIVATE CrispFormat
    PRIVATE tinyexr
    PRIVATE stb)


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


add_cpp_static_library(CrispFileUtils
    "FileUtils.cpp"
    "FileUtils.hpp")
target_link_libraries(CrispFileUtils
    PUBLIC CrispResult
    PUBLIC CrispFormat)


add_cpp_static_library(CrispFontIo
    "FontLoader.cpp"
    "FontLoader.hpp")
target_link_libraries(CrispFontIo
    PUBLIC freetype
    PRIVATE CrispLogger)