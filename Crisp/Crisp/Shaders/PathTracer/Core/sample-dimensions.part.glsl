#ifndef CRISP_PATH_TRACER_SAMPLE_DIMENSIONS_GLSL
#define CRISP_PATH_TRACER_SAMPLE_DIMENSIONS_GLSL

// Fixed layout of the sample vector. Every bounce restarts the sampler cursor at
// kDimBounceBase + bounce * kDimsPerBounce, so a path that skips light sampling on a delta bounce or has not
// reached the Russian-roulette cutoff still consumes the same dimensions as one that does. A free-running
// cursor would let neighbouring pixels disagree on what dimension d means.
//
// The light budget is sized for the widest sampler -- an area light drawing a triangle and a position on it --
// so an integrator that only ever samples the environment still steps by the same stride.
const uint kDimPixelFilter = 0u; // 2 dimensions.
const uint kDimBounceBase = 2u;
const uint kDimsPerBounce = 9u;
const uint kDimBsdf = 0u;            // 3 dimensions, relative to the bounce base.
const uint kDimLight = 3u;           // 5 dimensions.
const uint kDimRussianRoulette = 8u; // 1 dimension.

#endif // CRISP_PATH_TRACER_SAMPLE_DIMENSIONS_GLSL
