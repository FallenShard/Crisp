#pragma once

#include <filesystem>
#include <memory>

namespace crisp {
namespace pt {
class Scene;
}

class XmlSceneParser {
public:
    std::unique_ptr<pt::Scene> parse(
        const std::filesystem::path& sceneFilePath, const std::filesystem::path& meshDirectory);
};
} // namespace crisp
