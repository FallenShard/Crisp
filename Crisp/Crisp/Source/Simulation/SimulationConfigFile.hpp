#pragma once

#include <string>

#include "FluidSimulationParams.hpp"

namespace crisp
{
    class SimulationConfigFile
    {
    public:
        SimulationConfigFile(const std::string filePath);
        ~SimulationConfigFile();

        FluidSimulationParams getFluidSimulationParams();

    private:
        void parse();

        std::string m_filePath;
    };
}