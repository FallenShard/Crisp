add_cpp_test(
    CrispBlurPassTest
    "Test/BlurPassTest.cpp"
    "BlurPass.hpp"
)
target_link_libraries(
    CrispBlurPassTest
    PRIVATE Crisp::VulkanTestUtils
    PRIVATE Crisp::RenderGraph
)

add_cpp_test(
    CrispCubeMapPassTest
    "Test/CubeMapPassTest.cpp"
    "CubeMapRenderPass.hpp"
)
target_link_libraries(
    CrispCubeMapPassTest
    PRIVATE Crisp::VulkanTestUtils
    PRIVATE Crisp::RenderGraph
)

add_cpp_test(
    CrispDepthPassTest
    "Test/DepthPassTest.cpp"
    "DepthPass.hpp"
)
target_link_libraries(
    CrispDepthPassTest
    PRIVATE Crisp::VulkanTestUtils
    PRIVATE Crisp::RenderGraph
)

add_cpp_test(
    CrispLightShaftPassTest
    "Test/LightShaftPassTest.cpp"
    "LightShaftPass.hpp"
)
target_link_libraries(
    CrispLightShaftPassTest
    PRIVATE Crisp::VulkanTestUtils
    PRIVATE Crisp::RenderGraph
)

add_cpp_test(
    CrispLiquidPassTest
    "Test/LiquidPassTest.cpp"
    "LiquidRenderPass.hpp"
)
target_link_libraries(
    CrispLiquidPassTest
    PRIVATE Crisp::VulkanTestUtils
    PRIVATE Crisp::RenderGraph
)

add_cpp_test(
    CrispTexturePassTest
    "Test/TexturePassTest.cpp"
    "TexturePass.hpp"
)
target_link_libraries(
    CrispTexturePassTest
    PRIVATE Crisp::VulkanTestUtils
    PRIVATE Crisp::RenderGraph
)
