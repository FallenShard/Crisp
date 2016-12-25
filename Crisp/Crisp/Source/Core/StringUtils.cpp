#include "StringUtils.hpp"

#include <sstream>
#include <iomanip>

namespace crisp
{
    std::string StringUtils::toString(const glm::vec3& vec, unsigned int precision)
    {
        std::stringstream stringStream;
        stringStream << std::fixed << std::setprecision(precision) << std::setfill('0') <<
            vec.x << ", " << vec.y << ", " << vec.z;
            
        return stringStream.str();
    }

    std::string StringUtils::toString(const glm::vec4& vec, unsigned int precision)
    {
        std::stringstream stringStream;
        stringStream << std::fixed << std::setprecision(precision) << std::setfill('0') <<
            vec.x << ", " << vec.y << ", " << vec.z << ", " << vec.w;

        return stringStream.str();
    }

    std::string StringUtils::toString(const glm::quat& quat, unsigned int precision)
    {
        std::stringstream stringStream;
        stringStream << std::fixed << std::setprecision(precision) << std::setfill('0') <<
            quat.x << ", " << quat.y << ", " << quat.z << ", " << quat.w;

        return stringStream.str();
    }
}