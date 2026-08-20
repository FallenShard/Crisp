add_cpp_static_library(
    CrispMaterial
    "PbrMaterial.hpp"
    "PbrMaterial.cpp"
)
target_link_libraries(
    CrispMaterial
    PUBLIC Crisp::Math
    PUBLIC Crisp::Image
    PUBLIC Crisp::Format
    PUBLIC Crisp::Checks
)

add_cpp_static_library(
    CrispMaterialUtils
    "PbrMaterialUtils.hpp"
    "PbrMaterialUtils.cpp"
    "PbrMaterialTable.hpp"
    "PbrMaterialTable.cpp"
)
target_link_libraries(
    CrispMaterialUtils
    PUBLIC Crisp::Material
    PUBLIC Crisp::Renderer
)

add_cpp_test(
    CrispPbrMaterialTest
    "Test/PbrMaterialTest.cpp"
)
target_link_libraries(
    CrispPbrMaterialTest
    PRIVATE Crisp::Material
)

add_cpp_test(
    CrispPbrMaterialTableTest
    "Test/PbrMaterialTableTest.cpp"
)
target_link_libraries(
    CrispPbrMaterialTableTest
    PRIVATE Crisp::MaterialUtils
    PRIVATE Crisp::VulkanTestUtils
)
