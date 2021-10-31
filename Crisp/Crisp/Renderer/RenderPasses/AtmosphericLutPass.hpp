#pragma once

#include <Crisp/Vulkan/VulkanRenderPass.hpp>

#include <CrispCore/Math/Headers.hpp>

namespace crisp
{
    static constexpr float EarthMieScaleHeight = 1.2f;
    static constexpr float EarthRayleighScaleHeight = 8.0f;
    struct SkyAtmosphereConstantBufferStructure
    {
        //
        // From AtmosphereParameters
        //
        glm::vec3 solar_irradiance = glm::vec3(1.0f, 1.0f, 1.0f);
        float sun_angular_radius = 0.004675f;

        glm::vec3 absorption_extinction = glm::vec3(0.000650f, 0.001881f, 0.000085f);
        float mu_s_min = -0.5f;

        glm::vec3 rayleigh_scattering{ 0.005802f, 0.013558f, 0.033100f };
        float mie_phase_function_g = 0.80f;

        glm::vec3 mie_scattering = glm::vec3(0.003996f);
        float bottom_radius = 6360.0f;

        glm::vec3 mie_extinction = glm::vec3(0.004440f);
        float top_radius = 6460.0f;

        glm::vec3 mie_absorption = glm::vec3(0.00044f);
        float pad00 = -0.00142f;

        glm::vec3 ground_albedo = glm::vec3(0.0f);
        float pad0 = -0.00142f;

        float rayleigh_density[12] = {
            0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, -1.0f / EarthRayleighScaleHeight,
            0.0f, 0.0f, -0.00142f, -0.00142f
        };
        float mie_density[12] = {
            0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, -1.0f / EarthMieScaleHeight,
            0.0f, 0.0f, -0.00142f, -0.00142f
        };
        float absorption_density[12] = {
            25.0f, 0.0f, 0.0f, 1.0f / 15.0f,
            -2.0f / 3.0f, 0.0f, 0.0f, 0.0f,
            -1.0f / 15.0f, 8.0f / 3.0f, -0.00142f, -0.00142f
        };

        //
        // Add generated static header constant
        //

        int TRANSMITTANCE_TEXTURE_WIDTH = 256;
        int TRANSMITTANCE_TEXTURE_HEIGHT = 64;
        int IRRADIANCE_TEXTURE_WIDTH = 64;
        int IRRADIANCE_TEXTURE_HEIGHT = 16;

        int SCATTERING_TEXTURE_R_SIZE = 32;
        int SCATTERING_TEXTURE_MU_SIZE = 128;
        int SCATTERING_TEXTURE_MU_S_SIZE = 32;
        int SCATTERING_TEXTURE_NU_SIZE = 8;

        glm::vec3 SKY_SPECTRAL_RADIANCE_TO_LUMINANCE = { 114974.91406, 71305.95313, 65310.54688 };

        float  pad3;
        glm::vec3 SUN_SPECTRAL_RADIANCE_TO_LUMINANCE = { 98242.78906, 69954.39844, 66475.01563 };

        float  pad4;

        //
        // Other globals
        //
        glm::mat4 gSkyViewProjMat = glm::transpose(glm::mat4{
            - 0.85633, 0.00, 0.00, 0.00,
            0.00, 0.00, 1.52236, -0.76118,
            0.00, 1.00001, 0.00, 0.90 ,
            0.00, 1.00, 0.00, 1.00 ,

        });
        glm::mat4 gSkyInvViewProjMat = glm::transpose(glm::mat4{
            - 1.16778, 0.00, 0.00, 0.00,
            0.00, 0.00, 9.99995, -9.00,
            0.00, 0.65688, -4.99997, 5.00,
            0.00, 0.00, -9.99995, 10.00,


            });
        glm::mat4 gShadowmapViewProjMat = glm::transpose(glm::mat4{
            0.01, 0.00, 0.00, 0.00,
            0.00, -0.00435, 0.009, 0.00,
            0.00, -0.0045, -0.00217, 0.50,
            0.00, 0.00, 0.00, 1.00,

            });

        //glm::vec3 camera = {0.00, -1.00, 0.50 };
        glm::vec3 camera = { 0.00, 0.5, 1.0 };
        float  pad5;
        //glm::vec3 sun_direction = { 0.00, 0.90045, 0.43497 };
        glm::vec3 sun_direction = { 0.00, 0.43497, -0.90045 };
        float  pad6;
       // glm::vec3 view_ray = { 0.00, 0.00, -1.00 };
        glm::vec3 view_ray = { 0.00, 1.00, -1.00 };

        float  pad7;

        float MultipleScatteringFactor = 1.0;
        float MultiScatteringLUTRes = 32;
        float pad9;
        float pad10;

        float padded[48];
    };

    constexpr auto sz = sizeof(SkyAtmosphereConstantBufferStructure);


    class TransmittanceLutPass : public VulkanRenderPass
    {
    public:
        TransmittanceLutPass(Renderer* renderer);

    protected:
        virtual void createResources() override;
    };

    class SkyViewLutPass : public VulkanRenderPass
    {
    public:
        SkyViewLutPass(Renderer* renderer);

    protected:
        virtual void createResources() override;
    };

    class CameraVolumesPass : public VulkanRenderPass
    {
    public:
        CameraVolumesPass(Renderer* renderer);

        inline const std::vector<std::unique_ptr<VulkanImageView>>& getArrayViews() const {
            return m_arrayViews;
        }

    protected:
        virtual void createResources() override;

        std::vector<std::unique_ptr<VulkanImageView>> m_arrayViews;
    };

    struct CommonConstantBufferStructure
    {
        glm::mat4 gSkyViewProjMat = glm::transpose(glm::mat4{
           -0.85633, 0.00, 0.00, 0.00,
           0.00, 0.00, 1.52236, -0.76118,
           0.00, 1.00001, 0.00, 0.90 ,
           0.00, 1.00, 0.00, 1.00 ,
        });

        glm::vec4 gColor{ 0.0f, 1.0f, 1.0f, 1.0f };

        glm::vec3 gSunIlluminance{1.0f, 1.0f, 1.0f};
        int gScatteringMaxPathDepth = 4;

        glm::uvec2 gResolution = { 1280, 720 };
        float gFrameTimeSec{ 15.265f };
        float gTimeSec{ 10.0f };

        glm::vec2 RayMarchMinMaxSPP = { 4.0f, 14.0f };
        glm::vec2 pad = {};

        glm::uvec2 gMouseLastDownPos = { 0, 0 };
        unsigned int gFrameId{ 42 };
        unsigned int gTerrainResolution{512};
        //float gScreenshotCaptureActive{ 0.0 };
    };

    class RayMarchingPass : public VulkanRenderPass
    {
    public:
        RayMarchingPass(Renderer* renderer);

    protected:
        virtual void createResources() override;
    };
}
