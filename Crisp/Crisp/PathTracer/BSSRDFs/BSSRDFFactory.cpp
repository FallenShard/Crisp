#include "BSSRDFFactory.hpp"

#include "DipoleBSSRDF.hpp"

namespace crisp {
std::unique_ptr<BSSRDF> BSSRDFFactory::create(std::string type, VariantMap parameters) {
    if (type == "dipole") {
        return std::make_unique<DipoleBSSRDF>(parameters);
    } else {
        std::cerr << "Unknown bssrdf type \"" << type << "\" requested! Creating default dipole bssrdf" << std::endl;
        return std::make_unique<DipoleBSSRDF>(parameters);
    }
}
} // namespace crisp