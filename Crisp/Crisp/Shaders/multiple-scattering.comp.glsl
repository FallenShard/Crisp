// Copyright Epic Games, Inc. All Rights Reserved.

//#include "./Resources/RenderSkyCommon.hlsl"
#version 460 core

#define PI 3.1415926535897932384626433832795


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

struct SingleScatteringResult
{
	vec3 L;						// Scattered light (luminance)
	vec3 OpticalDepth;			// Optical depth (1/m)
	vec3 Transmittance;			// Transmittance in [0,1] (unitless)
	vec3 MultiScatAs1;

	vec3 NewMultiScatStep0Out;
	vec3 NewMultiScatStep1Out;
};

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

layout(set = 1, binding = 0, rgba16f) uniform writeonly image2D OutputTexture;
layout(set = 1, binding = 1) uniform sampler2D TransmittanceLutTexture;

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

const float PLANET_RADIUS_OFFSET = 0.01f;

float fromUnitToSubUvs(float u, float resolution) { return (u + 0.5f / resolution) * (resolution / (resolution + 1.0f)); }
float fromSubUvsToUnit(float u, float resolution) { return (u - 0.5f / resolution) * (resolution / (resolution - 1.0f)); }

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

    DepthBufferValue = 0.0f;
	// if (DepthBufferValue >= 0.0f)
	// {
	// 	ClipSpace.z = DepthBufferValue;
	// 	if (ClipSpace.z < 1.0f)
	// 	{
	// 		vec4 DepthBufferWorldPos = mul(gSkyInvViewProjMat, vec4(ClipSpace, 1.0));
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

//#ifdef ILLUMINANCE_IS_ONE
	// When building the scattering factor, we assume light illuminance is 1 to compute a transfert function relative to identity illuminance of 1.
	// This make the scattering factor independent of the light. It is now only linked to the atmosphere properties.
	vec3 globalL = vec3(1.0f);
// #else
// 	vec3 globalL = gSunIlluminance;
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

// #if DEBUGENABLED 
// 		if (debugEnabled)
// 		{
// 			vec3 Pprev = WorldPos + tPrev * WorldDir;
// 			vec3 TxToDebugWorld = vec3(0, 0, -Atmosphere.BottomRadius);
// 			addGpuDebugLine(TxToDebugWorld + Pprev, TxToDebugWorld + P, vec3(0.2, 1, 0.2));
// 			addGpuDebugCross(TxToDebugWorld + P, vec3(0.2, 0.2, 1.0), 0.2);
// 		}
// #endif

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


layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

shared vec3 MultiScatAs1SharedMem[64];
shared vec3 LSharedMem[64];


void main()
{
    uvec3 ThreadId = gl_GlobalInvocationID;
	vec2 pixPos = vec2(ThreadId.xy) + 0.5f;
	vec2 uv = pixPos / MultiScatteringLUTRes;


	uv = vec2(fromSubUvsToUnit(uv.x, MultiScatteringLUTRes), fromSubUvsToUnit(uv.y, MultiScatteringLUTRes));

   

	AtmosphereParameters Atmosphere = GetAtmosphereParameters();

	float cosSunZenithAngle = uv.x * 2.0 - 1.0;
	vec3 sunDir = vec3(0.0, cosSunZenithAngle, -sqrt(clamp(1.0 - cosSunZenithAngle * cosSunZenithAngle, 0, 1)));
	// We adjust again viewHeight according to PLANET_RADIUS_OFFSET to be in a valid range.
	float viewHeight = Atmosphere.BottomRadius + clamp(uv.y + PLANET_RADIUS_OFFSET, 0, 1) * (Atmosphere.TopRadius - Atmosphere.BottomRadius - PLANET_RADIUS_OFFSET);

	vec3 WorldPos = vec3(0.0f, viewHeight, 0.0f);
	vec3 WorldDir = vec3(0.0f, 1.0f, 0.0f);


	const bool ground = true;
	const float SampleCountIni = 20;// a minimum set of step is required for accuracy unfortunately
	const float DepthBufferValue = -1.0;
	const bool VariableSampleCount = false;
	const bool MieRayPhase = false;

	const float SphereSolidAngle = 4.0 * PI;
	const float IsotropicPhase = 1.0 / SphereSolidAngle;


	// Reference. Since there are many sample, it requires MULTI_SCATTERING_POWER_SERIE to be true for accuracy and to avoid divergences (see declaration for explanations)
    const int SQRTSAMPLECOUNT = 8;
	const float sqrtSample = float(SQRTSAMPLECOUNT);
	float i = 0.5f + float(ThreadId.z / SQRTSAMPLECOUNT);
	float j = 0.5f + float(ThreadId.z - float((ThreadId.z / SQRTSAMPLECOUNT)*SQRTSAMPLECOUNT));
	{
		float randA = i / sqrtSample;
		float randB = j / sqrtSample;
		float theta = 2.0f * PI * randA;
		float phi = PI * randB;
		float cosPhi = cos(phi);
		float sinPhi = sin(phi);
		float cosTheta = cos(theta);
		float sinTheta = sin(theta);
		WorldDir.x = cosTheta * sinPhi;
		WorldDir.y = cosPhi;
		WorldDir.z = -sinTheta * sinPhi;
		SingleScatteringResult result = IntegrateScatteredLuminance(
            pixPos,
            WorldPos,
            WorldDir,
            sunDir,
            Atmosphere,
            ground,
            SampleCountIni,
            DepthBufferValue,
            VariableSampleCount,
            MieRayPhase,
            9000000.0f);

		MultiScatAs1SharedMem[ThreadId.z] = result.MultiScatAs1 * SphereSolidAngle / (sqrtSample * sqrtSample);
		LSharedMem[ThreadId.z] = result.L * SphereSolidAngle / (sqrtSample * sqrtSample);
	}

    

    groupMemoryBarrier();
    barrier();

	// 64 to 32
	if (ThreadId.z < 32)
	{
		MultiScatAs1SharedMem[ThreadId.z] += MultiScatAs1SharedMem[ThreadId.z + 32];
		LSharedMem[ThreadId.z] += LSharedMem[ThreadId.z + 32];
	}
	groupMemoryBarrier();
    barrier();

	// 32 to 16
	if (ThreadId.z < 16)
	{
		MultiScatAs1SharedMem[ThreadId.z] += MultiScatAs1SharedMem[ThreadId.z + 16];
		LSharedMem[ThreadId.z] += LSharedMem[ThreadId.z + 16];
	}
	groupMemoryBarrier();
    barrier();

	// 16 to 8 (16 is thread group min hardware size with intel, no sync required from there)
	if (ThreadId.z < 8)
	{
		MultiScatAs1SharedMem[ThreadId.z] += MultiScatAs1SharedMem[ThreadId.z + 8];
		LSharedMem[ThreadId.z] += LSharedMem[ThreadId.z + 8];
	}
	groupMemoryBarrier();
    barrier();
	if (ThreadId.z < 4)
	{
		MultiScatAs1SharedMem[ThreadId.z] += MultiScatAs1SharedMem[ThreadId.z + 4];
		LSharedMem[ThreadId.z] += LSharedMem[ThreadId.z + 4];
	}
	groupMemoryBarrier();
    barrier();
	if (ThreadId.z < 2)
	{
		MultiScatAs1SharedMem[ThreadId.z] += MultiScatAs1SharedMem[ThreadId.z + 2];
		LSharedMem[ThreadId.z] += LSharedMem[ThreadId.z + 2];
	}
	groupMemoryBarrier();
    barrier();
	if (ThreadId.z < 1)
	{
		MultiScatAs1SharedMem[ThreadId.z] += MultiScatAs1SharedMem[ThreadId.z + 1];
		LSharedMem[ThreadId.z] += LSharedMem[ThreadId.z + 1];
	}
	groupMemoryBarrier();
    barrier();
	if (ThreadId.z > 0)
		return;

	vec3 MultiScatAs1			= MultiScatAs1SharedMem[0] * IsotropicPhase;	// Equation 7 f_ms
	vec3 InScatteredLuminance	= LSharedMem[0] * IsotropicPhase;				// Equation 5 L_2ndOrder

	// MultiScatAs1 represents the amount of luminance scattered as if the integral of scattered luminance over the sphere would be 1.
	//  - 1st order of scattering: one can ray-march a straight path as usual over the sphere. That is InScatteredLuminance.
	//  - 2nd order of scattering: the inscattered luminance is InScatteredLuminance at each of samples of fist order integration. Assuming a uniform phase function that is represented by MultiScatAs1,
	//  - 3nd order of scattering: the inscattered luminance is (InScatteredLuminance * MultiScatAs1 * MultiScatAs1)
	//  - etc.
#if	MULTI_SCATTERING_POWER_SERIE==0
	vec3 MultiScatAs1SQR = MultiScatAs1 * MultiScatAs1;
	vec3 L = InScatteredLuminance * (1.0 + MultiScatAs1 + MultiScatAs1SQR + MultiScatAs1 * MultiScatAs1SQR + MultiScatAs1SQR * MultiScatAs1SQR);
#else
	// For a serie, sum_{n=0}^{n=+inf} = 1 + r + r^2 + r^3 + ... + r^n = 1 / (1.0 - r), see https://en.wikipedia.org/wiki/Geometric_series 
	const vec3 r = MultiScatAs1;
	const vec3 SumOfAllMultiScatteringEventsContribution = 1.0f / (1.0 - r);
	vec3 L = InScatteredLuminance * SumOfAllMultiScatteringEventsContribution;// Equation 10 Psi_ms
#endif
    imageStore(OutputTexture, ivec2(ThreadId.xy), vec4(MultipleScatteringFactor * L, 1.0f));
}
