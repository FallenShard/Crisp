#pragma once

#include <Crisp/PathTracer/Cameras/Camera.hpp>

#include <functional>
#include <memory>
#include <string>

namespace crisp {
class CameraFactory {
public:
    static std::unique_ptr<Camera> create(std::string type, VariantMap parameters);
};
} // namespace crisp