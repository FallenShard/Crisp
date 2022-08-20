add_cpp_static_library(CrispMaterial
    "PbrMaterial.hpp"
    "PbrMaterial.cpp")
target_include_directories(CrispMaterial PUBLIC ".")
target_link_libraries(CrispMaterial
    PUBLIC CrispMath
    PUBLIC CrispImage)


add_cpp_static_library(CrispMaterialUtils
    "PbrMaterialUtils.hpp"
    "PbrMaterialUtils.cpp")
target_include_directories(CrispMaterialUtils PUBLIC ".")
target_link_libraries(CrispMaterialUtils
    PUBLIC CrispMaterial
    PUBLIC CrispRenderer)