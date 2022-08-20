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
    "SPH.cpp"
    "SPH.hpp")
target_include_directories(CrispModels PUBLIC "../../")
target_link_libraries(CrispModels
    PUBLIC CrispRenderer)