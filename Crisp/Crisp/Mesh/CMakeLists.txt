add_cpp_static_library(CrispMesh
    "TriangleMesh.cpp"
    "TriangleMesh.hpp"
    "TriangleMeshUtils.cpp"
    "TriangleMeshUtils.hpp"
    "TriangleMeshView.hpp"
    "VertexAttributeBuffer.hpp"
    "VertexAttributeDescriptor.cpp"
    "VertexAttributeDescriptor.hpp"
    "VertexAttributeTraits.hpp"
    "SkinningData.hpp")
target_link_libraries(CrispMesh
    PUBLIC Crisp::Math
    PRIVATE Crisp::Logger
    PUBLIC Crisp::HashMap)

add_subdirectory(Io)