add_cpp_static_library(CrispMesh
    "TriangleMesh.cpp"
    "TriangleMesh.hpp"
    "TriangleMeshUtils.cpp"
    "TriangleMeshUtils.hpp"
    "TriangleMeshView.hpp"
    "VertexAttributeBuffer.hpp"
    "VertexAttributeDescriptor.cpp"
    "VertexAttributeDescriptor.hpp"
    "VertexAttributeTraits.hpp")
target_link_libraries(CrispMesh
    PUBLIC CrispMath
    PRIVATE CrispLogger
    PUBLIC CrispHashMap)

 add_subdirectory(IO)