add_cpp_static_library(CrispMath
    "BoundingBox.hpp"
    "Constants.hpp"
    "CoordinateFrame.hpp"
    "Distribution1D.cpp"
    "Distribution1D.hpp"
    "Distribution2D.cpp"
    "Distribution2D.hpp"
    "Headers.hpp"
    "Octree.hpp"
    "Operations.hpp"
    "Ray.hpp"
    "Rect.hpp"
    "Transform.hpp"
    "Warp.cpp"
    "Warp.hpp")
target_link_libraries(CrispMath
    PUBLIC glm)