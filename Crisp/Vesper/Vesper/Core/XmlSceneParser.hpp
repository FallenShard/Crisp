#pragma once

#include <memory>
#include <string>

namespace vesper
{
    class Scene;

    class XmlSceneParser
    {
    public:
        std::unique_ptr<Scene> parse(const std::string& fileName);
    };
}