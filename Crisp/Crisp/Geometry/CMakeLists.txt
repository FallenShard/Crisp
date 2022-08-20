add_cpp_static_library(CrispGeometry
    "Geometry.cpp"
    "Geometry.hpp"
    "GeometryView.hpp"
    "TransformBuffer.cpp"
    "TransformBuffer.hpp"
    "TransformPack.hpp")
target_include_directories(CrispGeometry PUBLIC "../../")
target_link_libraries(CrispGeometry
    PUBLIC CrispMath
    PUBLIC CrispMesh
    PUBLIC CrispRenderer
    PUBLIC CrispCoreUtils)