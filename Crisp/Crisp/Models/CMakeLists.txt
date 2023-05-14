add_cpp_static_library(CrispModels
    "Atmosphere.cpp"
    "Atmosphere.hpp"
    "BoxVisualizer.cpp"
    "BoxVisualizer.hpp"
    "FluidSimulation.hpp"
    "FluidSimulationParams.hpp"
    "Grass.cpp"
    "Grass.hpp"
    "PositionBasedFluid.cpp"
    "PositionBasedFluid.hpp"
    "Skybox.cpp"
    "Skybox.hpp"
    "Ocean.cpp"
    "Ocean.hpp"
    "SPH.cpp"
    "SPH.hpp")
target_link_libraries(CrispModels
    PUBLIC CrispRenderer)

add_cpp_test(CrispTest
    "Test/TransmittanceLutTest.cpp")
target_link_libraries(CrispTest PRIVATE CrispVulkanTestUtils CrispModels)
copy_shared_libs(CrispTest lua freetype)