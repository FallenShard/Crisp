add_cpp_static_library(CrispModels

    "Atmosphere.cpp"
    "Atmosphere.hpp"

    "Skybox.cpp"
    "Skybox.hpp"

    "Ocean.cpp"
    "Ocean.hpp"

    "Tonemap.cpp"
    "Tonemap.hpp"

    "Ssao.cpp"
    "Ssao.hpp"

    "SPH.cpp"
    "SPH.hpp"

    "Pbf.cpp"
    "Pbf.hpp"
)
target_link_libraries(CrispModels
    PUBLIC Crisp::Renderer
    PUBLIC Crisp::RenderGraph
    PUBLIC Crisp::MeshIo
    PUBLIC Crisp::Window
)

add_cpp_static_library(CrispAtmosphereGui
    "AtmosphereGui.cpp"
    "AtmosphereGui.hpp"
)
target_link_libraries(CrispAtmosphereGui
    PUBLIC ImGui
    PUBLIC Crisp::Models
)

add_cpp_test(CrispPbfTest
    "Test/PbfTest.cpp"
)
target_link_libraries(CrispPbfTest
    PRIVATE Crisp::Models
    PRIVATE Crisp::Renderer
    PRIVATE Crisp::RenderGraph
    PRIVATE Crisp::Application
)

add_cpp_test(
    CrispOceanTest
    "Test/OceanTest.cpp"
    DEPS
    Crisp::VulkanTestUtils
    Crisp::Models
    Crisp::ShaderCompiler
    Crisp::UniqueTemporaryFile
    ASSET_DIR
    "../Shaders"
)
