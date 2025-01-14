add_cpp_static_library(CrispModels

    # "Atmosphere.cpp"
    # "Atmosphere.hpp"
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

    # "SPH.cpp"
    # "SPH.hpp"
)
target_link_libraries(CrispModels
    PUBLIC Crisp::Renderer
    PUBLIC Crisp::Window
)

add_cpp_test(CrispOceanTest
    "Test/OceanTest.cpp"
)
target_link_libraries(CrispOceanTest
    PRIVATE Crisp::VulkanTestUtils
    PRIVATE Crisp::Models
)