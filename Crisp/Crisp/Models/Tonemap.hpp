#pragma once

#include <array>
#include <cstdint>

#include <Crisp/Math/Headers.hpp>

#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>

namespace crisp {
class Renderer;
class ResourceContext;

inline constexpr std::array<const char*, 4> kTonemapOperatorNames{
    "None (clamp)",
    "Reinhard (extended)",
    "ACES (Hill fit)",
    "Uchimura (GT)",
};

// Mirrors the TonemapParams struct in Shaders/Common/tonemap.part.glsl.
struct TonemapParameters {
    // Linear scale applied to scene radiance before the curve. Owns what the atmosphere used to apply itself.
    float exposure{5.0f};

    // Indexes kTonemapOperatorNames.
    int32_t operatorIndex{2};

    // Reinhard only: the exposed radiance that maps to 1.
    float whitePoint{4.0f};

    // The remaining four are Uchimura's curve controls and are ignored by the other operators.
    float contrast{1.0f};

    float linearStart{0.22f};
    float linearLength{0.4f};
    float blackTightness{1.33f};
    float pedestal{0.0f};
};

struct TonemapPassData {
    RenderGraphResourceHandle image;
};

// Reads scene radiance from hdrImage and writes a display referred, still linear image; the sRGB encode stays the
// last step of the chain. Expects a uniform ring buffer registered under kTonemapBufferId.
inline constexpr const char* kTonemapBufferId = "tonemapBuffer";

void addTonemapPass(
    rg::RenderGraph& renderGraph,
    Renderer& renderer,
    ResourceContext& resourceContext,
    RenderGraphResourceHandle hdrImage);

} // namespace crisp
