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
        void setMetallic(double metallic);
        void setRoughness(double roughness);

        DrawCommand createDrawCommand(Material* material, Geometry* geometry, int transformIndex);
        DrawCommand createLightDrawCommand(Material* material, Geometry* geometry, int transformIndex);
        DrawCommand createPbrUnifDrawCommand(Material* material, Geometry* geometry, int transformIndex);

    private:
        void createDefaultPbrTextures();
        std::unique_ptr<Material> createPbrMaterial(const std::string& type);

        Renderer*     m_renderer;
        Application*  m_app;

        std::unique_ptr<CameraController> m_cameraController;
        std::unique_ptr<UniformBuffer>    m_cameraBuffer;

        std::unique_ptr<VulkanPipeline> m_colorPipeline;
        std::unique_ptr<Material> m_colorMaterial;
        std::unique_ptr<VulkanImage> m_texture;
        std::unique_ptr<VulkanImageView> m_textureView;

        std::unique_ptr<VulkanPipeline> m_colorLightPipeline;
        std::unique_ptr<Material> m_colorLightMaterial;

        std::unique_ptr<VulkanPipeline> m_normalMapPipeline;
        std::unique_ptr<Material> m_normalMapMaterial;

        std::vector<std::pair<uint32_t, uint32_t>> m_geometryViews;
        std::vector<std::unique_ptr<VulkanImage>> m_normalMaps;
        std::vector<std::unique_ptr<VulkanImageView>> m_normalMapViews;
        std::vector<std::unique_ptr<Material>> m_materials;
        std::vector<std::unique_ptr<Geometry>> m_geometries;

        std::vector<RenderNode> m_renderNodes;

        std::vector<std::unique_ptr<VulkanImage>>     m_images;
        std::vector<std::unique_ptr<VulkanImageView>> m_imageViews;
        std::unique_ptr<VulkanPipeline> m_physBasedPipeline;
        std::unique_ptr<VulkanImage> m_envMap;
        std::unique_ptr<VulkanImageView> m_envMapView;

        std::unique_ptr<VulkanPipeline> m_irradiancePipeline;
        std::unique_ptr<VulkanPipeline> m_physBasedUnifPipeline;

        std::vector<TransformPack> m_transforms;
        std::unique_ptr<UniformBuffer> m_transformBuffer;

        std::unique_ptr<CascadedShadowMapper> m_cascadedShadowMapper;

        std::vector<LightDescriptorData> m_lights;
        std::unique_ptr<UniformBuffer> m_lightBuffer;

        std::unique_ptr<RenderGraph> m_renderGraph;
        std::unique_ptr<VulkanPipeline> m_shadowMapPipeline;
        std::unique_ptr<Material> m_shadowMapMaterial;

        std::unique_ptr<VulkanPipeline> m_colorShadowPipeline;
        std::unique_ptr<Material> m_colorShadowMaterial;

        std::vector<std::unique_ptr<VulkanPipeline>> m_csmPipelines;
        std::unique_ptr<Material> m_csmMaterial;


        std::unique_ptr<Grass> m_grass;

        std::unique_ptr<VulkanPipeline> m_lightShaftPipeline;
        std::unique_ptr<Material> m_lightShaftMaterial;
        std::unique_ptr<UniformBuffer> m_lightShaftBuffer;
        struct LightShaftParams
        {
            glm::vec2 lightPos;
            float exposure;
            float decay;
            float density;
            float weight;
        };
        LightShaftParams m_lightShaftParams;

        //std::unique_ptr<CascadedShadowMapper> m_csm;

        //std::unique_ptr<Geometry> m_planeGeometry;
        //std::unique_ptr<Geometry> m_cubeGeometry;
        std::unique_ptr<Geometry> m_treeGeometry;
        std::unique_ptr<Geometry> m_sphereGeometry;
        //std::unique_ptr<Geometry> m_cwPlaneGeometry;
        //
        //std::unique_ptr<BoxVisualizer> m_boxVisualizer;
        std::unique_ptr<Skybox> m_skybox;
        //
        //DescriptorSetGroup m_shadowMapDescGroup;
        //
        //std::vector<glm::mat4> m_lvpTransforms;
        //std::unique_ptr<UniformBuffer> m_lvpBuffer;
        //
        //std::vector<glm::mat4> m_vsmLightTransforms;
        //std::unique_ptr<UniformBuffer> m_vsmLightBuffer;
        //
        //std::unique_ptr<BlinnPhongPipeline> m_blinnPhongPipeline;
        //std::unique_ptr<ColorAndShadowPipeline> m_colorAndShadowPipeline;
        //std::array<DescriptorSetGroup, Renderer::NumVirtualFrames> m_colorAndShadowDescGroups;
        //std::array<DescriptorSetGroup, Renderer::NumVirtualFrames> m_blinnPhongDescGroups;
        //
        //std::unique_ptr<FullScreenQuadPipeline> m_fsQuadPipeline;
        //DescriptorSetGroup m_sceneDescSetGroup;
        //std::unique_ptr<VulkanImageView> m_sceneImageView;
        //
        std::unique_ptr<VulkanSampler> m_linearClampSampler;
        std::unique_ptr<VulkanSampler> m_nearestNeighborSampler;
        std::unique_ptr<VulkanSampler> m_linearRepeatSampler;
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

        std::unique_ptr<VulkanImage>       m_normalMap;
        std::unique_ptr<VulkanImageView>   m_normalMapView;

        //GaussianBlur m_blurParameters;
    };
}