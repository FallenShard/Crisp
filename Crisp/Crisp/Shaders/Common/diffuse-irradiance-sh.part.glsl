#ifndef CRISP_DIFFUSE_IRRADIANCE_SH_GLSL
#define CRISP_DIFFUSE_IRRADIANCE_SH_GLSL

const int diffuseIrradianceShChannelCount = 3;

vec3 loadDiffuseIrradianceShCoefficient(const float coefficients[27], const int index) {
    const int offset = index * diffuseIrradianceShChannelCount;
    return vec3(coefficients[offset], coefficients[offset + 1], coefficients[offset + 2]);
}

// Evaluates Filament cmgen's --sh-shader polynomial. The coefficients already contain the cosine convolution,
// SH reconstruction factors, and the Lambertian 1 / pi term, so the result has the same semantics as cmgen's
// legacy diffuse-irradiance cubemap: reflected diffuse radiance for a unit-albedo surface.
vec3 evaluateDiffuseIrradianceSh(const float coefficients[27], const vec3 direction) {
    const vec3 n = normalize(direction);
    vec3 irradiance = loadDiffuseIrradianceShCoefficient(coefficients, 0);
    irradiance += loadDiffuseIrradianceShCoefficient(coefficients, 1) * n.y;
    irradiance += loadDiffuseIrradianceShCoefficient(coefficients, 2) * n.z;
    irradiance += loadDiffuseIrradianceShCoefficient(coefficients, 3) * n.x;
    irradiance += loadDiffuseIrradianceShCoefficient(coefficients, 4) * n.y * n.x;
    irradiance += loadDiffuseIrradianceShCoefficient(coefficients, 5) * n.y * n.z;
    irradiance += loadDiffuseIrradianceShCoefficient(coefficients, 6) * (3.0f * n.z * n.z - 1.0f);
    irradiance += loadDiffuseIrradianceShCoefficient(coefficients, 7) * n.z * n.x;
    irradiance += loadDiffuseIrradianceShCoefficient(coefficients, 8) * (n.x * n.x - n.y * n.y);
    return max(irradiance, vec3(0.0f));
}

#endif // CRISP_DIFFUSE_IRRADIANCE_SH_GLSL
