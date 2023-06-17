add_cpp_static_library(CrispGeometry
    "Geometry.cpp"
    "Geometry.hpp"
    "GeometryView.hpp"
    "TransformBuffer.cpp"
    "TransformBuffer.hpp"
    "TransformPack.hpp"
)
target_link_libraries(CrispGeometry
    PUBLIC Crisp::Math
    PUBLIC Crisp::Mesh
    PUBLIC Crisp::Renderer
    PUBLIC Crisp::CoreUtils
    PUBLIC Crisp::VertexLayout)

add_cpp_static_library(CrispVertexLayout
    "VertexLayout.cpp"
    "VertexLayout.hpp"
)
target_link_libraries(CrispVertexLayout
    PUBLIC Crisp::VulkanHeader
    PUBLIC Crisp::Mesh
    PRIVATE Crisp::Logger
    PRIVATE Crisp::Result
)