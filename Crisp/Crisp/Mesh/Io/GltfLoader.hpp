#pragma once

#include <filesystem>

#include <Crisp/Core/Result.hpp>
#include <Crisp/Materials/PbrMaterial.hpp>
#include <Crisp/Mesh/Io/ModelData.hpp>

namespace crisp {

Result<SceneData> loadGltfAsset(const std::filesystem::path& path);

} // namespace crisp
