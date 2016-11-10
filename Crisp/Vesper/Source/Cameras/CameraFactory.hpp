#pragma once

#include <memory>
#include <string>
#include <functional>

#include "Core/VariantMap.hpp"

#include "Camera.hpp"

namespace vesper
{
    class CameraFactory
    {
    public:
        static std::unique_ptr<Camera> create(std::string type, VariantMap parameters);
    };
}