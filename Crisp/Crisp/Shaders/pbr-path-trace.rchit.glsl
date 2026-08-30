#version 460 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "Common/math-constants.part.glsl"
#include "Common/warp.part.glsl"
#include "PathTracer/Core/pbr-hit.part.glsl"
#include "PathTracer/Core/pbr-scene.part.glsl"
#include "PathTracer/BSDFs/pbr-surface.part.glsl"

layout(location = 0) rayPayloadInEXT PbrHitInfo hitInfo;

hitAttributeEXT vec2 barycentric;

void main() {
    // Not const: the record holds buffer references, which GLSL forbids qualifying.
    PathTracedInstance instance = scene.instances.data[gl_InstanceCustomIndexEXT];
    const uvec3 triangle = instance.triangles.data[gl_PrimitiveID];

    const vec3 baryCoord = vec3(1.0f - barycentric.x - barycentric.y, barycentric.x, barycentric.y);
    const vec3 objectNormal = normalize(
        instance.attributes.data[triangle.x].normal * baryCoord.x +
        instance.attributes.data[triangle.y].normal * baryCoord.y +
        instance.attributes.data[triangle.z].normal * baryCoord.z);

    // Normals transform by the inverse transpose; gl_WorldToObjectEXT already is the inverse, so a row-vector
    // multiply transposes it without inverting anything here.
    const vec3 worldNormal = normalize(objectNormal * mat3(gl_WorldToObjectEXT));

    hitInfo.position = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;
    hitInfo.tHit = gl_HitTEXT;

    const PbrMaterialParameters material = scene.materials.data[instance.materialIndex];
    hitInfo.emission = max(material.emissionColor, vec3(0.0f)) * max(material.emissionLuminance, 0.0f);

    // Shade against the side the ray arrived on, so a double-sided surface does not go black from behind.
    const vec3 wiWorld = -gl_WorldRayDirectionEXT;
    const vec3 shadingNormal = dot(worldNormal, wiWorld) < 0.0f ? -worldNormal : worldNormal;
    const mat3 frame = createCoordinateFrame(shadingNormal);
    const vec3 wi = transpose(frame) * wiWorld;

    const PbrSurface surface = createPbrSurface(material);

    vec3 wo;
    float pdf;
    bool sampledSpecular;
    const vec3 weight = samplePbrSurface(surface, hitInfo.unitSample, wi, wo, pdf, sampledSpecular);

    hitInfo.sampleDirection = frame * wo;
    hitInfo.samplePdf = pdf;
    hitInfo.sampleWeight = weight;
    hitInfo.sampleIsSpecular = sampledSpecular ? 1u : 0u;
}
