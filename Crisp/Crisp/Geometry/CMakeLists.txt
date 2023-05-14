add_cpp_static_library(CrispGeometry
    "Geometry.cpp"
    "Geometry.hpp"
    "GeometryView.hpp"
    "TransformBuffer.cpp"
    "TransformBuffer.hpp"
    "TransformPack.hpp"
)
target_link_libraries(CrispGeometry
    PUBLIC CrispMath
    PUBLIC CrispMesh
    PUBLIC CrispRenderer
    PUBLIC CrispCoreUtils
    PUBLIC CrispVertexLayout)

add_cpp_static_library(CrispVertexLayout
    "VertexLayout.cpp"
    "VertexLayout.hpp"
)
target_link_libraries(CrispVertexLayout
    PUBLIC CrispVulkanHeader
    PUBLIC CrispMesh
    PRIVATE CrispLogger
    PRIVATE CrispResult
)