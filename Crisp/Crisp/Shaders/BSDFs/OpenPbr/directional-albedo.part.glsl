#ifndef CRISP_OPENPBR_DIRECTIONAL_ALBEDO_GLSL
#define CRISP_OPENPBR_DIRECTIONAL_ALBEDO_GLSL

// Directional albedo E(mu, alpha) of the single-scattering GGX lobe with F = 1, baked by
// tools/bake_ggx_albedo_lut.py into GgxAlbedoLut.exr: R = E(mu, alpha), G = E_avg(alpha).

// A heap subscript cannot be written in a shared header, so the including shader supplies the sampler; a silent
// fallback would make every compensation mode a no-op with no diagnostic.
#ifndef CRISP_GGX_ALBEDO_LUT
#error "Define CRISP_GGX_ALBEDO_LUT as a sampler2D before including directional-albedo.part.glsl."
#endif

// Must match kGgxAlbedoLutExtent in Renderer/GgxAlbedoLut.hpp and LUT_SIZE in tools/bake_ggx_albedo_lut.py.
const uint kGgxAlbedoLutSize = 64;

// Matches the endpoint-mapped bake (texel i holds parameter i/(N-1)); a texel-centred read costs ~2% at both
// ends. Second axis is roughness = sqrt(alpha), which E varies far less sharply in.
vec2 ggxAlbedoLutUv(const float cosTheta, const float alpha) {
    const vec2 param = clamp(vec2(cosTheta, sqrt(alpha)), 0.0f, 1.0f);
    return (param * float(kGgxAlbedoLutSize - 1u) + 0.5f) / float(kGgxAlbedoLutSize);
}

float ggxDirectionalAlbedo(const float cosTheta, const float alpha) {
    return textureLod(CRISP_GGX_ALBEDO_LUT, ggxAlbedoLutUv(cosTheta, alpha), 0.0f).x;
}

// Cosine-weighted average of E, baked by the same integrator: Kulla-Conty conserves energy only when this is
// genuinely the average of the E beside it. Constant along mu, so the lookup's cosTheta is arbitrary.
// Directional albedo of the Fresnel-weighted single-scattering lobe: the share of the incident energy the
// specular layer reflects at this angle, and therefore the share the substrate beneath it never receives.
// E = A + B with F = 1, so F0 * A + B is F0 * E + (1 - F0) * B. Identical in definition to what
// tools/bake_brdf_lut.py hands the rasterizer, which is what keeps the two renderers on one quantity.
vec3 specularDirectionalAlbedo(const float cosTheta, const float alpha, const vec3 f0) {
    const vec4 lut = textureLod(CRISP_GGX_ALBEDO_LUT, ggxAlbedoLutUv(cosTheta, alpha), 0.0f);
    return f0 * lut.x + (1.0f - f0) * lut.z;
}

float ggxAverageAlbedo(const float alpha) {
    return textureLod(CRISP_GGX_ALBEDO_LUT, ggxAlbedoLutUv(0.0f, alpha), 0.0f).y;
}

#endif // CRISP_OPENPBR_DIRECTIONAL_ALBEDO_GLSL
