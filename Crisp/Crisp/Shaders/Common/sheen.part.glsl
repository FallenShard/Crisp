

float sheenD(float NdotH, float alpha)
{
    const float sinTheta = sqrt(1.0f - NdotH * NdotH);
    const float invTerm = 1.0f / alpha;
    return (2.0f + invTerm) * pow(sinTheta, invTerm) / (2.0f * PI);
}

float sheenL(float x, float alpha)
{
    const float c_0 = -1.59612;
    const float c_1 = 0.20375;
    const float c_2 = -0.55825;
    const float c_3 = 1.32805;
    const float w = c_0 / (1.0f + c_1 * pow(alpha, c_2)) + c_3;

    const float a_0 = (1.0f - w) * 13.7000 + w * 11.9095;
    const float a_1 = (1.0f - w) * 2.92754 + w * 4.68753;
    const float a_2 = (1.0f - w) * 0.28670 + w * 0.33467;
    const float a_3 = (1.0f - w) * -0.81757 + w * -2.22664;
    const float a_4 = (1.0f - w) * -1.76591 + w * -1.22466;
    return a_0 / (1.0f + a_1 * pow(x, a_2)) + a_3 * x + a_4;
}

float sheenLambda(float cosTheta, float alpha)
{
    return cosTheta < 0.5f
        ? exp(sheenL(cosTheta, alpha))
        : exp(2.0f * sheenL(0.5f, alpha) - sheenL(1.0f - cosTheta, alpha));
}

float sheenG(float LdotH, float VdotH, float NdotV, float NdotL, float alpha)
{
    const float step1 = LdotH > 0.0f ? 1.0f : 0.0f;
    const float step2 = VdotH > 0.0f ? 1.0f : 0.0f;
    return step1 * step2 / (1.0f + sheenLambda(NdotV, alpha) + sheenLambda(NdotL, alpha));
}

float sheenScale(vec3 sheenColor, float NdotV, float NdotL, float alpha, in sampler2D E)
{
    const float E_NdotV = texture(E, vec2(NdotV, alpha)).r;
    const float E_NdotL = texture(E, vec2(NdotL, alpha)).r;
    const float sheenMax = max(max(sheenColor.r, sheenColor.g), sheenColor.b);
    return min(1.0f - sheenMax * E_NdotV, 1.0f - sheenMax * E_NdotL);
}