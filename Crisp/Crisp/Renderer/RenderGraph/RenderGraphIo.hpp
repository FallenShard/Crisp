#pragma once

#include <filesystem>

#include <Crisp/Core/Result.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>

namespace crisp {

Result<> toGraphViz(const rg::RenderGraph& renderGraph, const std::filesystem::path& outputPath);

} // namespace crisp