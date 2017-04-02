#version 450 core

layout(location = 0) in	vec3 eyePos;
layout(location = 1) in	vec3 eyeNormal;

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

bool analyticIntersection(in vec3 center, float radius, in vec3 origin, in vec3 dir, out float t)
{
	t = 0.0f;
	vec3 centerToOrigin = origin - center;

	float b = 2.0f * dot(dir, centerToOrigin);
	float c = dot(centerToOrigin, centerToOrigin) - radius * radius;

	float discrim = b * b - 4.0f * c;

	if (discrim < 0.0f) // both complex solutions, no intersection
	{
		return false;
	}
	else if (discrim < 1e-4) // Discriminant is practically zero
	{
		t = -0.5f * b;
	}
	else
	{
		float factor = (b > 0.0f) ? -0.5f * (b + sqrt(discrim)) : -0.5f * (b - sqrt(discrim));
		float t0 = factor;
		float t1 = c / factor;

		if (t0 > t1) // Prefer the smaller t, select larger if smaller is less than tNear
		{
			t = t1 < 1e-4 ? t0 : t1;
		}
		else
		{
			t = t0 < 1e-4 ? t1 : t0;
		}
	}

	if (t < 1e-4 || t > 1e10)
		return false;
	else
		return true;
}

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

vec3 getReflectedColor(in vec3 viewDir, in vec3 viewNormal)
{
	vec3 reflectionDir = reflect(viewDir, viewNormal);
	vec3 worldReflectDir = (transpose(V) * vec4(reflectionDir, 0.0f)).xyz;
	return texture(samplerCube(cubeMap, s), worldReflectDir).xyz;
}

vec3 getRefractedColor(in vec3 viewDir, in vec3 viewNormal, in vec3 viewPosition, float etaRatio)
{
	vec3 firstRefractionDir = refract(viewDir, viewNormal, etaRatio);
	vec3 worldFirstRefractionDir = (transpose(V) * vec4(firstRefractionDir, 0.0f)).xyz;
	vec3 worldPos = (transpose(V) * vec4(viewPosition, 0.0f)).xyz;

	float t;
	bool hasIntersection = analyticIntersection(vec3(0), 1, worldPos, worldFirstRefractionDir, t);

	vec3 backFacePos = worldPos + t * worldFirstRefractionDir;
	vec3 backFaceNormal = -normalize(backFacePos);
	vec3 secondRefractionDir = refract(worldFirstRefractionDir, backFaceNormal, 1 / etaRatio);
	return texture(samplerCube(cubeMap, s), secondRefractionDir).xyz;
}

void main()
{
	float extIOR = 1.00029;
	float intIOR = 1.52;

	//finalColor = subpassLoad(sceneTex); //vec4(2 * fsIn.position, 1.0f);

	vec3 normal = normalize(eyeNormal);
	vec3 viewDir = normalize(eyePos);

	float cosTheta = dot(-viewDir, normal);
	float fresnel = fresnelDielectric(cosTheta, extIOR, intIOR);

	// float thickness = abs(normal.z) * 2;

	// vec3 extinction = vec3(1.0f) - vec3(0.1f, 0.5f, 0.9f);
	// vec3 transmission = exp(-extinction * thickness);

	// finalColor = vec4(mix(refractedColor * transmission, reflectedColor, fresnel), 1.0f);

	vec3 reflection = getReflectedColor(viewDir, normal);
	vec3 refraction = getRefractedColor(viewDir, normal, normal, extIOR / intIOR);

	finalColor = vec4(mix(refraction, reflection, fresnel), 1.0f);
}