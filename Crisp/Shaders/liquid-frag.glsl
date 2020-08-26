#version 450 core
 
layout(location = 0) in vec2 fsTexCoord;

layout(location = 0) out vec4 finalColor;

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput normalDepthTex;
layout(set = 0, binding = 1) uniform sampler2D sceneTex;
layout(set = 0, binding = 2) uniform CameraParams
{
    mat4 V;
    mat4 P;
    vec2 screenSize;
    vec2 nearFar;
};

layout(set = 0, binding = 3) uniform sampler2D sceneDepthTex;
layout(set = 0, binding = 4) uniform samplerCube cubeMap;

float getEyeDepth(float fragDepth) // Used to transform scene depth for comparison
{
    return P[3][2] / (1 - 2 * fragDepth - P[2][2]);
}

vec3 screenToEye(vec2 uv, float eyeDepth)
{
    uv = 2.0f * uv - 1.0f;
    return vec3(-uv.x / P[0][0], uv.y / P[1][1], 1.0f) * eyeDepth;
}

vec4 projectToScreen(vec3 eyePos)
{
    mat4 invY = mat4(1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
    vec4 clipPos = invY * P * vec4(eyePos, 1.0);
    float invW = 1 / clipPos.w;
    vec3 ndcPos = clipPos.xyz * invW; // NDC
    vec3 screenPos = ndcPos * 0.5 + 0.5;
    return vec4(screenPos * vec3(screenSize, 1), invW);
}

float distanceSquared(vec2 a, vec2 b)
{
    a -= b;
    return dot(a, a);
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

const float ssStride = 1.0f;
const float ssJitter = 0.0f;
const float ssMaxRaySteps = 300;

bool traceScreenSpaceReflection(vec3 rayOrig, vec3 rayDir, out vec2 hitPixel, out float alpha)
{
    // Params
    float zThickness      = 0.1;
    float maxRayTraceDist = 20;

    // Clip ray length to near plane (important when reflection is facing the camera)
    float rayLength = ((rayOrig.z + rayDir.z * maxRayTraceDist) > -nearFar.x) ? 
                      (-nearFar.x - rayOrig.z) / rayDir.z : maxRayTraceDist;

    vec3 rayEndPoint = rayOrig + rayDir * rayLength;

    // Project into screen space: vec4([0...W], [0...H], [0...1], 1/w)
    vec4 H0 = projectToScreen(rayOrig);
    vec4 H1 = projectToScreen(rayEndPoint);

    // These values interpolate linearly in screen-space
    float k0 = H0.w; // is invW
    float k1 = H1.w;
    vec3 Q0 = rayOrig     * k0;
    vec3 Q1 = rayEndPoint * k1;

    // Screen space values for the ray
    vec2 P0 = H0.xy; // start point
    vec2 P1 = H1.xy; // end point
 
    P1 += (distanceSquared(P0, P1) < 0.0001) ? 1.0 : 0.0;
    vec2 delta = P1 - P0;

    bool permute = false;
    if (abs(delta.x) < abs(delta.y))
    {
        permute = true;
        delta = delta.yx;
        P0 = P0.yx;
        P1 = P1.yx;
    }

    float stepDir = sign(delta.x); // +1 or -1, step with "whole" numbers in screen-space
    float invDx = stepDir / delta.x;

    vec4 PQk  = vec4(P0, Q0.z, k0);
    vec4 dPQk = vec4(vec2(stepDir, invDx * delta.y), (Q1 - Q0).z * invDx, (k1 - k0) * invDx);

    // scale derivatives here with stride... and add jitter
    dPQk *= ssStride; PQk += dPQk * ssJitter;
  
    float prevZMaxEstimate = rayOrig.z;
    float stepCount = 0.0;
  
    float rayZMin = prevZMaxEstimate;
    float rayZMax = prevZMaxEstimate;
    float sceneZMax = rayZMax + 1e4;
  
    float end = P1.x * stepDir;
  
    bool inBounds = true;
    while ((PQk.x * stepDir <= end) &&
           (stepCount < ssMaxRaySteps) &&
           ((rayZMax < sceneZMax - zThickness) || (rayZMin > sceneZMax)) &&
           inBounds)
    {
        hitPixel = permute ? PQk.yx : PQk.xy;

        rayZMin = prevZMaxEstimate;
        rayZMax = (0.5 * dPQk.z + PQk.z) / (0.5 * dPQk.w + PQk.w);

        prevZMaxEstimate = rayZMax;

        if (rayZMin > rayZMax)
        {
            float t = rayZMin;
            rayZMin = rayZMax;
            rayZMax = t;
        }

        ivec2 unTexCoord = ivec2(hitPixel);

        if (any(greaterThan(abs(hitPixel / screenSize - 0.5), vec2(0.5))))
            inBounds = false;

        sceneZMax = getEyeDepth(texelFetch(sceneDepthTex, unTexCoord, 0).r);

        PQk += dPQk;
        stepCount += 1.0;
    }

    vec2 absNdcHit = abs(hitPixel / screenSize * 2 - 1);
    alpha = 1.0 - max(absNdcHit.x, absNdcHit.y);
    alpha *= 1.0 - (stepCount / ssMaxRaySteps);
    alpha *= -rayDir.z * 0.5f + 0.5f;

    return (rayZMax >= sceneZMax - zThickness) && (rayZMin <= sceneZMax);
}

const float extIOR = 1.00f;
const float intIOR = 1.33f;

void main()
{
    vec4 normalDepth = subpassLoad(normalDepthTex);
    vec3 sceneColor = texture(sceneTex, fsTexCoord).rgb;
    float sceneDepth = texture(sceneDepthTex, fsTexCoord).r;

    vec3 normal = normalDepth.rgb;

    if (normal == vec3(0.0f) || sceneDepth < normalDepth.w)
    {
        finalColor = vec4(sceneColor, 1.0f);
        return;
    }

    vec3 viewPos = screenToEye(fsTexCoord, getEyeDepth(normalDepth.w));
    vec3 viewDir = normalize(viewPos);

    float cosTheta = dot(-viewDir, normal);
    float fresnel = fresnelDielectric(cosTheta, extIOR, intIOR);

    vec3 reflectionColor;
    vec3 reflectDir = reflect(viewDir, normal);
    vec2 hitPixel;
    float alpha;
    bool hasSSR = traceScreenSpaceReflection(viewPos, reflectDir, hitPixel, alpha);
    if (hasSSR) {
        vec2 hitCoord = hitPixel / screenSize;
        reflectionColor = texture(sceneTex, hitCoord).xyz;
    }
    else {
        vec3 worldReflectDir = (transpose(V) * vec4(reflectDir, 0.0f)).xyz;
        reflectionColor = texture(cubeMap, worldReflectDir).xyz;
    }


    vec2 refractCoord = fsTexCoord + vec2(normalDepth.xy) * 0.1f;
    vec3 refractColor = texture(sceneTex, refractCoord).rgb;

    finalColor = vec4(mix(refractColor, reflectionColor, fresnel), 1.0f);
    finalColor = vec4(reflectionColor, 1.0f);
}

//#version 450 core

//layout(location = 0) in	vec3 eyePos;
//layout(location = 1) in	vec3 eyeNormal;

//layout(location = 0) out vec4 finalColor;

//layout(set = 0, binding = 2) uniform sampler s;
//layout(set = 0, binding = 3) uniform textureCube cubeMap;

//layout(set = 1, binding = 0) uniform texture2D sceneTex;
//layout(input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput normalDepthTex;

//bool analyticIntersection(in vec3 center, float radius, in vec3 origin, in vec3 dir, out float t)
//{
//	t = 0.0f;
//	vec3 centerToOrigin = origin - center;

//	float b = 2.0f * dot(dir, centerToOrigin);
//	float c = dot(centerToOrigin, centerToOrigin) - radius * radius;

//	float discrim = b * b - 4.0f * c;

//	if (discrim < 0.0f) // both complex solutions, no intersection
//	{
//		return false;
//	}
//	else if (discrim < 1e-4) // Discriminant is practically zero
//	{
//		t = -0.5f * b;
//	}
//	else
//	{
//		float factor = (b > 0.0f) ? -0.5f * (b + sqrt(discrim)) : -0.5f * (b - sqrt(discrim));
//		float t0 = factor;
//		float t1 = c / factor;

//		if (t0 > t1) // Prefer the smaller t, select larger if smaller is less than tNear
//		{
//			t = t1 < 1e-4 ? t0 : t1;
//		}
//		else
//		{
//			t = t0 < 1e-4 ? t1 : t0;
//		}
//	}

//	if (t < 1e-4 || t > 1e10)
//		return false;
//	else
//		return true;
//}

//float fresnelDielectric(float cosThetaI, float extIOR, float intIOR)
//{
//	float etaI = extIOR, etaT = intIOR;

//	// If indices of refraction are the same, no fresnel effects
//	if (extIOR == intIOR)
//		return 0.0f;

//	// if cosThetaI is < 0, it means the ray is coming from inside the object
//	if (cosThetaI < 0.0f)
//	{
//		float t = etaI;
//		etaI = etaT;
//		etaT = t;
//		cosThetaI = -cosThetaI;
//	}

//	float eta = etaI / etaT;
//	float sinThetaTSqr = eta * eta * (1.0f - cosThetaI * cosThetaI);

//	// Total internal reflection
//	if (sinThetaTSqr > 1.0f)
//		return 1.0f;

//	float cosThetaT = sqrt(1.0f - sinThetaTSqr);

//	float Rs = (etaI * cosThetaI - etaT * cosThetaT) / (etaI * cosThetaI + etaT * cosThetaT);
//	float Rp = (etaT * cosThetaI - etaI * cosThetaT) / (etaT * cosThetaI + etaI * cosThetaT);
//	return (Rs * Rs + Rp * Rp) / 2.0f;
//}

//vec3 getReflectedColor(in vec3 viewDir, in vec3 viewNormal)
//{
//	vec3 reflectionDir = reflect(viewDir, viewNormal);
//	vec3 worldReflectDir = (transpose(V) * vec4(reflectionDir, 0.0f)).xyz;
//	return texture(samplerCube(cubeMap, s), worldReflectDir).xyz;
//}

//vec3 getRefractedColor(in vec3 viewDir, in vec3 viewNormal, in vec3 viewPosition, float etaRatio)
//{
//	vec3 firstRefractionDir = refract(viewDir, viewNormal, etaRatio);
//	vec3 worldFirstRefractionDir = (transpose(V) * vec4(firstRefractionDir, 0.0f)).xyz;
//	vec3 worldPos = (transpose(V) * vec4(viewPosition, 0.0f)).xyz;

//	float t;
//	bool hasIntersection = analyticIntersection(vec3(0), 1, worldPos, worldFirstRefractionDir, t);

//	vec3 backFacePos = worldPos + t * worldFirstRefractionDir;
//	vec3 backFaceNormal = -normalize(backFacePos);
//	vec3 secondRefractionDir = refract(worldFirstRefractionDir, backFaceNormal, 1 / etaRatio);
//	return texture(samplerCube(cubeMap, s), secondRefractionDir).xyz;
//}

//void main()
//{
//	float extIOR = 1.00029;
//	float intIOR = 1.52;

//	//finalColor = subpassLoad(sceneTex); //vec4(2 * fsIn.position, 1.0f);

//	vec3 normal = normalize(eyeNormal);
//	vec3 viewDir = normalize(eyePos);

//	float cosTheta = dot(-viewDir, normal);
//	float fresnel = fresnelDielectric(cosTheta, extIOR, intIOR);

//	// float thickness = abs(normal.z) * 2;

//	// vec3 extinction = vec3(1.0f) - vec3(0.1f, 0.5f, 0.9f);
//	// vec3 transmission = exp(-extinction * thickness);

//	// finalColor = vec4(mix(refractedColor * transmission, reflectedColor, fresnel), 1.0f);

//	vec3 reflection = getReflectedColor(viewDir, normal);
//	vec3 refraction = getRefractedColor(viewDir, normal, normal, extIOR / intIOR);

//	finalColor = vec4(mix(refraction, reflection, fresnel), 1.0f);
//}