#include "CameraFactory.hpp"

#include "Camera.hpp"
#include "Perspective.hpp"

namespace crisp {
std::unique_ptr<Camera> CameraFactory::create(std::string type, VariantMap parameters) {
    if (type == "perspective") {
        return std::make_unique<PerspectiveCamera>(parameters);
    } else {
        std::cerr << "Unknown camera type \"" << type << "\" requested! Creating default perspective camera"
                  << std::endl;
        return std::make_unique<PerspectiveCamera>(parameters);
    }
}
} // namespace crisp