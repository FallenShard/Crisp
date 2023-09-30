#include "PhaseFunctionFactory.hpp"

#include "Isotropic.hpp"

namespace crisp {
std::unique_ptr<PhaseFunction> PhaseFunctionFactory::create(std::string type, VariantMap parameters) {
    if (type == "isotropic") {
        return std::make_unique<IsotropicPhaseFunction>(parameters);
    } else {
        std::cerr << "Unknown phase function type \"" << type << "\" requested! Creating default isotropic type"
                  << std::endl;
        return std::make_unique<IsotropicPhaseFunction>(parameters);
    }
}
} // namespace crisp