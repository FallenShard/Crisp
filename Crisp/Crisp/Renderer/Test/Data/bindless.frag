#version 450

layout(set = 0, binding = 0) uniform CameraData { mat4 viewProjection; } cameraData;
layout(set = 0, binding = 1) uniform MaterialData { vec4 tint; } materialData;
layout(set = 0, binding = 2) uniform sampler2D baseColor;
layout(set = 0, binding = 3) uniform sampler2D normalMap;
layout(set = 0, binding = 4) uniform sampler2D materialTextures[4];
layout(set = 0, binding = 5) uniform sampler2D occlusionMap;
layout(set = 0, binding = 6) uniform sampler2D emissiveMap;

layout(set = 1, binding = 0) uniform SceneData { vec4 exposure; } sceneData;
layout(set = 1, binding = 1) uniform sampler2D environmentMaps[6];

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 fragmentColor;

void main() {
    fragmentColor = materialData.tint + cameraData.viewProjection[0] + sceneData.exposure;
    fragmentColor += texture(baseColor, texCoord) + texture(normalMap, texCoord);
    fragmentColor += texture(materialTextures[0], texCoord) + texture(materialTextures[3], texCoord);
    fragmentColor += texture(occlusionMap, texCoord) + texture(emissiveMap, texCoord);
    fragmentColor += texture(environmentMaps[0], texCoord) + texture(environmentMaps[5], texCoord);
}
