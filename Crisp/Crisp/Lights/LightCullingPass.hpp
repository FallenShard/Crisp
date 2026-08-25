#pragma once

#include <string>

#include <Crisp/Lights/LightSystem.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>

namespace crisp {
class Renderer;
class ResourceContext;

void addLightCullingPass(
    rg::RenderGraph& renderGraph,
    Renderer& renderer,
    ResourceContext& resourceContext,
    LightSystem& lightSystem,
    RenderGraphResourceHandle depthImage,
    const std::string& viewBufferId);

} // namespace crisp
