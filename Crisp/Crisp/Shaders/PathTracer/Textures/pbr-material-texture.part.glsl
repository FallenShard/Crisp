#ifndef CRISP_PATH_TRACER_PBR_MATERIAL_TEXTURE_GLSL
#define CRISP_PATH_TRACER_PBR_MATERIAL_TEXTURE_GLSL

// The including shader supplies CRISP_MATERIAL_TEXTURE(heapIndex) as a sampler2D over the material texture heap.

#ifndef CRISP_MATERIAL_TEXTURE
#error "Define CRISP_MATERIAL_TEXTURE(heapIndex) before including pbr-material-texture.part.glsl."
#endif

const uint kInvalidMaterialTextureOffset = 0xFFFFFFFFu;

// Must match the per-instance texture order written by PathTracedView's constructor.
const uint kMaterialBaseColorTexture = 0u;
const uint kMaterialNormalTexture = 1u;
const uint kMaterialOrmTexture = 2u;
const uint kMaterialEmissionTexture = 3u;

vec4 sampleMaterialTexture(const uint textureOffset, const uint textureIndex, const vec2 texCoord) {
    const uint heapIndex = nonuniformEXT(textureOffset + textureIndex);
    // Ray tracing stages have no implicit screen-space derivatives; use the base mip until ray differentials land.
    return textureLod(CRISP_MATERIAL_TEXTURE(heapIndex), texCoord, 0.0f);
}

// Folds the textured channels into the material record and returns the surface's emitted radiance. The hit
// shader samples a BSDF from the result and the raygen evaluates the same BSDF for next-event estimation, so
// the two have to agree to the bit; that is why this lives in one function rather than in both shaders.
vec3 applyMaterialTextures(inout PbrMaterialParameters material, const uint textureOffset, const vec2 texCoord) {
    const vec3 emission = max(material.emissionColor, vec3(0.0f)) * max(material.emissionLuminance, 0.0f);
    if (textureOffset == kInvalidMaterialTextureOffset) {
        return emission;
    }

    const vec2 scaledTexCoord = texCoord * material.uvScale;
    material.baseColor *= sampleMaterialTexture(textureOffset, kMaterialBaseColorTexture, scaledTexCoord).rgb;

    const vec3 ormSample = sampleMaterialTexture(textureOffset, kMaterialOrmTexture, scaledTexCoord).rgb;
    material.baseMetalness *= ormSample.b;
    material.specularRoughness *= ormSample.g;

    return sampleMaterialTexture(textureOffset, kMaterialEmissionTexture, scaledTexCoord).rgb * emission;
}

#endif // CRISP_PATH_TRACER_PBR_MATERIAL_TEXTURE_GLSL
