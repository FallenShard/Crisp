add_cpp_static_library(CrispModels

    "Atmosphere.cpp"
    "Atmosphere.hpp"

    # "BoxVisualizer.cpp"
    # "BoxVisualizer.hpp"
    # "FluidSimulation.hpp"
    # "FluidSimulationParams.hpp"
    # "Grass.cpp"
    # "Grass.hpp"
    "Skybox.cpp"
    "Skybox.hpp"

    "Ocean.cpp"
    "Ocean.hpp"

    "Tonemap.cpp"
    "Tonemap.hpp"

    # "SPH.cpp"
    # "SPH.hpp"
)
target_link_libraries(CrispModels
    PUBLIC Crisp::Renderer
    PUBLIC Crisp::MeshIo
    PUBLIC Crisp::Window
)

# Kept out of CrispModels so the model layer stays free of an ImGui dependency.
add_cpp_static_library(CrispAtmosphereGui
    "AtmosphereGui.cpp"
    "AtmosphereGui.hpp"
)
target_link_libraries(CrispAtmosphereGui
    PUBLIC ImGui
    PUBLIC Crisp::Models
)

add_cpp_test(CrispOceanTest
    "Test/OceanTest.cpp"
)
target_link_libraries(CrispOceanTest
    PRIVATE Crisp::VulkanTestUtils
    PRIVATE Crisp::Models
)
target_add_test_shader(CrispOceanTest "../Shaders/ocean-spectrum.comp.glsl")
target_add_test_shader(CrispOceanTest "../Shaders/ifft.comp.glsl")
target_add_test_file(
    CrispOceanTest "../Shaders/Common/math-constants.part.glsl" "Common/math-constants.part.glsl")
