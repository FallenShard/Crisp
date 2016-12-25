#pragma once

#include <string>
#include "Math/Headers.hpp"

namespace crisp
{
    class StringUtils
    {
    public:
        static std::string toString(const glm::vec3& vec, unsigned int precision = 2);
        static std::string toString(const glm::vec4& vec, unsigned int precision = 2);
        static std::string toString(const glm::quat& quat, unsigned int precision = 2);
    };
}