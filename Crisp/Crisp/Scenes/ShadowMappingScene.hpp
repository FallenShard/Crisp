#pragma once

#include <vector>
#include <memory>
#include <array>

#include <Crisp/Renderer/Renderer.hpp>
#include "Scene.hpp"

#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Lights/LightDescriptor.hpp>
#include <Crisp/Geometry/TransformPack.hpp>

#include <CrispCore/RobinHood.hpp>

namespace crisp
{
    namespace gui
    {
        class Form;
        class Panel;
    }

    struct GaussianBlur
    {
        glm::vec2 texelSize;
        float sigma;
        int radius;
    };

    struct PbrUnifMaterial
    {
        glm::vec4 albedo;
        float metallic;
        float roughness;
    };

    class Application;
    class FreeCameraController;

    class TransformBuffer;
    class LightSystem;

    class ShadowMapper;

    class BlurPass;

    class VarianceShadowMapPass;

    class VulkanImageView;

    class Renderer;
    class RenderGraph;
    struct RenderNode;

    class VulkanDevice;

    class VulkanSampler;
    class BoxVisualizer;
    class Skybox;

    class Grass;
    class RayTracingMaterial;
    class VulkanRayTracer;

    class ResourceContext;

    class ShadowMappingScene : public AbstractScene
    {
    public:
        ShadowMappingScene(Renderer* renderer, Application* app);
        ~ShadowMappingScene();

        virtual void resize(int width, int height) override;
        virtual void update(float dt)  override;
        virtual void render() override;

        void setBlurRadius(int radius);
        void setSplitLambda(double lambda);
        void setRedAlbedo(double red);
        void setGreenAlbedo(double green);
        void setBlueAlbedo(double blue);
        void setMetallic(double metallic);
        void setRoughness(double roughness);
        void onMaterialSelected(const std::string& material);

    private:
        RenderNode* createRenderNode(std::string id, int transformIndex);

        void createCommonTextures();

        std::unique_ptr<gui::Panel> createShadowMappingSceneGui(gui::Form* form, ShadowMappingScene* scene);


        Material* createPbrTexMaterial(const std::string& type);
        void createSkyboxNode();
        void createTerrain();
        void createShaderballs();
        void createTrees();
        void createPlane();
        void createBox();

        void setupInput();
        std::pair<std::unique_ptr<VulkanImage>, std::unique_ptr<VulkanImageView>> convertEquirectToCubeMap(std::shared_ptr<VulkanImageView> equirectMapView, uint32_t cubeMapSize);
        void setupDiffuseEnvMap(const VulkanImageView& cubeMapView, uint32_t cubeMapSize);
        void setupReflectEnvMap(const VulkanImageView& cubeMapView, uint32_t cubeMapSize);
        std::unique_ptr<VulkanImage> integrateBrdfLut();

        Renderer*    m_renderer;
        Application* m_app;

        std::unique_ptr<FreeCameraController> m_cameraController;

        std::unique_ptr<LightSystem> m_lightSystem;

        std::unique_ptr<TransformBuffer> m_transformBuffer;

        std::unique_ptr<ShadowMapper> m_shadowMapper;

        robin_hood::unordered_flat_map<std::string, std::unique_ptr<RenderNode>> m_renderNodes;

        std::unique_ptr<ResourceContext> m_resourceContext;

        std::vector<ManyLightDescriptor> m_manyLights;
        std::unique_ptr<UniformBuffer> m_manyLightsBuffer;

        std::unique_ptr<UniformBuffer> m_tilePlaneBuffer;
        std::unique_ptr<UniformBuffer> m_lightIndexCountBuffer;
        std::unique_ptr<UniformBuffer> m_lightIndexListBuffer;
        std::unique_ptr<VulkanImage> m_lightGrid;
        std::vector<std::unique_ptr<VulkanImageView>> m_lightGridViews;

        PbrUnifMaterial m_pbrUnifMaterial;

        std::unique_ptr<RenderGraph> m_renderGraph;

        std::unordered_map<std::string, std::unique_ptr<VulkanBuffer>> m_computeBuffers;

        std::unique_ptr<BoxVisualizer> m_boxVisualizer;
        std::unique_ptr<Skybox>        m_skybox;

        std::unique_ptr<VulkanRayTracer> m_rayTracer;
        std::unique_ptr<RayTracingMaterial> m_rayTracingMaterial;

        //std::unique_ptr<Grass> m_grass;

        //std::unique_ptr<Material> m_lightShaftMaterial;
        //std::unique_ptr<UniformBuffer> m_lightShaftBuffer;
        //struct LightShaftParams
        //{
        //    glm::vec2 lightPos;
        //    float exposure;
        //    float decay;
        //    float density;
        //    float weight;
        //};
        //LightShaftParams m_lightShaftParams;

        ////std::unique_ptr<CascadedShadowMapper> m_csm;

        //
        //std::unique_ptr<BlurPass> m_blurPass;
        //std::unique_ptr<BlurPipeline> m_blurPipeline;
        //std::array<DescriptorSetGroup, Renderer::NumVirtualFrames> m_blurDescGroups;
        //
        //std::unique_ptr<BlurPass> m_vertBlurPass;
        //std::unique_ptr<BlurPipeline> m_vertBlurPipeline;
        //std::array<DescriptorSetGroup, Renderer::NumVirtualFrames> m_vertBlurDescGroups;
        //
        //std::unique_ptr<VarianceShadowMapPass> m_vsmPass;
        //std::unique_ptr<VarianceShadowMapPipeline> m_vsmPipeline;
        //DescriptorSetGroup m_vsmDescSetGroup;

        //GaussianBlur m_blurParameters;
    };
}