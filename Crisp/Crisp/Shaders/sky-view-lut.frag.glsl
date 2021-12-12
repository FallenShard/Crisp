#version 450 core

layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 finalColor;

const float PI = 3.14159265358979323846f;


layout(set = 0, binding = 0) uniform CommonBuffer
{
	mat4 gViewProjMat;

	vec4 gColor;

	vec3 gSunIlluminance;
	int gScatteringMaxPathDepth;

	uvec2 gResolution;
	float gFrameTimeSec;
	float gTimeSec;

    vec2 RayMarchMinMaxSPP;
    vec2 pad;

	uvec2 gMouseLastDownPos;
	uint gFrameId;
	uint gTerrainResolution;
	//float gScreenshotCaptureActive;
};


layout(set = 0, binding = 1) uniform SamplesBuffer
{
	//
	// From AtmosphereParameters
	//

	/*float3*/	vec3 solar_irradiance;
	/*float*/	float sun_angular_radius;

	/*float3*/	vec3 absorption_extinction;
	/*float*/	float mu_s_min;

	/*float3*/	vec3 rayleigh_scattering;
	/*float*/	float mie_phase_function_g;

	/*float3*/	vec3 mie_scattering;
	/*float*/	float bottom_radius;

	/*float3*/	vec3 mie_extinction;
	/*float*/	float top_radius;

	vec3	mie_absorption;
	float	pad00;

	/*float3*/	vec3 ground_albedo;
	float pad0;

	// /*float10*/	DensityProfile rayleigh_density;
	// /*float10*/	DensityProfile mie_density;
	// /*float10*/	DensityProfile absorption_density;
	vec4 rayleigh_density[3];
	vec4 mie_density[3];
	vec4 absorption_density[3];

	//
	// Add generated static header constant
	//

	int TRANSMITTANCE_TEXTURE_WIDTH;
	int TRANSMITTANCE_TEXTURE_HEIGHT;
	int IRRADIANCE_TEXTURE_WIDTH;
	int IRRADIANCE_TEXTURE_HEIGHT;

	int SCATTERING_TEXTURE_R_SIZE;
	int SCATTERING_TEXTURE_MU_SIZE;
	int SCATTERING_TEXTURE_MU_S_SIZE;
	int SCATTERING_TEXTURE_NU_SIZE;

	vec3 SKY_SPECTRAL_RADIANCE_TO_LUMINANCE;
	float  pad3;
	vec3 SUN_SPECTRAL_RADIANCE_TO_LUMINANCE;
	float  pad4;

	//
	// Other globals
	//
	mat4 gSkyViewProjMat;
	mat4 gSkyInvViewProjMat;
	mat4 gShadowmapViewProjMat;

	vec3 camera;
	float  pad5;
	vec3 sun_direction;
	float  pad6;
	vec3 view_ray;
	float  pad7;

	float MultipleScatteringFactor;
	float MultiScatteringLUTRes;
	float pad9;
	float pad10;
};

layout(set = 1, binding = 0) uniform sampler2D TransmittanceLutTexture;
layout(set = 1, binding = 1) uniform sampler2D MultiScatteringTexture;

struct AtmosphereParameters
{
	// Radius of the planet (center to ground)
	float BottomRadius;
	// Maximum considered atmosphere height (center to atmosphere top)
	float TopRadius;

	// Rayleigh scattering exponential distribution scale in the atmosphere
	float RayleighDensityExpScale;
	// Rayleigh scattering coefficients
	vec3 RayleighScattering;

	// Mie scattering exponential distribution scale in the atmosphere
	float MieDensityExpScale;
	// Mie scattering coefficients
	vec3 MieScattering;
	// Mie extinction coefficients
	vec3 MieExtinction;
	// Mie absorption coefficients
	vec3 MieAbsorption;
	// Mie phase function excentricity
	float MiePhaseG;

	// Another medium type in the atmosphere
	float AbsorptionDensity0LayerWidth;
	float AbsorptionDensity0ConstantTerm;
	float AbsorptionDensity0LinearTerm;
	float AbsorptionDensity1ConstantTerm;
	float AbsorptionDensity1LinearTerm;
	// This other medium only absorb light, e.g. useful to represent ozone in the earth atmosphere
	vec3 AbsorptionExtinction;

	// The albedo of the ground.
	vec3 GroundAlbedo;
};

AtmosphereParameters GetAtmosphereParameters()
{
	AtmosphereParameters Parameters;
	Parameters.AbsorptionExtinction = absorption_extinction;

	// Traslation from Bruneton2017 parameterisation.
	Parameters.RayleighDensityExpScale = rayleigh_density[1].w;
	Parameters.MieDensityExpScale = mie_density[1].w;
	Parameters.AbsorptionDensity0LayerWidth = absorption_density[0].x;
	Parameters.AbsorptionDensity0ConstantTerm = absorption_density[1].x;
	Parameters.AbsorptionDensity0LinearTerm = absorption_density[0].w;
	Parameters.AbsorptionDensity1ConstantTerm = absorption_density[2].y;
	Parameters.AbsorptionDensity1LinearTerm = absorption_density[2].x;

	Parameters.MiePhaseG = mie_phase_function_g;
	Parameters.RayleighScattering = rayleigh_scattering;
	Parameters.MieScattering = mie_scattering;
	Parameters.MieAbsorption = mie_absorption;
	Parameters.MieExtinction = mie_extinction;
	Parameters.GroundAlbedo = ground_albedo;
	Parameters.BottomRadius = bottom_radius;
	Parameters.TopRadius = top_radius;
	return Parameters;
}


// float ClampCosine(float mu) {
//   return clamp(mu, float(-1.0), float(1.0));
// }

// float ClampDistance(float d) {
//   return max(d, 0.0 * m);
// }

// float ClampRadius(AtmosphereParameters atmosphere, float r) {
//   return clamp(r, atmosphere.bottom_radius, atmosphere.top_radius);
// }

// float SafeSqrt(float a) {
//   return sqrt(max(a, 0.0 * m2));
// }


// float GetUnitRangeFromTextureCoord(float u, int texture_size) {
//   return (u - 0.5 / float(texture_size)) / (1.0 - 1.0 / float(texture_size));
// }

// void GetRMuFromTransmittanceTextureUv(in AtmosphereParameters atmosphere,
//     vec2 uv, out float r, out float mu) {
//     float x_mu = GetUnitRangeFromTextureCoord(uv.x, TRANSMITTANCE_TEXTURE_WIDTH);
//     float x_r = GetUnitRangeFromTextureCoord(uv.y, TRANSMITTANCE_TEXTURE_HEIGHT);
//     // Distance to top atmosphere boundary for a horizontal ray at ground level.
//     float H = sqrt(atmosphere.top_radius * atmosphere.top_radius -
//         atmosphere.bottom_radius * atmosphere.bottom_radius);
//     // Distance to the horizon, from which we can compute r:
//     float rho = H * x_r;
//     r = sqrt(rho * rho + atmosphere.bottom_radius * atmosphere.bottom_radius);
//     // Distance to the top atmosphere boundary for the ray (r,mu), and its minimum
//     // and maximum values over all mu - obtained for (r,1) and (r,mu_horizon) -
//     // from which we can recover mu:
//     float d_min = atmosphere.top_radius - r;
//     float d_max = rho + H;
//     float d = d_min + x_mu * (d_max - d_min);
//     mu = d == 0.0 * m ? float(1.0) : (H * H - rho * rho - d * d) / (2.0 * r * d);
//     mu = ClampCosine(mu);
// }

// float DistanceToTopAtmosphereBoundary(AtmosphereParameters atmosphere,
//     float r, float mu) {
//   float discriminant = r * r * (mu * mu - 1.0) +
//       atmosphere.top_radius * atmosphere.top_radius;
//   return ClampDistance(-r * mu + SafeSqrt(discriminant));
// }

// float GetLayerDensity(DensityProfileLayer layer, float altitude) {
//   float density = layer.exp_term * exp(layer.exp_scale * altitude) +
//       layer.linear_term * altitude + layer.constant_term;
//   return clamp(density, float(0.0), float(1.0));
// }

// float GetProfileDensity(DensityProfile profile, float altitude) {
//   return altitude < profile.layers[0].width ?
//       GetLayerDensity(profile.layers[0], altitude) :
//       GetLayerDensity(profile.layers[1], altitude);
// }

// float ComputeOpticalfloatToTopAtmosphereBoundary(
//     AtmosphereParameters atmosphere, DensityProfile profile,
//     float r, float mu) {
//   // float of intervals for the numerical integration.
//   const int SAMPLE_COUNT = 500;
//   // The integration step, i.e. the length of each integration interval.
//   float dx =
//       DistanceToTopAtmosphereBoundary(atmosphere, r, mu) / float(SAMPLE_COUNT);
//   // Integration loop.
//   float result = 0.0 * m;
//   for (int i = 0; i <= SAMPLE_COUNT; ++i) {
//     float d_i = float(i) * dx;
//     // Distance between the current sample point and the planet center.
//     float r_i = sqrt(d_i * d_i + 2.0 * r * mu * d_i + r * r);
//     // float density at the current sample point (divided by the number density
//     // at the bottom of the atmosphere, yielding a dimensionless number).
//     float y_i = GetProfileDensity(profile, r_i - atmosphere.bottom_radius);
//     // Sample weight (from the trapezoidal rule).
//     float weight_i = i == 0 || i == SAMPLE_COUNT ? 0.5 : 1.0;
//     result += y_i * weight_i * dx;
//   }
//   return result;
// }

// vec3 ComputeTransmittanceToTopAtmosphereBoundary(
//     in AtmosphereParameters atmosphere, float r, float mu) {
//   return exp(-(
//       atmosphere.rayleigh_scattering *
//           ComputeOpticalfloatToTopAtmosphereBoundary(
//               atmosphere, atmosphere.rayleigh_density, r, mu) +
//       atmosphere.mie_extinction *
//           ComputeOpticalfloatToTopAtmosphereBoundary(
//               atmosphere, atmosphere.mie_density, r, mu) +
//       atmosphere.absorption_extinction *
//           ComputeOpticalfloatToTopAtmosphereBoundary(
//               atmosphere, atmosphere.absorption_density, r, mu)));
// }

// vec3 ComputeTransmittanceToTopAtmosphereBoundaryTexture(in AtmosphereParameters atmosphere,
//     in vec2 tc)
// {
//     const vec2 TRANSMITTANCE_TEXTURE_SIZE = vec2(TRANSMITTANCE_TEXTURE_WIDTH, TRANSMITTANCE_TEXTURE_HEIGHT);
//     float r;
//     float mu;
//     GetRMuFromTransmittanceTextureUv(
//         atmosphere, tc, r, mu);
//     return ComputeTransmittanceToTopAtmosphereBoundary(atmosphere, r, mu);
// }

float fromUnitToSubUvs(float u, float resolution) { return (u + 0.5f / resolution) * (resolution / (resolution + 1.0f)); }
float fromSubUvsToUnit(float u, float resolution) { return (u - 0.5f / resolution) * (resolution / (resolution - 1.0f)); }

#define NONLINEARSKYVIEWLUT 1
void UvToSkyViewLutParams(in AtmosphereParameters Atmosphere, out float viewZenithCosAngle, out float lightViewCosAngle, in float viewHeight, in vec2 uv)
{
	// Constrain uvs to valid sub texel range (avoid zenith derivative issue making LUT usage visible)
	uv = vec2(fromSubUvsToUnit(uv.x, 192.0f), fromSubUvsToUnit(uv.y, 108.0f));

	float Vhorizon = sqrt(viewHeight * viewHeight - Atmosphere.BottomRadius * Atmosphere.BottomRadius);
	float CosBeta = Vhorizon / viewHeight;				// GroundToHorizonCos
	float Beta = acos(CosBeta);
	float ZenithHorizonAngle = PI - Beta;

	if (uv.y < 0.5f)
	{
		float coord = 2.0*uv.y;
		coord = 1.0 - coord;
// #if NONLINEARSKYVIEWLUT
		coord *= coord;
// #endif
		coord = 1.0 - coord;
		viewZenithCosAngle = cos(ZenithHorizonAngle * coord);
	}
	else
	{
		float coord = uv.y*2.0 - 1.0;
// #if NONLINEARSKYVIEWLUT
		coord *= coord;
// #endif
		viewZenithCosAngle = cos(ZenithHorizonAngle + Beta * coord);
	}

	float coord = uv.x;
	coord *= coord;
	lightViewCosAngle = -(coord*2.0 - 1.0);
}

float raySphereIntersectNearest(vec3 r0, vec3 rd, vec3 s0, float sR)
{
	float a = dot(rd, rd);
	vec3 s0_r0 = r0 - s0;
	float b = 2.0 * dot(rd, s0_r0);
	float c = dot(s0_r0, s0_r0) - (sR * sR);
	float delta = b * b - 4.0*a*c;
	if (delta < 0.0 || a == 0.0)
	{
		return -1.0;
	}
	float sol0 = (-b - sqrt(delta)) / (2.0*a);
	float sol1 = (-b + sqrt(delta)) / (2.0*a);
	if (sol0 < 0.0 && sol1 < 0.0)
	{
		return -1.0;
	}
	if (sol0 < 0.0)
	{
		return max(0.0, sol1);
	}
	else if (sol1 < 0.0)
	{
		return max(0.0, sol0);
	}
	return max(0.0, min(sol0, sol1));
}

const float PLANET_RADIUS_OFFSET = 0.01f;

bool MoveToTopAtmosphere(inout vec3 WorldPos, in vec3 WorldDir, in float AtmosphereTopRadius)
{
	float viewHeight = length(WorldPos);
	if (viewHeight > AtmosphereTopRadius)
	{
		float tTop = raySphereIntersectNearest(WorldPos, WorldDir, vec3(0.0f, 0.0f, 0.0f), AtmosphereTopRadius);
		if (tTop >= 0.0f)
		{
			vec3 UpVector = WorldPos / viewHeight;
			vec3 UpOffset = UpVector * -PLANET_RADIUS_OFFSET;
			WorldPos = WorldPos + WorldDir * tTop + UpOffset;
		}
		else
		{
			// Ray is not intersecting the atmosphere
			return false;
		}
	}
	return true; // ok to start tracing
}

struct SingleScatteringResult
{
	vec3 L;						// Scattered light (luminance)
	vec3 OpticalDepth;			// Optical depth (1/m)
	vec3 Transmittance;			// Transmittance in [0,1] (unitless)
	vec3 MultiScatAs1;

	vec3 NewMultiScatStep0Out;
	vec3 NewMultiScatStep1Out;

	vec3 Debug;
};

float CornetteShanksMiePhaseFunction(float g, float cosTheta)
{
	float k = 3.0 / (8.0 * PI) * (1.0 - g * g) / (2.0 + g * g);
	return k * (1.0 + cosTheta * cosTheta) / pow(1.0 + g * g - 2.0 * g * -cosTheta, 1.5);
}

float hgPhase(float g, float cosTheta)
{
//#ifdef USE_CornetteShanks
	return CornetteShanksMiePhaseFunction(g, cosTheta);
// #else
// 	// Reference implementation (i.e. not schlick approximation). 
// 	// See http://www.pbr-book.org/3ed-2018/Volume_Scattering/Phase_Functions.html
// 	float numer = 1.0f - g * g;
// 	float denom = 1.0f + g * g + 2.0f * g * cosTheta;
// 	return numer / (4.0f * PI * denom * sqrt(denom));
// #endif
}

float RayleighPhase(float cosTheta)
{
	float factor = 3.0f / (16.0f * PI);
	return factor * (1.0f + cosTheta * cosTheta);
}

float getAlbedo(float scattering, float extinction)
{
	return scattering / max(0.001, extinction);
}
vec3 getAlbedo3(vec3 scattering, vec3 extinction)
{
	return scattering / max(vec3(0.001), extinction);
}

struct MediumSampleRGB
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

MediumSampleRGB sampleMediumRGB(in vec3 WorldPos, in AtmosphereParameters Atmosphere)
{
	const float viewHeight = length(WorldPos) - Atmosphere.BottomRadius;

	const float densityMie = exp(Atmosphere.MieDensityExpScale * viewHeight);
	const float densityRay = exp(Atmosphere.RayleighDensityExpScale * viewHeight);
	const float densityOzo = clamp(viewHeight < Atmosphere.AbsorptionDensity0LayerWidth ?
		Atmosphere.AbsorptionDensity0LinearTerm * viewHeight + Atmosphere.AbsorptionDensity0ConstantTerm :
		Atmosphere.AbsorptionDensity1LinearTerm * viewHeight + Atmosphere.AbsorptionDensity1ConstantTerm, 0, 1);

	MediumSampleRGB s;

	s.scatteringMie = densityMie * Atmosphere.MieScattering;
	s.absorptionMie = densityMie * Atmosphere.MieAbsorption;
	s.extinctionMie = densityMie * Atmosphere.MieExtinction;

	s.scatteringRay = densityRay * Atmosphere.RayleighScattering;
	s.absorptionRay = vec3(0.0f);
	s.extinctionRay = s.scatteringRay + s.absorptionRay;

	s.scatteringOzo = vec3(0.0f);
	s.absorptionOzo = densityOzo * Atmosphere.AbsorptionExtinction;
	s.extinctionOzo = s.scatteringOzo + s.absorptionOzo;

	s.scattering = s.scatteringMie + s.scatteringRay + s.scatteringOzo;
	s.absorption = s.absorptionMie + s.absorptionRay + s.absorptionOzo;
	s.extinction = s.extinctionMie + s.extinctionRay + s.extinctionOzo;
	s.albedo = getAlbedo3(s.scattering, s.extinction);

	return s;
}

void LutTransmittanceParamsToUv(in AtmosphereParameters Atmosphere, in float viewHeight, in float viewZenithCosAngle, out vec2 uv)
{
	float H = sqrt(max(0.0f, Atmosphere.TopRadius * Atmosphere.TopRadius - Atmosphere.BottomRadius * Atmosphere.BottomRadius));
	float rho = sqrt(max(0.0f, viewHeight * viewHeight - Atmosphere.BottomRadius * Atmosphere.BottomRadius));

	float discriminant = viewHeight * viewHeight * (viewZenithCosAngle * viewZenithCosAngle - 1.0) + Atmosphere.TopRadius * Atmosphere.TopRadius;
	float d = max(0.0, (-viewHeight * viewZenithCosAngle + sqrt(discriminant))); // Distance to atmosphere boundary

	float d_min = Atmosphere.TopRadius - viewHeight;
	float d_max = rho + H;
	float x_mu = (d - d_min) / (d_max - d_min);
	float x_r = rho / H;

	uv = vec2(x_mu, x_r);
	//uv = float2(fromUnitToSubUvs(uv.x, TRANSMITTANCE_TEXTURE_WIDTH), fromUnitToSubUvs(uv.y, TRANSMITTANCE_TEXTURE_HEIGHT)); // No real impact so off
}

float getShadow(in AtmosphereParameters Atmosphere, vec3 P)
{
	// // First evaluate opaque shadow
	// float4 shadowUv = mul(gShadowmapViewProjMat, float4(P + float3(0.0, 0.0, -Atmosphere.BottomRadius), 1.0));
	// //shadowUv /= shadowUv.w;	// not be needed as it is an ortho projection
	// shadowUv.x = shadowUv.x*0.5 + 0.5;
	// shadowUv.y = -shadowUv.y*0.5 + 0.5;
	// if (all(shadowUv.xyz >= 0.0) && all(shadowUv.xyz < 1.0))
	// {
	// 	return ShadowmapTexture.SampleCmpLevelZero(samplerShadow, shadowUv.xy, shadowUv.z);
	// }
	// return 1.0f;
    return 1.0f;
}

vec3 GetMultipleScattering(AtmosphereParameters Atmosphere, vec3 scattering, vec3 extinction, vec3 worlPos, float viewZenithCosAngle)
{
	vec2 uv = clamp(vec2(viewZenithCosAngle*0.5f + 0.5f, (length(worlPos) - Atmosphere.BottomRadius) / (Atmosphere.TopRadius - Atmosphere.BottomRadius)), vec2(0), vec2(1));
	uv = vec2(fromUnitToSubUvs(uv.x, MultiScatteringLUTRes), fromUnitToSubUvs(uv.y, MultiScatteringLUTRes));

	vec3 multiScatteredLuminance = textureLod(MultiScatteringTexture, uv, 0).rgb;
	return multiScatteredLuminance;
}

SingleScatteringResult IntegrateScatteredLuminance(
	in vec2 pixPos, in vec3 WorldPos, in vec3 WorldDir, in vec3 SunDir, in AtmosphereParameters Atmosphere,
	in bool ground, in float SampleCountIni, in float DepthBufferValue, in bool VariableSampleCount,
	in bool MieRayPhase, float tMaxMax)
{
	const bool debugEnabled = false;//all(uvec2(pixPos.xx) == gMouseLastDownPos.xx) && uint(pixPos.y) % 10 == 0 && DepthBufferValue != -1.0f;
	SingleScatteringResult result;
    result.L = vec3(0.0f);
    result.OpticalDepth = vec3(0.0f);			// Optical depth (1/m)
	result.Transmittance = vec3(0.0f);			// Transmittance in [0,1] (unitless)
	result.MultiScatAs1 = vec3(0.0f);
	result.NewMultiScatStep0Out = vec3(0.0f);
	result.NewMultiScatStep1Out = vec3(0.0f);

	vec3 ClipSpace = vec3((pixPos / vec2(gResolution))*vec2(2.0, -2.0) - vec2(1.0, -1.0), 1.0);

	// Compute next intersection with atmosphere or ground 
	vec3 earthO = vec3(0.0f, 0.0f, 0.0f);
	float tBottom = raySphereIntersectNearest(WorldPos, WorldDir, earthO, Atmosphere.BottomRadius);
	float tTop = raySphereIntersectNearest(WorldPos, WorldDir, earthO, Atmosphere.TopRadius);
	float tMax = 0.0f;
	if (tBottom < 0.0f)
	{
		if (tTop < 0.0f)
		{
			tMax = 0.0f; // No intersection with earth nor atmosphere: stop right away  
			return result;
		}
		else
		{
			tMax = tTop;
		}
	}
	else
	{
		if (tTop > 0.0f)
		{
			tMax = min(tTop, tBottom);
		}
	}

	// if (DepthBufferValue >= 0.0f)
	// {
	// 	ClipSpace.z = DepthBufferValue;
	// 	if (ClipSpace.z < 1.0f)
	// 	{
	// 		vec4 DepthBufferWorldPos = gSkyInvViewProjMat * vec4(ClipSpace, 1.0);
	// 		DepthBufferWorldPos /= DepthBufferWorldPos.w;

	// 		float tDepth = length(DepthBufferWorldPos.xyz - (WorldPos + vec3(0.0, 0.0, -Atmosphere.BottomRadius))); // apply earth offset to go back to origin as top of earth mode. 
	// 		if (tDepth < tMax)
	// 		{
	// 			tMax = tDepth;
	// 		}
	// 	}
	// 	//		if (VariableSampleCount && ClipSpace.z == 1.0f)
	// 	//			return result;
	// }
	tMax = min(tMax, tMaxMax);

	// Sample count 
	float SampleCount = SampleCountIni;
	float SampleCountFloor = SampleCountIni;
	float tMaxFloor = tMax;
	if (VariableSampleCount)
	{
		SampleCount = mix(RayMarchMinMaxSPP.x, RayMarchMinMaxSPP.y, clamp(tMax*0.01, 0, 1));
		SampleCountFloor = floor(SampleCount);
		tMaxFloor = tMax * SampleCountFloor / SampleCount;	// rescale tMax to map to the last entire step segment.
	}
	float dt = tMax / SampleCount;
	

	// Phase functions
	const float uniformPhase = 1.0 / (4.0 * PI);
	const vec3 wi = SunDir;
	const vec3 wo = WorldDir;
	float cosTheta = dot(wi, wo);
	float MiePhaseValue = hgPhase(Atmosphere.MiePhaseG, -cosTheta);	// mnegate cosTheta because due to WorldDir being a "in" direction. 
	float RayleighPhaseValue = RayleighPhase(cosTheta);

	result.Debug = vec3(cosTheta);

//#ifdef ILLUMINANCE_IS_ONE
	// When building the scattering factor, we assume light illuminance is 1 to compute a transfert function relative to identity illuminance of 1.
	// This make the scattering factor independent of the light. It is now only linked to the atmosphere properties.
	vec3 globalL = vec3(1.0f);
// #else
    //vec3 globalL = gSunIlluminance;
// #endif

	// Ray march the atmosphere to integrate optical depth
	vec3 L = vec3(0.0f);
	vec3 throughput = vec3(1.0);
	vec3 OpticalDepth = vec3(0.0);
	float t = 0.0f;
	float tPrev = 0.0;
	const float SampleSegmentT = 0.3f;
	for (float s = 0.0f; s < SampleCount; s += 1.0f)
	{
		if (VariableSampleCount)
		{
			// More expenssive but artefact free
			float t0 = (s) / SampleCountFloor;
			float t1 = (s + 1.0f) / SampleCountFloor;
			// Non linear distribution of sample within the range.
			t0 = t0 * t0;
			t1 = t1 * t1;
			// Make t0 and t1 world space distances.
			t0 = tMaxFloor * t0;
			if (t1 > 1.0)
			{
				t1 = tMax;
				//	t1 = tMaxFloor;	// this reveal depth slices
			}
			else
			{
				t1 = tMaxFloor * t1;
			}
			//t = t0 + (t1 - t0) * (whangHashNoise(pixPos.x, pixPos.y, gFrameId * 1920 * 1080)); // With dithering required to hide some sampling artefact relying on TAA later? This may even allow volumetric shadow?
			t = t0 + (t1 - t0)*SampleSegmentT;
			dt = t1 - t0;
		}
		else
		{
			//t = tMax * (s + SampleSegmentT) / SampleCount;
			// Exact difference, important for accuracy of multiple scattering
			float NewT = tMax * (s + SampleSegmentT) / SampleCount;
			dt = NewT - t;
			t = NewT;
		}
		vec3 P = WorldPos + t * WorldDir;

		MediumSampleRGB medium = sampleMediumRGB(P, Atmosphere);
		const vec3 SampleOpticalDepth = medium.extinction * dt;
		const vec3 SampleTransmittance = exp(-SampleOpticalDepth);
		OpticalDepth += SampleOpticalDepth;

		float pHeight = length(P);
		const vec3 UpVector = P / pHeight;
		float SunZenithCosAngle = dot(SunDir, UpVector);
		vec2 uv;
		LutTransmittanceParamsToUv(Atmosphere, pHeight, SunZenithCosAngle, uv);
		vec3 TransmittanceToSun = textureLod(TransmittanceLutTexture, uv, 0).rgb;

		vec3 PhaseTimesScattering;
		if (MieRayPhase)
		{
			PhaseTimesScattering = medium.scatteringMie * MiePhaseValue + medium.scatteringRay * RayleighPhaseValue;
		}
		else
		{
			PhaseTimesScattering = medium.scattering * uniformPhase;
		}

		// Earth shadow 
		float tEarth = raySphereIntersectNearest(P, SunDir, earthO + PLANET_RADIUS_OFFSET * UpVector, Atmosphere.BottomRadius);
		float earthShadow = tEarth >= 0.0f ? 0.0f : 1.0f;

		// Dual scattering for multi scattering 

		vec3 multiScatteredLuminance = vec3(0.0f);
//#if MULTISCATAPPROX_ENABLED
		multiScatteredLuminance = GetMultipleScattering(Atmosphere, medium.scattering, medium.extinction, P, SunZenithCosAngle);
//#endif

		float shadow = 1.0f;
//#if SHADOWMAP_ENABLED
		// First evaluate opaque shadow
		shadow = getShadow(Atmosphere, P);
//#endif

		vec3 S = globalL * (earthShadow * shadow * TransmittanceToSun * PhaseTimesScattering + multiScatteredLuminance * medium.scattering);

		// When using the power serie to accumulate all sattering order, serie r must be <1 for a serie to converge.
		// Under extreme coefficient, MultiScatAs1 can grow larger and thus result in broken visuals.
		// The way to fix that is to use a proper analytical integration as proposed in slide 28 of http://www.frostbite.com/2015/08/physically-based-unified-volumetric-rendering-in-frostbite/
		// However, it is possible to disable as it can also work using simple power serie sum unroll up to 5th order. The rest of the orders has a really low contribution.
#define MULTI_SCATTERING_POWER_SERIE 1

#if MULTI_SCATTERING_POWER_SERIE==0
		// 1 is the integration of luminance over the 4pi of a sphere, and assuming an isotropic phase function of 1.0/(4*PI)
		result.MultiScatAs1 += throughput * medium.scattering * 1 * dt;
#else
		vec3 MS = medium.scattering * 1;
		vec3 MSint = (MS - MS * SampleTransmittance) / medium.extinction;
		result.MultiScatAs1 += throughput * MSint;
#endif

		// Evaluate input to multi scattering 
		{
			vec3 newMS;

			newMS = earthShadow * TransmittanceToSun * medium.scattering * uniformPhase * 1;
			result.NewMultiScatStep0Out += throughput * (newMS - newMS * SampleTransmittance) / medium.extinction;
			//	result.NewMultiScatStep0Out += SampleTransmittance * throughput * newMS * dt;

			newMS = medium.scattering * uniformPhase * multiScatteredLuminance;
			result.NewMultiScatStep1Out += throughput * (newMS - newMS * SampleTransmittance) / medium.extinction;
			//	result.NewMultiScatStep1Out += SampleTransmittance * throughput * newMS * dt;
		}

#if 0
		L += throughput * S * dt;
		throughput *= SampleTransmittance;
#else
		// See slide 28 at http://www.frostbite.com/2015/08/physically-based-unified-volumetric-rendering-in-frostbite/ 
		vec3 Sint = (S - S * SampleTransmittance) / medium.extinction;	// integrate along the current step segment 
		L += throughput * Sint;														// accumulate and also take into account the transmittance from previous steps
		throughput *= SampleTransmittance;
#endif

		tPrev = t;
	}

	if (ground && tMax == tBottom && tBottom > 0.0)
	{
		// Account for bounced light off the earth
		vec3 P = WorldPos + tBottom * WorldDir;
		float pHeight = length(P);

		const vec3 UpVector = P / pHeight;
		float SunZenithCosAngle = dot(SunDir, UpVector);
		vec2 uv;
		LutTransmittanceParamsToUv(Atmosphere, pHeight, SunZenithCosAngle, uv);
		vec3 TransmittanceToSun = textureLod(TransmittanceLutTexture, uv, 0).rgb;

		const float NdotL = clamp(dot(normalize(UpVector), normalize(SunDir)), 0, 1);
		L += globalL * TransmittanceToSun * throughput * NdotL * Atmosphere.GroundAlbedo / PI;
	}

	result.L = L;
	result.OpticalDepth = OpticalDepth;
	result.Transmittance = throughput;
	return result;
}

void main()
{
    vec2 pixPos = vec2(gl_FragCoord.xy);
    AtmosphereParameters Atmosphere = GetAtmosphereParameters();

    vec3 ClipSpace = vec3((pixPos / vec2(192.0,108.0))*vec2(2.0, -2.0) - vec2(1.0, -1.0), 1.0);
    vec4 HPos = gSkyInvViewProjMat * vec4(ClipSpace, 1.0);

	

    vec3 WorldDir = normalize(HPos.xyz / HPos.w - camera);
    vec3 WorldPos = camera + vec3(0, Atmosphere.BottomRadius, 0);


    vec2 uv = pixPos / vec2(192.0, 108.0);

    float viewHeight = length(WorldPos);

    float viewZenithCosAngle;
    float lightViewCosAngle;
    UvToSkyViewLutParams(Atmosphere, viewZenithCosAngle, lightViewCosAngle, viewHeight, uv);

    vec3 SunDir;
    {
      vec3 UpVector = WorldPos / viewHeight;
      float sunZenithCosAngle = dot(UpVector, sun_direction);
	  float sunZenithSinAngle = sqrt(1.0 - sunZenithCosAngle * sunZenithCosAngle);
      SunDir = normalize(vec3(sunZenithSinAngle, sunZenithCosAngle, 0.0));
    }

    WorldPos = vec3(0.0f, viewHeight, 0.0f);

    float viewZenithSinAngle = sqrt(max(0, 1 - viewZenithCosAngle * viewZenithCosAngle));
    WorldDir = vec3(
      viewZenithSinAngle * lightViewCosAngle,
	  viewZenithCosAngle,
      -viewZenithSinAngle * sqrt(max(0, 1.0 - lightViewCosAngle * lightViewCosAngle)));

	

    // Move to top atmospehre
    if (!MoveToTopAtmosphere(WorldPos, WorldDir, Atmosphere.TopRadius))
    {
      // Ray is not intersecting the atmosphere
      finalColor = vec4(0, 0, 0, 1);
	  return;
    }

	const bool ground = false;
	const float SampleCountIni = 30;
	const float DepthBufferValue = -1.0;
	const bool VariableSampleCount = true;
	const bool MieRayPhase = true;
	SingleScatteringResult ss = IntegrateScatteredLuminance(pixPos, WorldPos, WorldDir, SunDir, Atmosphere, ground, SampleCountIni, DepthBufferValue, VariableSampleCount, MieRayPhase, 9000000.0f);
	vec3 L = ss.L;

	finalColor = vec4(ss.L, 1);
}