#pragma once

#include <memory>
#include <string>

namespace crisp
{
    class Scene;

    class XmlSceneParser
    {
    public:
        std::unique_ptr<Scene> parse(const std::string& fileName);
    };
}