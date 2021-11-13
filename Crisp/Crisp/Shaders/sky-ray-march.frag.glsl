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
layout(set = 1, binding = 2) uniform sampler2D SkyViewLutTexture;
layout(set = 1, binding = 3) uniform sampler2DArray AtmosphereCameraScatteringVolume;
layout(set = 1, binding = 4) uniform sampler2D ViewDepthTexture;

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
#if NONLINEARSKYVIEWLUT
		coord *= coord;
#endif
		coord = 1.0 - coord;
		viewZenithCosAngle = cos(ZenithHorizonAngle * coord);
	}
	else
	{
		float coord = uv.y*2.0 - 1.0;
#if NONLINEARSKYVIEWLUT
		coord *= coord;
#endif
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
    tMaxMax = 9000000.0f;
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

	if (DepthBufferValue >= 0.0f)
	{
		ClipSpace.z = DepthBufferValue;
		if (ClipSpace.z < 1.0f)
		{
			vec4 DepthBufferWorldPos = gSkyInvViewProjMat * vec4(ClipSpace, 1.0);
			DepthBufferWorldPos /= DepthBufferWorldPos.w;

			float tDepth = length(DepthBufferWorldPos.xyz - (WorldPos + vec3(0.0, 0.0, -Atmosphere.BottomRadius))); // apply earth offset to go back to origin as top of earth mode. 
			if (tDepth < tMax)
			{
				tMax = tDepth;
			}
		}
		//		if (VariableSampleCount && ClipSpace.z == 1.0f)
		//			return result;
	}
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
	//vec3 globalL = vec3(1.0f);
// #else
    vec3 globalL = gSunIlluminance;
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

#define AP_SLICE_COUNT 32.0f
#define AP_KM_PER_SLICE 4.0f

float AerialPerspectiveDepthToSlice(float depth)
{
	return depth * (1.0f / AP_KM_PER_SLICE);
}
float AerialPerspectiveSliceToDepth(float slice)
{
	return slice * AP_KM_PER_SLICE;
}

// struct RayMarchPixelOutputStruct
// {
// 	float4 Luminance		: SV_TARGET0;
// #if COLORED_TRANSMITTANCE_ENABLED
// 	float4 Transmittance	: SV_TARGET1;
// #endif
// };

vec3 GetSunLuminance(vec3 WorldPos, vec3 WorldDir, float PlanetRadius)
{
// #if RENDER_SUN_DISK
	if (dot(WorldDir, sun_direction) > cos(0.5*0.505*3.14159 / 180.0))
	{
		float t = raySphereIntersectNearest(WorldPos, WorldDir, vec3(0.0f, 0.0f, 0.0f), PlanetRadius);
		if (t < 0.0f) // no intersection
		{
			const vec3 SunLuminance = vec3(1000000.0); // arbitrary. But fine, not use when comparing the models
			return SunLuminance * (1.0 - 0.0);
		}
	}
// #endif
	return vec3(0.0f);
}

void SkyViewLutParamsToUv(AtmosphereParameters Atmosphere, in bool IntersectGround, in float viewZenithCosAngle, in float lightViewCosAngle, in float viewHeight, out vec2 uv)
{
	float Vhorizon = sqrt(viewHeight * viewHeight - Atmosphere.BottomRadius * Atmosphere.BottomRadius);
	float CosBeta = Vhorizon / viewHeight;				// GroundToHorizonCos
	float Beta = acos(CosBeta);
	float ZenithHorizonAngle = PI - Beta;

	if (!IntersectGround)
	{
		float coord = acos(viewZenithCosAngle) / ZenithHorizonAngle;
		coord = 1.0 - coord;
//#if NONLINEARSKYVIEWLUT
		coord = sqrt(coord);
//#endif
		coord = 1.0 - coord;
		uv.y = coord * 0.5f;
	}
	else
	{
		float coord = (acos(viewZenithCosAngle) - ZenithHorizonAngle) / Beta;
//#if NONLINEARSKYVIEWLUT
		coord = sqrt(coord);
//#endif
		uv.y = coord * 0.5f + 0.5f;
	}

	{
		float coord = -lightViewCosAngle * 0.5f + 0.5f;
		coord = sqrt(coord);
		uv.x = coord;
	}

	// Constrain uvs to valid sub texel range (avoid zenith derivative issue making LUT usage visible)
	uv = vec2(fromUnitToSubUvs(uv.x, 192.0f), fromUnitToSubUvs(uv.y, 108.0f));
}

void main()
{
	const vec2 pixPos = gl_FragCoord.xy;
	AtmosphereParameters Atmosphere = GetAtmosphereParameters();

	const vec2 ndcPos = pixPos / vec2(gResolution) * 2.0f - 1.0f;
	vec3 ClipSpace = vec3(ndcPos, 1.0);
	const vec4 HPos = gSkyInvViewProjMat * vec4(ClipSpace, 1.0);

	vec3 WorldDir = normalize(HPos.xyz / HPos.w - camera);
	vec3 WorldPos = camera + vec3(0, Atmosphere.BottomRadius, 0);

	float DepthBufferValue = -1.0;

	float viewHeight = length(WorldPos);
	vec3 L = vec3(0.0f);
	DepthBufferValue = 1.0f;//texture(ViewDepthTexture, pixPos).r;//1.0f;//ViewDepthTexture[pixPos].r;
// #if FASTSKY_ENABLED
	if (viewHeight < Atmosphere.TopRadius && DepthBufferValue == 1.0f)
	{
		vec2 uv;
		vec3 UpVector = normalize(WorldPos);
		float viewZenithCosAngle = dot(WorldDir, UpVector);

		vec3 sideVector = normalize(cross(UpVector, WorldDir));		// assumes non parallel vectors
		vec3 forwardVector = normalize(cross(sideVector, UpVector));	// aligns toward the sun light but perpendicular to up vector
		vec2 lightOnPlane = vec2(dot(sun_direction, forwardVector), dot(sun_direction, sideVector));
		lightOnPlane = normalize(lightOnPlane);
		float lightViewCosAngle = lightOnPlane.x;

		bool IntersectGround = raySphereIntersectNearest(WorldPos, WorldDir, vec3(0, 0, 0), Atmosphere.BottomRadius) >= 0.0f;

		SkyViewLutParamsToUv(Atmosphere, IntersectGround, viewZenithCosAngle, lightViewCosAngle, viewHeight, uv);


		//output.Luminance = float4(SkyViewLutTexture.SampleLevel(samplerLinearClamp, pixPos / float2(gResolution), 0).rgb + GetSunLuminance(WorldPos, WorldDir, Atmosphere.BottomRadius), 1.0);
		finalColor = vec4(textureLod(SkyViewLutTexture, uv, 0).rgb + GetSunLuminance(WorldPos, WorldDir, Atmosphere.BottomRadius), 1.0);
		vec3 whitepoint = vec3(1.08241, 0.96756, 0.95003);
		float exposure = 10.0;
		finalColor.rgb = vec3(1.0f) - exp(-finalColor.rgb / whitepoint * exposure);
		return;
	}
// #else
	if (DepthBufferValue == 1.0f)
		L += GetSunLuminance(WorldPos, WorldDir, Atmosphere.BottomRadius);

// #endif

//#if FASTAERIALPERSPECTIVE_ENABLED

//#if COLORED_TRANSMITTANCE_ENABLED
//#error The FASTAERIALPERSPECTIVE_ENABLED path does not support COLORED_TRANSMITTANCE_ENABLED.
//#else

	ClipSpace = vec3((pixPos / vec2(gResolution))*vec2(2.0, 2.0) - vec2(1.0, 1.0), DepthBufferValue);
	vec4 DepthBufferWorldPos =  gSkyInvViewProjMat * vec4(ClipSpace, 1.0);
	DepthBufferWorldPos /= DepthBufferWorldPos.w;
	float tDepth = length(DepthBufferWorldPos.xyz - (WorldPos + vec3(0.0, -Atmosphere.BottomRadius, 0.0)));
	float Slice = AerialPerspectiveDepthToSlice(tDepth);
	float Weight = 1.0;
	if (Slice < 0.5)
	{
		// We multiply by weight to fade to 0 at depth 0. That works for luminance and opacity.
		Weight = clamp(Slice * 2.0, 0, 1);
		Slice = 0.5;
	}
	float w = sqrt(Slice / AP_SLICE_COUNT);	// squared distribution

	const vec4 AP = Weight * textureLod(AtmosphereCameraScatteringVolume, vec3(pixPos / vec2(gResolution), w), 0);
	L.rgb += AP.rgb;
	float Opacity = AP.a;

	finalColor = vec4(L, Opacity);
	//output.Luminance *= frac(clamp(w*AP_SLICE_COUNT, 0, AP_SLICE_COUNT));
//#endif

//#else // FASTAERIALPERSPECTIVE_ENABLED

	// Move to top atmosphere as the starting point for ray marching.
	// This is critical to be after the above to not disrupt above atmosphere tests and voxel selection.
	if (!MoveToTopAtmosphere(WorldPos, WorldDir, Atmosphere.TopRadius))
	{
		// Ray is not intersecting the atmosphere
		finalColor = vec4(GetSunLuminance(WorldPos, WorldDir, Atmosphere.BottomRadius), 1.0);
		return;
	}

	const bool ground = false;
	const float SampleCountIni = 0.0f;
	const bool VariableSampleCount = true;
	const bool MieRayPhase = true;
	SingleScatteringResult ss = IntegrateScatteredLuminance(pixPos, WorldPos, WorldDir, sun_direction, Atmosphere, ground, SampleCountIni, DepthBufferValue, VariableSampleCount, MieRayPhase, 9000000.0f);

	L += ss.L;
	vec3 throughput = ss.Transmittance;

// #if COLORED_TRANSMITTANCE_ENABLED
// 	output.Luminance = float4(L, 1.0f);
// 	output.Transmittance = float4(throughput, 1.0f);
// #else
	const float Transmittance = dot(throughput, vec3(1.0f / 3.0f));
	finalColor = vec4(L, 1.0 - Transmittance);
	// finalColor.rgb = pow(finalColor.rgb, vec3(2.2f));
	// finalColor.a = 1.0f;
// #endif



// #endif // FASTAERIALPERSPECTIVE_ENABLED
}
