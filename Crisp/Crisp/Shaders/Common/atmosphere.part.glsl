

struct AtmosphereParams
{
    mat4 VP;
    mat4 invVP;

    // Rayleigh scattering coefficients + exponential distribution scale in the atmosphere
    vec3 rayleighScattering;
    float rayleighDensityExpScale;

    // Mie scattering coefficients + exponential distribution scale in the atmosphere
    vec3 mieScattering;
    float mieDensityExpScale;

    // Mie extinction coefficients + phase function excentricity
    vec3 mieExtinction;
    float miePhaseG;

    // Mie absorption coefficients
    vec4 mieAbsorption;

    // Ozone absorption and layer width
    vec3 ozoneAbsorption;

    // Another medium type in the atmosphere
    float absorptionDensity0LayerWidth;
    float absorptionDensity0ConstantTerm;
    float absorptionDensity0LinearTerm;
    float absorptionDensity1ConstantTerm;
    float absorptionDensity1LinearTerm;

    // The albedo of the ground.
    vec3 groundAlbedo;
    // Radius of the planet (center to ground) in km.
    float bottomRadius;

    // Direction from where the sun is facing
    vec3 sunDirection;
    // Maximum considered atmosphere height (center to atmosphere top) in km.
    float topRadius;

    vec4 sunIlluminance;

    vec3 cameraPosition;
    int multiScatteringLutResolution;

    vec2 screenResolution;
    int transmittanceLutWidth;
    int transmittanceLutHeight;

    int minRayMarchingSamples;
    int maxRayMarchingSamples;
};

struct MediumSample
{
    vec3 scattering;
    vec3 absorption;
    vec3 extinction;

    vec3 scatteringMie;
    vec3 absorptionMie;
    vec3 extinctionMie;

    vec3 scatteringRay;
    vec3 absorptionRay;
    vec3 extinctionRay;

    vec3 scatteringOzo;
    vec3 absorptionOzo;
    vec3 extinctionOzo;

    vec3 albedo;
};

const float PlanetRadiusOffset = 0.01f;

vec3 getAlbedo3(vec3 scattering, vec3 extinction)
{
    return scattering / max(vec3(0.001), extinction);
}

MediumSample sampleMedium(const vec3 worldPos, const AtmosphereParams atmosphere)
{
    const float viewHeight = length(worldPos) - atmosphere.bottomRadius;

    const float densityMie = exp(atmosphere.mieDensityExpScale * viewHeight);
    const float densityRay = exp(atmosphere.rayleighDensityExpScale * viewHeight);
    const float densityOzo = clamp(viewHeight < atmosphere.absorptionDensity0LayerWidth ?
        atmosphere.absorptionDensity0LinearTerm * viewHeight + atmosphere.absorptionDensity0ConstantTerm :
        atmosphere.absorptionDensity1LinearTerm * viewHeight + atmosphere.absorptionDensity1ConstantTerm, 0, 1);

    MediumSample s;

    s.scatteringMie = densityMie * atmosphere.mieScattering;
    s.absorptionMie = densityMie * atmosphere.mieAbsorption.rgb;
    s.extinctionMie = densityMie * atmosphere.mieExtinction;

    s.scatteringRay = densityRay * atmosphere.rayleighScattering;
    s.absorptionRay = vec3(0.0f);
    s.extinctionRay = s.scatteringRay + s.absorptionRay;

    s.scatteringOzo = vec3(0.0f);
    s.absorptionOzo = densityOzo * atmosphere.ozoneAbsorption;
    s.extinctionOzo = s.scatteringOzo + s.absorptionOzo;

    s.scattering = s.scatteringMie + s.scatteringRay + s.scatteringOzo;
    s.absorption = s.absorptionMie + s.absorptionRay + s.absorptionOzo;
    s.extinction = s.extinctionMie + s.extinctionRay + s.extinctionOzo;
    s.albedo = getAlbedo3(s.scattering, s.extinction);

    return s;
}

float raySphereIntersectNearest(const vec3 rayOrigin, const vec3 rayDir, const vec3 sphereCenter, const float sphereRadius)
{
    const float a = dot(rayDir, rayDir);
    const vec3 centerToOrigin = rayOrigin - sphereCenter;
    const float b = 2.0f * dot(rayDir, centerToOrigin);
    const float c = dot(centerToOrigin, centerToOrigin) - sphereRadius * sphereRadius;
    const float delta = b * b - 4.0f * a * c;
    if (delta < 0.0f || a == 0.0f)
        return -1.0f;

    float t1 = (-b - sqrt(delta)) / (2.0f * a);
    float t2 = (-b + sqrt(delta)) / (2.0f * a);
    if (t1 < 0.0f && t2 < 0.0f)
        return -1.0f;

    if (t1 < 0.0f)
        return max(0.0f, t2);
    else if (t2 < 0.0f)
        return max(0.0f, t1);

    return max(0.0f, min(t1, t2));
}

float intersectAtmosphere(const vec3 worldPos, const vec3 worldDir, const float bottomRadius, const float topRadius)
{
    const vec3 earthOrigin = vec3(0.0f);
    const float tBottom = raySphereIntersectNearest(worldPos, worldDir, earthOrigin, bottomRadius);
    const float tTop = raySphereIntersectNearest(worldPos, worldDir, earthOrigin, topRadius);
    if (tBottom < 0.0f)
    {
        // No intersection occurs with either of the layers; we are outside of the atmosphere, looking towards space.
        if (tTop < 0.0f)
            return -1.0f;

        // Intersection occurs with just the outer layer; we are in the atmosphere, looking above the horizon.
        return tTop;
    }

    if (tTop < 0.0f)
        return tBottom;

    // We intersect both layers, pick the closer intersection; we look towards the ground.
    return min(tTop, tBottom);
}

bool moveToTopAtmosphere(inout vec3 worldPos, const in vec3 worldDir, const in float atmosphereTopRadius)
{
    const float viewHeight = length(worldPos);

    // Check if we are above the atmosphere (in the space).
    if (viewHeight > atmosphereTopRadius)
    {
        const float tTop = raySphereIntersectNearest(worldPos, worldDir, vec3(0.0f, 0.0f, 0.0f), atmosphereTopRadius);
        if (tTop < 0.0f)
        {
            // Ray is not intersecting the atmosphere.
            return false;
        }

        const vec3 upVector = worldPos / viewHeight;
        worldPos = worldPos + tTop * worldDir - PlanetRadiusOffset * upVector;
    }

    return true; // ok to start tracing.
}


void uvToTransmittanceLutParams(const float bottomRadius, const float topRadius, in vec2 uv, out float viewHeight, out float viewZenithCosAngle)
{
    const float H = sqrt(topRadius * topRadius - bottomRadius * bottomRadius);
    const float rho = H * uv.y;
    viewHeight = sqrt(rho * rho + bottomRadius * bottomRadius);

    const float dMin = topRadius - viewHeight;
    const float dMax = rho + H;
    const float d = dMin + uv.x * (dMax - dMin);
    viewZenithCosAngle = d == 0.0 ? 1.0f : (H * H - rho * rho - d * d) / (2.0 * viewHeight * d);
    viewZenithCosAngle = clamp(viewZenithCosAngle, -1.0, 1.0);
}

// Cornette-Shanks phase function.
float computeMiePhaseFunction(float g, float cosTheta)
{
    const float k = 3.0f / (8.0f * PI) * (1.0f - g * g) / (2.0f + g * g);
    return k * (1.0f + cosTheta * cosTheta) / pow(1.0f + g * g - 2.0f * g * -cosTheta, 1.5f);
}

float computeRayleighPhaseFunction(float cosTheta)
{
    return 3.0f / (16.0f * PI) * (1.0f + cosTheta * cosTheta);
}