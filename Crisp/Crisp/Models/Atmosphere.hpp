#pragma once

#include <Crisp/Renderer/RenderNode.hpp>
#include <Crisp/Renderer/RenderPasses/AtmospherePasses.hpp>

#include <CrispCore/RobinHood.hpp>

#include <string>

namespace crisp
{
class RenderGraph;
class Renderer;
class ResourceContext;

robin_hood::unordered_flat_map<std::string, std::unique_ptr<RenderNode>> addAtmosphereRenderPasses(
    RenderGraph& renderGraph, Renderer& renderer, ResourceContext& resourceContext, const std::string& dependentPass);

} // namespace crisp