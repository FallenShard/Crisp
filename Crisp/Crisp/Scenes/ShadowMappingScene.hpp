#pragma once

#include <array>
#include <memory>
#include <vector>

#include "Scene.hpp"
#include <Crisp/Renderer/Renderer.hpp>

#include <Crisp/Geometry/TransformPack.hpp>
#include <Crisp/Lights/LightDescriptor.hpp>
#include <Crisp/Renderer/Material.hpp>

#include <Crisp/Core/HashMap.hpp>

namespace crisp {
namespace gui {
class Form;
class Panel;
} // namespace gui

struct GaussianBlur {
    glm::vec2 texelSize;
    float sigma;
    int radius;
};

struct PbrUnifMaterial {
    glm::vec4 albedo;
    float metallic;
    float roughness;
};

class FreeCameraController;

class TransformBuffer;
class LightSystem;

class ShadowMapper;

class BlurPass;

class VarianceShadowMapPass;

class VulkanImageView;

struct RenderNode;

class VulkanDevice;

class VulkanSampler;
class BoxVisualizer;
class Skybox;

class Grass;
class RayTracingMaterial;

class ShadowMappingScene : public Scene {
public:
    ShadowMappingScene(Renderer* renderer, Window* window);
    ~ShadowMappingScene();

    virtual void resize(int width, int height) override;
    virtual void update(float dt) override;
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

    std::unique_ptr<FreeCameraController> m_cameraController;

    std::unique_ptr<LightSystem> m_lightSystem;

    std::unique_ptr<TransformBuffer> m_transformBuffer;

    std::unique_ptr<ShadowMapper> m_shadowMapper;

    robin_hood::unordered_flat_map<std::string, std::unique_ptr<RenderNode>> m_renderNodes;

    std::vector<ManyLightDescriptor> m_manyLights;
    std::unique_ptr<UniformBuffer> m_manyLightsBuffer;

    std::unique_ptr<UniformBuffer> m_tilePlaneBuffer;
    std::unique_ptr<UniformBuffer> m_lightIndexCountBuffer;
    std::unique_ptr<UniformBuffer> m_lightIndexListBuffer;
    std::unique_ptr<VulkanImage> m_lightGrid;
    std::vector<std::unique_ptr<VulkanImageView>> m_lightGridViews;

    PbrUnifMaterial m_pbrUnifMaterial;

    std::unordered_map<std::string, std::unique_ptr<VulkanBuffer>> m_computeBuffers;

    std::unique_ptr<BoxVisualizer> m_boxVisualizer;
    std::unique_ptr<Skybox> m_skybox;

    // std::unique_ptr<Grass> m_grass;

    // std::unique_ptr<Material> m_lightShaftMaterial;
    // std::unique_ptr<UniformBuffer> m_lightShaftBuffer;
    // struct LightShaftParams
    //{
    //     glm::vec2 lightPos;
    //     float exposure;
    //     float decay;
    //     float density;
    //     float weight;
    // };
    // LightShaftParams m_lightShaftParams;

    ////std::unique_ptr<CascadedShadowMapper> m_csm;

    //
    // std::unique_ptr<BlurPass> m_blurPass;
    // std::unique_ptr<BlurPipeline> m_blurPipeline;
    // std::array<DescriptorSetGroup, RendererConfig::VirtualFrameCount> m_blurDescGroups;
    //
    // std::unique_ptr<BlurPass> m_vertBlurPass;
    // std::unique_ptr<BlurPipeline> m_vertBlurPipeline;
    // std::array<DescriptorSetGroup, RendererConfig::VirtualFrameCount> m_vertBlurDescGroups;
    //
    // std::unique_ptr<VarianceShadowMapPass> m_vsmPass;
    // std::unique_ptr<VarianceShadowMapPipeline> m_vsmPipeline;
    // DescriptorSetGroup m_vsmDescSetGroup;

    // GaussianBlur m_blurParameters;
};
} // namespace crisp