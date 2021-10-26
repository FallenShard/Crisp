#version 450 core

layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 finalColor;

const float PI = 3.14159265358979323846f;

struct AtmosphereParameters
{
	// Radius of the planet (center to ground)
	float bottomRadius;

	// Maximum considered atmosphere height (center to atmosphere top)
	float topRadius;

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
};

layout(set = 0, binding = 1) uniform SamplesBuffer
{
	vec3 solar_irradiance;
	float sun_angular_radius;

	vec3 absorption_extinction;
	float mu_s_min;

	vec3 rayleigh_scattering;
	float mie_phase_function_g;

	vec3 mie_scattering;
	float bottom_radius;

	vec3 mie_extinction;
	float top_radius;

	vec3	mie_absorption;
	float	pad00;

	vec3 ground_albedo;
	float pad0;

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

AtmosphereParameters getAtmosphereParameters()
{
	AtmosphereParameters parameters;
	parameters.bottomRadius = bottom_radius;
	parameters.topRadius = top_radius;

	parameters.AbsorptionExtinction = absorption_extinction;

	// Traslation from Bruneton2017 parameterisation.
	parameters.RayleighDensityExpScale = rayleigh_density[1].w;
	parameters.MieDensityExpScale = mie_density[1].w;
	parameters.AbsorptionDensity0LayerWidth = absorption_density[0].x;
	parameters.AbsorptionDensity0ConstantTerm = absorption_density[1].x;
	parameters.AbsorptionDensity0LinearTerm = absorption_density[0].w;
	parameters.AbsorptionDensity1ConstantTerm = absorption_density[2].y;
	parameters.AbsorptionDensity1LinearTerm = absorption_density[2].x;

	parameters.MiePhaseG = mie_phase_function_g;
	parameters.RayleighScattering = rayleigh_scattering;
	parameters.MieScattering = mie_scattering;
	parameters.MieAbsorption = mie_absorption;
	parameters.MieExtinction = mie_extinction;
	parameters.GroundAlbedo = ground_albedo;
	
	return parameters;
}

void uvToLutTransmittanceParams(in AtmosphereParameters atmosphere, in vec2 uv, out float viewHeight, out float viewZenithCosAngle)
{
	const float x_mu = uv.x;
	const float x_r = uv.y;

	const float H = sqrt(atmosphere.topRadius * atmosphere.topRadius - atmosphere.bottomRadius * atmosphere.bottomRadius);
	const float rho = H * x_r;
	viewHeight = sqrt(rho * rho + atmosphere.bottomRadius * atmosphere.bottomRadius);

	const float d_min = atmosphere.topRadius - viewHeight;
	const float d_max = rho + H;
	const float d = d_min + x_mu * (d_max - d_min);
	viewZenithCosAngle = d == 0.0 ? 1.0f : (H * H - rho * rho - d * d) / (2.0 * viewHeight * d);
	viewZenithCosAngle = clamp(viewZenithCosAngle, -1.0, 1.0);
}

float raySphereIntersectNearest(vec3 r0, vec3 rd, vec3 s0, float sR)
{
	const float a = dot(rd, rd);
	const vec3 s0_r0 = r0 - s0;
	const float b = 2.0f * dot(rd, s0_r0);
	const float c = dot(s0_r0, s0_r0) - (sR * sR);
	const float delta = b * b - 4.0f * a * c;
	if (delta < 0.0f || a == 0.0f)
		return -1.0f;

	float sol0 = (-b - sqrt(delta)) / (2.0f * a);
	float sol1 = (-b + sqrt(delta)) / (2.0f * a);
	if (sol0 < 0.0f && sol1 < 0.0f)
		return -1.0f;

	if (sol0 < 0.0f)
		return max(0.0f, sol1);
	else if (sol1 < 0.0f)
		return max(0.0f, sol0);

	return max(0.0f, min(sol0, sol1));
}

float hgPhase(float g, float cosTheta)
{
	const float k = 3.0 / (8.0 * PI) * (1.0 - g * g) / (2.0 + g * g);
	return k * (1.0 + cosTheta * cosTheta) / pow(1.0 + g * g - 2.0 * g * -cosTheta, 1.5);
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
	const float viewHeight = length(WorldPos) - Atmosphere.bottomRadius;

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

vec3 IntegrateOpticalDepth(
	in vec2 pixPos, in vec3 WorldPos, in vec3 WorldDir, in vec3 SunDir, in AtmosphereParameters atmosphere,
	in float SampleCountIni, in bool VariableSampleCount)
{
	// Compute next intersection with atmosphere or ground 
	const vec3 earthOrigin = vec3(0.0f);
	const float tBottom = raySphereIntersectNearest(WorldPos, WorldDir, earthOrigin, atmosphere.bottomRadius);
	const float tTop = raySphereIntersectNearest(WorldPos, WorldDir, earthOrigin, atmosphere.topRadius);
	float tMax = 0.0f;
	if (tBottom < 0.0f)
	{
		// No intersection occurs with either of the layers
		if (tTop < 0.0f)
			return vec3(0.0f);

		// Intersection occurs with just the outer layer
		tMax = tTop;
	}
	else
	{
		// We intersect both layers, pick the closer intersection
		tMax = min(tTop, tBottom);
	}

	// Sample count 
	float SampleCount = SampleCountIni;
	float SampleCountFloor = SampleCountIni;
	float tMaxFloor = tMax;
	if (VariableSampleCount)
	{
		SampleCount = mix(RayMarchMinMaxSPP.x, RayMarchMinMaxSPP.y, clamp(tMax * 0.01f, 0.0f, 1.0f));
		SampleCountFloor = floor(SampleCount);
		tMaxFloor = tMax * SampleCountFloor / SampleCount;	// rescale tMax to map to the last entire step segment.
	}
	float dt = tMax / SampleCount;

	// Phase functions
	const float uniformPhase = 1.0 / (4.0 * PI);
	const vec3 wi = SunDir;
	const vec3 wo = WorldDir;
	const float cosTheta = dot(wi, wo);
	const float MiePhaseValue = hgPhase(atmosphere.MiePhaseG, -cosTheta);	// mnegate cosTheta because due to WorldDir being a "in" direction. 
	const float RayleighPhaseValue = RayleighPhase(cosTheta);

	// Ray march the atmosphere to integrate optical depth
	vec3 opticalDepth = vec3(0.0f);
	float t = 0.0f;
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
			// Exact difference, important for accuracy of multiple scattering
			const float tNext = tMax * (s + SampleSegmentT) / SampleCount;
			dt = tNext - t;
			t = tNext;
		}

		const vec3 P = WorldPos + t * WorldDir;
		MediumSampleRGB medium = sampleMediumRGB(P, atmosphere);

		// Accumulate optical depth along the ray
		opticalDepth += medium.extinction * dt;
	}

	return opticalDepth;
}

void main()
{
	vec2 pixPos = vec2(gl_FragCoord.xy);
	AtmosphereParameters atmosphere = getAtmosphereParameters();

	// Compute camera position from LUT coords
	vec2 uv = (pixPos) / vec2(TRANSMITTANCE_TEXTURE_WIDTH, TRANSMITTANCE_TEXTURE_HEIGHT);
	float viewHeight;
	float viewZenithCosAngle;
	uvToLutTransmittanceParams(atmosphere, uv, viewHeight, viewZenithCosAngle);

	//  A few extra needed constants
	vec3 worldPos = vec3(0.0f, viewHeight, 0.0f);
	vec3 worldDir = vec3(0.0f, viewZenithCosAngle, -sqrt(1.0 - viewZenithCosAngle * viewZenithCosAngle));

	const float SampleCountIni = 40.0f;
	const bool VariableSampleCount = false;
	const vec3 transmittance = exp(-IntegrateOpticalDepth(pixPos, worldPos, worldDir, sun_direction, atmosphere, SampleCountIni, VariableSampleCount));
	finalColor = vec4(transmittance, 1.0f);
}