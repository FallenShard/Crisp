#version 450 core

in VsOut
{
	vec3 position;
	vec3 normal;
} fsIn;

layout(location = 0) out vec4 finalColor;

layout(set = 0, binding = 1) uniform CameraParams
{
	mat4 V;
	mat4 P;
	vec2 screenSize;
	vec2 nearFar;
};

layout(set = 0, binding = 2) uniform sampler s;
layout(set = 0, binding = 3) uniform textureCube cubeMap;

//layout(input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput sceneTex;
layout(set = 1, binding = 0) uniform texture2D sceneTex;

float fresnelDielectric(float cosThetaI, float extIOR, float intIOR)
{
	float etaI = extIOR, etaT = intIOR;

	// If indices of refraction are the same, no fresnel effects
	if (extIOR == intIOR)
		return 0.0f;

	// if cosThetaI is < 0, it means the ray is coming from inside the object
	if (cosThetaI < 0.0f)
	{
		float t = etaI;
		etaI = etaT;
		etaT = t;
		cosThetaI = -cosThetaI;
	}

	float eta = etaI / etaT;
	float sinThetaTSqr = eta * eta * (1.0f - cosThetaI * cosThetaI);

	// Total internal reflection
	if (sinThetaTSqr > 1.0f)
		return 1.0f;

	float cosThetaT = sqrt(1.0f - sinThetaTSqr);

	float Rs = (etaI * cosThetaI - etaT * cosThetaT) / (etaI * cosThetaI + etaT * cosThetaT);
	float Rp = (etaT * cosThetaI - etaI * cosThetaT) / (etaT * cosThetaI + etaI * cosThetaT);
	return (Rs * Rs + Rp * Rp) / 2.0f;
}

void main()
{
	vec2 texCoords = gl_FragCoord.xy / screenSize + 0.2f;
	//finalColor = subpassLoad(sceneTex); //vec4(2 * fsIn.position, 1.0f);
	finalColor = vec4(texture(sampler2D(sceneTex, s), texCoords).rgb, 1.0f);

	vec3 normal = normalize(fsIn.normal);
	float extIOR = 1.00000;
	float intIOR = 1.33;

	vec3 viewDir = normalize(fsIn.position);

	float cosTheta = dot(-viewDir, normal);

	float fresnel = fresnelDielectric(cosTheta, extIOR, intIOR);

	vec3 reflectionDir = reflect(viewDir, normal);
	vec3 worldReflectDir = (transpose(V) * vec4(reflectionDir, 0.0f)).xyz;
	vec3 reflectedColor = texture(samplerCube(cubeMap, s), worldReflectDir).xyz;

	vec3 refractionDir = refract(viewDir, normal, extIOR / intIOR);
	vec3 worldRefractDir = (transpose(V) * vec4(refractionDir, 0.0f)).xyz;
	vec3 refractedColor = texture(samplerCube(cubeMap, s), worldRefractDir).xyz;

	float thickness = abs(normal.z) * 2;

	vec3 extinction = vec3(1.0f) - vec3(0.1f, 0.5f, 0.9f);
	vec3 transmission = exp(-extinction * thickness);

	finalColor = vec4(mix(refractedColor * transmission, reflectedColor, fresnel), 1.0f);

	//finalColor = vec4(viewDir, 1.0f);
	//finalColor = vec4(texCoords, 0.0f, 1.0f);
}