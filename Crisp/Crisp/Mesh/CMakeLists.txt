add_cpp_static_library(CrispMesh
    "MeshOptimizer.cpp"
    "MeshOptimizer.hpp"
    "Meshlet.hpp"
    "MeshletBuilder.cpp"
    "MeshletBuilder.hpp"
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
    PRIVATE meshoptimizer
    PUBLIC Crisp::HashMap)

add_cpp_test(CrispMeshTest "Test/TriangleMeshTest.cpp")
target_link_libraries(CrispMeshTest PRIVATE Crisp::Mesh)

add_cpp_test(CrispMeshOptimizerTest "Test/MeshOptimizerTest.cpp")
target_link_libraries(CrispMeshOptimizerTest PRIVATE Crisp::Mesh)

add_cpp_test(CrispMeshletBuilderTest "Test/MeshletBuilderTest.cpp")
target_link_libraries(CrispMeshletBuilderTest PRIVATE Crisp::Mesh)

add_subdirectory(Io)
