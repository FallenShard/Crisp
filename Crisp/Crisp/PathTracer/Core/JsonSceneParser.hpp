#pragma once

#include <filesystem>
#include <memory>

#include <Crisp/Core/Result.hpp>

namespace crisp {
namespace pt {
class Scene;
} // namespace pt

class JsonSceneParser {
public:
    Result<std::unique_ptr<pt::Scene>> parse(
        const std::filesystem::path& sceneFilePath, const std::filesystem::path& meshDirectory);
};
} // namespace crisp
