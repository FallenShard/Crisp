#ifndef CRISP_PATH_TRACER_HEAP_SLOTS_GLSL
#define CRISP_PATH_TRACER_HEAP_SLOTS_GLSL

// Must match Scenes/PathTracer.hpp.
const uint kBvhSlot = 0u;
const uint kImageSlot = 1u;
const uint kViewSlot = 2u;
const uint kIntegratorSlot = 3u;
const uint kEnvironmentSlot = 4u;
const uint kGgxAlbedoLutSlot = 5u;
const uint kMediumVolumeSlot = 6u;
const uint kMaterialTextureFirstSlot = 7u;

const uint kEnvironmentSamplerSlot = 0u;
const uint kMaterialSamplerSlot = 1u;
const uint kGgxAlbedoLutSamplerSlot = 2u;
const uint kMediumSamplerSlot = 3u;

#endif // CRISP_PATH_TRACER_HEAP_SLOTS_GLSL
