#pragma once

#include <vector>
#include <memory>
#include <array>

#include "Renderer/Renderer.hpp"
#include "Renderer/DrawCommand.hpp"
#include "Geometry/TransformPack.hpp"
#include "Renderer/DescriptorSetGroup.hpp"
#include "Scene.hpp"

#include "Renderer/Material.hpp"

#include "Lights/LightDescriptorData.hpp"

namespace crisp
{
    struct GaussianBlur
    {
        glm::vec2 texelSize;
        float sigma;
        int radius;
    };

    struct PbrUnifMaterial
    {
        glm::vec3 albedo;
        float metallic;
        float roughness;
    };

    class Application;
    class CameraController;

    class CascadedShadowMapper;

    class BlurPass;

    class VarianceShadowMapPass;

    class VulkanImageView;
    class UniformBuffer;

    class Renderer;
    class RenderGraph;

    class VulkanDevice;

    class VulkanSampler;
    class BoxVisualizer;
    class Skybox;

    class Geometry;
    class Grass;

    struct RenderNode
    {
        Material* material = nullptr;
        Geometry* geometry = nullptr;
        int transformIndex = -1;
        RenderNode(Material* mat, Geometry* geom, int index) : material(mat), geometry(geom), transformIndex(index) {}
    };

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
        void createDefaultPbrTextures();
        std::unique_ptr<Material> createPbrMaterial(const std::string& type, VulkanPipeline* pipeline);
        std::unique_ptr<Material> createUnifPbrMaterial(VulkanPipeline* pipeline);

        void setupInput();
        std::pair<std::unique_ptr<VulkanImage>, std::unique_ptr<VulkanImageView>> convertEquirectToCubeMap(std::shared_ptr<VulkanImageView> equirectMapView);
        void setupDiffuseEnvMap(const VulkanImageView& cubeMapView);
        void setupReflectEnvMap(const VulkanImageView& cubeMapView);
        std::unique_ptr<VulkanImage> integrateBrdfLut();

        Renderer*     m_renderer;
        Application*  m_app;

        std::unique_ptr<CameraController> m_cameraController;
        std::unique_ptr<UniformBuffer>    m_cameraBuffer;

        std::vector<std::unique_ptr<Material>> m_materials;
        std::vector<std::unique_ptr<Geometry>> m_geometries;
        std::vector<std::unique_ptr<VulkanPipeline>> m_pipelines;

        Material* m_activeMaterial;

        std::vector<RenderNode> m_renderNodes;

        std::vector<std::unique_ptr<VulkanImage>>     m_images;
        std::vector<std::unique_ptr<VulkanImageView>> m_imageViews;
        std::unordered_map<std::string, std::unique_ptr<VulkanImageView>> m_defaultTexImageViews;
        std::shared_ptr<VulkanImage> m_envRefMap;
        std::shared_ptr<VulkanImageView> m_envRefMapView;

        std::unique_ptr<VulkanImage> m_filteredMap;
        std::unique_ptr<VulkanImageView> m_filteredMapView;

        std::unique_ptr<VulkanImage> m_envIrrMap;
        std::unique_ptr<VulkanImageView> m_envIrrMapView;

        std::unique_ptr<VulkanImage> m_brdfLut;
        std::unique_ptr<VulkanImageView> m_brdfLutView;

        std::vector<TransformPack> m_transforms;
        std::unique_ptr<UniformBuffer> m_transformBuffer;

        std::unique_ptr<CascadedShadowMapper> m_cascadedShadowMapper;

        std::vector<LightDescriptorData> m_lights;
        std::unique_ptr<UniformBuffer> m_lightBuffer;

        PbrUnifMaterial m_pbrUnifMaterial;
        std::unique_ptr<UniformBuffer> m_pbrUnifMatBuffer;

        std::unique_ptr<RenderGraph> m_renderGraph;

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

        std::unique_ptr<BoxVisualizer> m_boxVisualizer;
        //std::unique_ptr<Skybox> m_skybox;

        std::unique_ptr<VulkanSampler> m_linearClampSampler;
        std::unique_ptr<VulkanSampler> m_nearestNeighborSampler;
        std::unique_ptr<VulkanSampler> m_linearRepeatSampler;
        std::unique_ptr<VulkanSampler> m_mipmapSampler;
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