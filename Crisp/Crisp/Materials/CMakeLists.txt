add_cpp_static_library(CrispMaterial
    "PbrMaterial.hpp"
    "PbrMaterial.cpp")
target_link_libraries(CrispMaterial
    PUBLIC Crisp::Math
    PUBLIC Crisp::Image)

add_cpp_static_library(CrispMaterialUtils
    "PbrMaterialUtils.hpp"
    "PbrMaterialUtils.cpp")
target_link_libraries(CrispMaterialUtils
    PUBLIC Crisp::Material
    PUBLIC Crisp::Renderer)