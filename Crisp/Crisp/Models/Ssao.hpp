#pragma once

#include <cstdint>
#include <string>

#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>

namespace crisp {
class Renderer;
class ResourceContext;

// Mirrors the AoParams push constant block in Shaders/ssao.frag.glsl.
struct SsaoParameters {
    int32_t sampleCount{128};

    float radius{0.5f};
};

struct SsaoPassData {
    RenderGraphResourceHandle image;
};

// Must match the samples[] array length in Shaders/ssao.frag.glsl.
inline constexpr uint32_t kSsaoSampleCount = 512;

void addSsaoPass(
    rg::RenderGraph& renderGraph,
    Renderer& renderer,
    ResourceContext& resourceContext,
    RenderGraphResourceHandle depthNormalImage,
    const std::string& viewBufferId,
    const SsaoParameters& parameters);

} // namespace crisp
