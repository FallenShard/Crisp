#pragma once

#include <memory>
#include <string>

namespace vesper
{
    class Scene;

    class JsonSceneParser
    {
    public:
        std::unique_ptr<Scene> parse(const std::string& fileName);

    };
}