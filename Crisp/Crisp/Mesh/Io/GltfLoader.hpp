#pragma once

#include <Crisp/Core/Result.hpp>
#include <Crisp/Materials/PbrMaterial.hpp>
#include <Crisp/Mesh/Io/ModelData.hpp>

#include <filesystem>

namespace crisp {

Result<std::vector<ModelData>> loadGltfModel(const std::filesystem::path& path);

} // namespace crisp
