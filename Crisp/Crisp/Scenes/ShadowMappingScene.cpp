#include <Crisp/Scenes/ShadowMappingScene.hpp>

#include <Crisp/Camera/FreeCameraController.hpp>
#include <Crisp/Core/Application.hpp>
#include <Crisp/Core/Window.hpp>

#include <Crisp/Renderer/RenderPasses/CubeMapRenderPass.hpp>
#include <Crisp/Renderer/RenderPasses/ForwardLightingPass.hpp>
#include <Crisp/Renderer/RenderPasses/ShadowPass.hpp>
#include <Crisp/Renderer/RenderPasses/TexturePass.hpp>

#include <Crisp/Materials/PbrMaterialUtils.hpp>
#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/RenderGraph.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/UniformBuffer.hpp>
#include <Crisp/Renderer/VulkanBufferUtils.hpp>
#include <Crisp/Renderer/VulkanImageUtils.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>
#include <Crisp/Vulkan/VulkanPipeline.hpp>
#include <Crisp/Vulkan/VulkanSampler.hpp>
#include <Crisp/vulkan/VulkanDevice.hpp>
#include <Crisp/vulkan/VulkanImage.hpp>

#include <Crisp/Renderer/RenderPasses/BlurPass.hpp>

#include <Crisp/Lights/DirectionalLight.hpp>
#include <Crisp/Lights/EnvironmentLighting.hpp>
#include <Crisp/Lights/PointLight.hpp>

#include <Crisp/Lights/LightClustering.hpp>
#include <Crisp/Lights/ShadowMapper.hpp>
#include <Crisp/Models/BoxVisualizer.hpp>

#include <Crisp/GUI/Form.hpp>
#include <Crisp/GUI/Panel.hpp>
#include <Crisp/GUI/Slider.hpp>

#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Mesh/TriangleMeshUtils.hpp>
#include <Crisp/Models/Grass.hpp>
#include <Crisp/Models/Skybox.hpp>

#include <Crisp/Math/Constants.hpp>
#include <Crisp/Utils/LuaConfig.hpp>

#include <random>
#include <thread>

#include <Crisp/Geometry/TransformBuffer.hpp>
#include <Crisp/Lights/LightSystem.hpp>

#include <Crisp/Mesh/Io/MeshLoader.hpp>
#include <Crisp/Renderer/ResourceContext.hpp>
#include <Crisp/Utils/Profiler.hpp>

#include <Crisp/Core/Logger.hpp>

#include <Crisp/Image/Io/Utils.hpp>

namespace crisp
{
namespace
{
bool renderAll = true;
glm::vec3 lightWorldPos = 300.0f * glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f));

float planeTilt = 0.0f;
float angle = glm::radians(90.0f);
glm::vec4 pointLightPos = glm::vec4(5.0f * std::cosf(angle), 3.0f, 5.0f * std::sinf(angle), 1.0f);
bool animate = false;

static constexpr uint32_t ShadowMapSize = 2048;

static constexpr uint32_t NumLights = 1;

static constexpr uint32_t CascadeCount = 4;

static constexpr const char* DepthPrePass = "depthPrePass";
static constexpr const char* MainPass = "mainPass";
static constexpr const char* CsmPass = "csmPass";
static constexpr const char* VsmPass = "vsmPass";

int MaterialIndex = 1;

PointLight pointLight(glm::vec3(1250.0f), glm::vec3(10.0f), glm::normalize(glm::vec3(-1.0f)));

static const int DefaultShadowMap = 0;

int lightMode = 0;

glm::vec4 color = glm::vec4(10, 0, 0, 1);

glm::vec2 terrainPushConstants = glm::vec2(1.0f, 15.0f);
} // namespace

ShadowMappingScene::ShadowMappingScene(Renderer* renderer, Application* app)
    : AbstractScene(app, renderer)
{
    setupInput();

    //// Camera
    // m_cameraController = std::make_unique<CameraController>(app->getWindow());
    // m_resourceContext->createUniformBuffer("camera", sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);

    // m_renderGraph = std::make_unique<RenderGraph>(m_renderer);

    //// Main render pass
    // m_renderGraph->addRenderPass(MainPass, std::make_unique<ForwardLightingPass>(m_renderer, VK_SAMPLE_COUNT_4_BIT));

    ////auto& depthPrePass = m_renderGraph->addRenderPass(DepthPrePass, std::make_unique<DepthPass>(m_renderer));
    ////m_renderGraph->addDependency(DepthPrePass, MainPass, 0);

    //// Shadow map pass
    // m_renderGraph->addRenderPass(CsmPass, std::make_unique<ShadowPass>(m_renderer, ShadowMapSize, CascadeCount));
    // m_renderGraph->addDependency(CsmPass, MainPass, 0, CascadeCount);
    ////
    ////auto& vsmPassNode = m_renderGraph->addRenderPass(VsmPass, std::make_unique<VarianceShadowMapPass>(m_renderer,
    /// ShadowMapSize)); /m_renderGraph->addDependency(VsmPass, MainPass, 0);
    ////

    //// Wrap-up render graph definition
    // m_renderGraph->addDependency(MainPass, "SCREEN", 2);
    // m_renderGraph->sortRenderPasses();
    // m_renderer->setSceneImageView(m_renderGraph->getNode(MainPass).renderPass.get(), 2);

    // m_lightSystem = std::make_unique<LightSystem>(m_renderer, ShadowMapSize);
    // DirectionalLight dirLight(-glm::vec3(1, 1, 0), glm::vec3(3.0f), glm::vec3(-5), glm::vec3(5));
    // m_lightSystem->setDirectionalLight(dirLight);
    // m_lightSystem->enableCascadedShadowMapping(CascadeCount, ShadowMapSize);

    //// Object transforms
    // m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, 100);

    //// Geometry setup
    // std::vector<VertexAttributeDescriptor> shadowVertexFormat = { VertexAttribute::Position };
    // m_resourceContext->addGeometry("cubeRT", std::make_unique<Geometry>(m_renderer, createCubeMesh({
    // VertexAttribute::Position, VertexAttribute::Normal }))); m_resourceContext->addGeometry("cubeShadow",
    // std::make_unique<Geometry>(m_renderer, createCubeMesh(shadowVertexFormat)));
    // m_resourceContext->addGeometry("sphereShadow", std::make_unique<Geometry>(m_renderer,
    // createSphereMesh(shadowVertexFormat))); m_resourceContext->addGeometry("floorShadow",
    // std::make_unique<Geometry>(m_renderer, createPlaneMesh(shadowVertexFormat)));

    //{
    //    Profiler profiler("Common");
    //    createCommonTextures();

    //    for (uint32_t i = 0; i < CascadeCount; ++i)
    //    {
    //        std::string key = "cascadedShadowMap" + std::to_string(i);
    //        auto csmPipeline = m_resourceContext->createPipeline(key, "ShadowMap.lua",
    //        m_renderGraph->getRenderPass(CsmPass), i); auto csmMaterial = m_resourceContext->createMaterial(key,
    //        csmPipeline); csmMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
    //        csmMaterial->writeDescriptor(0, 1, *m_lightSystem->getCascadedDirectionalLightBuffer(i));
    //    }
    //}
    //{

    //    Profiler profiler("Plane, boxes, balls");
    //    createPlane();
    //    createBox();
    //    createShaderballs();
    //}

    // m_skybox = std::make_unique<Skybox>(m_renderer, m_renderGraph->getRenderPass(MainPass),
    // *m_resourceContext->getImageView("cubeMap"), *m_resourceContext->getSampler("linearClamp"));

    // createTrees();

    // m_renderer->getDevice()->flushDescriptorUpdates();

    // createTerrain();

    // m_app->getForm()->add(gui::createShadowMappingSceneGui(m_app->getForm(), this));
}

ShadowMappingScene::~ShadowMappingScene()
{
    m_app->getForm()->remove("shadowMappingPanel");
}

void ShadowMappingScene::resize(int, int)
{
    /* m_cameraController->resize(width, height);

     m_renderGraph->resize(width, height);
     m_renderer->setSceneImageView(m_renderGraph->getNode(MainPass).renderPass.get(), 2);*/
}

void ShadowMappingScene::update(float /*dt*/)
{
    ////static float t = 0;
    ////
    //////if (animate)
    //////{
    //////    angle += 2.0f * PI * dt / 5.0f;
    //////    pointLightPos.x = 5.0f * std::cosf(angle);
    //////    pointLightPos.z = 5.0f * std::sinf(angle);
    //////    m_transforms[3].M = glm::translate(glm::vec3(pointLightPos)) * glm::scale(glm::vec3(0.2f));
    //////}
    ////
    // m_cameraController->update(dt);
    // const auto& V = m_cameraController->getViewMatrix();
    // const auto& P = m_cameraController->getProjectionMatrix();

    // m_transformBuffer->update(V, P);
    // m_resourceContext->getUniformBuffer("camera")->updateStagingBuffer(m_cameraController->getCameraParameters(),
    // sizeof(CameraParameters));

    // m_lightSystem->update(m_cameraController->getCamera(), dt);

    // if (m_boxVisualizer)
    //     m_boxVisualizer->update(V, P);
    // m_skybox->updateTransforms(V, P);

    ////m_resourceContext->getUniformBuffer("pbrUnifParams")->updateStagingBuffer(m_pbrUnifMaterial);

    //////auto screen = m_renderer->getVulkanSwapChainExtent();
    //////
    //////glm::vec3 pt = glm::project(lightWorldPos, V, P, glm::vec4(0.0f, 0.0f, screen.width, screen.height));
    //////pt.x /= screen.width;
    //////pt.y /= screen.height;
    //////m_lightShaftParams.lightPos = glm::vec2(pt);
    //////m_lightShaftBuffer->updateStagingBuffer(m_lightShaftParams);
    ////
    ////m_cascadedShadowMapper->setZ(2.0f * PI * t / 5.0f);
    ////m_cascadedShadowMapper->recalculateLightProjections(m_cameraController->getCamera());
    ////
    ////m_lightBuffer->updateStagingBuffer(m_lights.data(), m_lights.size() * sizeof(LightDescriptor));
    ////

    ////m_boxVisualizer->updateFrusta(m_cascadedShadowMapper.get(), m_cameraController.get());
    ////
    ////m_shadowMapper->setLightTransform(0, pointLight.getProjectionMatrix() * pointLight.getViewMatrix());
    ////m_shadowMapper->setLightFullTransform(0, pointLight.getViewMatrix(), pointLight.getProjectionMatrix());
    ////
    ////t += dt;
}

void ShadowMappingScene::render()
{
    // m_renderer->enqueueDrawCommand([this](VkCommandBuffer cmdBuffer)
    //{
    //     m_rayTracer->traceRays(cmdBuffer, m_renderer->getCurrentVirtualFrameIndex(), *m_rayTracingMaterial);
    // });

    /* m_renderGraph->clearCommandLists();
     if (renderAll)
         m_renderGraph->buildCommandLists(m_renderNodes);

     m_renderGraph->addToCommandLists(m_skybox->getRenderNode());
     m_renderGraph->executeCommandLists();*/
}

void ShadowMappingScene::setBlurRadius(int /*radius*/)
{
    // m_blurParameters.radius = radius;
    // m_blurParameters.sigma = radius == 0 ? 1.0f : static_cast<float>(radius) / 3.0f;
}

void ShadowMappingScene::setSplitLambda(double lambda)
{
    m_lightSystem->setSplitLambda(static_cast<float>(lambda));
}

void ShadowMappingScene::setRedAlbedo(double red)
{
    m_pbrUnifMaterial.albedo.r = static_cast<float>(red);
}

void ShadowMappingScene::setGreenAlbedo(double green)
{
    m_pbrUnifMaterial.albedo.g = static_cast<float>(green);
}

void ShadowMappingScene::setBlueAlbedo(double blue)
{
    m_pbrUnifMaterial.albedo.b = static_cast<float>(blue);
}

void ShadowMappingScene::setMetallic(double metallic)
{
    m_pbrUnifMaterial.metallic = static_cast<float>(metallic);
}

void ShadowMappingScene::setRoughness(double roughness)
{
    m_pbrUnifMaterial.roughness = static_cast<float>(roughness);
}

void ShadowMappingScene::onMaterialSelected(const std::string& materialName)
{
    m_renderer->finish();

    auto material = m_resourceContext->getMaterial("pbrTex");

    auto& imageCache = m_resourceContext->imageCache;

    const std::vector<std::string> texNames = {"diffuse", "metallic", "roughness", "normal", "ao"};
    const std::vector<VkFormat> formats = {
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM};
    for (uint32_t i = 0; i < texNames.size(); ++i)
    {
        const std::string& filename = "PbrMaterials/" + materialName + "/" + texNames[i] + ".png";
        const auto& path = m_renderer->getResourcesPath() / "Textures" / filename;
        if (std::filesystem::exists(path))
        {
            std::string key = materialName + "-" + texNames[i];
            imageCache.addImageWithView(
                key,
                createVulkanImage(
                    *m_renderer,
                    loadImage(m_renderer->getResourcesPath() / "Textures" / filename, 4, FlipOnLoad::Y).unwrap(),
                    formats[i]));
            material->writeDescriptor(1, 2 + i, imageCache.getImageView(key), imageCache.getSampler("linearRepeat"));
        }
        else
        {
            spdlog::warn("Texture type {} is using default values for '{}'", materialName, texNames[i]);
            std::string key = "default-" + texNames[i];
            material->writeDescriptor(1, 2 + i, imageCache.getImageView(key), imageCache.getSampler("linearRepeat"));
        }
    }
    m_renderer->getDevice().flushDescriptorUpdates();
}

RenderNode* ShadowMappingScene::createRenderNode(std::string id, int transformIndex)
{
    const TransformHandle handle{static_cast<uint16_t>(transformIndex), 0};
    auto node = std::make_unique<RenderNode>(*m_transformBuffer, handle);
    m_renderNodes.emplace(id, std::move(node));
    return m_renderNodes.at(id).get();
}

void ShadowMappingScene::createCommonTextures()
{
    auto& imageCache = m_resourceContext->imageCache;
    imageCache.addSampler(
        "nearestNeighbor",
        std::make_unique<VulkanSampler>(
            m_renderer->getDevice(), VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));
    imageCache.addSampler(
        "linearRepeat",
        std::make_unique<VulkanSampler>(
            m_renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 16.0f, 12.0f));
    imageCache.addSampler(
        "linearMipmap",
        std::make_unique<VulkanSampler>(
            m_renderer->getDevice(),
            VK_FILTER_LINEAR,
            VK_FILTER_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            16.0f,
            5.0f));
    imageCache.addSampler(
        "linearClamp",
        std::make_unique<VulkanSampler>(
            m_renderer->getDevice(),
            VK_FILTER_LINEAR,
            VK_FILTER_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            16.0f,
            1.0f));
    imageCache.addSampler(
        "linearMirrorRepeat",
        std::make_unique<VulkanSampler>(
            m_renderer->getDevice(),
            VK_FILTER_LINEAR,
            VK_FILTER_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
            16.0f,
            11.0f));

    // For textured pbr
    addPbrTexturesToImageCache(createDefaultPbrTextureGroup(), "default", imageCache);

    // Environment map
    LuaConfig config(m_renderer->getResourcesPath() / "Scripts/scene.lua");
    auto hdrName = config.get<std::string>("environmentMap").value_or("GreenwichPark") + ".hdr";
    auto envRefMap = createVulkanImage(
        *m_renderer,
        loadImage(m_renderer->getResourcesPath() / "Textures/EnvironmentMaps" / hdrName, 4, FlipOnLoad::Y).unwrap(),
        VK_FORMAT_R32G32B32A32_SFLOAT);
    std::shared_ptr<VulkanImageView> envRefMapView = envRefMap->createView(VK_IMAGE_VIEW_TYPE_2D);

    auto [cubeMap, cubeMapView] = convertEquirectToCubeMap(m_renderer, envRefMapView, 1024);
    setupDiffuseEnvMap(m_renderer, *cubeMapView, 64);
    setupReflectEnvMap(m_renderer, *cubeMapView, 1024);
    imageCache.addImageWithView("cubeMap", std::move(cubeMap), std::move(cubeMapView));
    imageCache.addImageWithView("brdfLut", integrateBrdfLut(m_renderer));

    m_resourceContext->createPipeline("pbrTex", "PbrTex.lua", m_renderGraph->getRenderPass(MainPass), 0);
}

Material* ShadowMappingScene::createPbrTexMaterial(const std::string& type)
{
    auto& imageCache = m_resourceContext->imageCache;
    auto material = m_resourceContext->createMaterial("pbrTex" + type, "pbrTex");
    material->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
    material->writeDescriptor(0, 1, *m_resourceContext->getUniformBuffer("camera"));

    material->writeDescriptor(1, 0, *m_lightSystem->getCascadedDirectionalLightBuffer());
    material->writeDescriptor(
        1, 1, *m_renderGraph->getNode(CsmPass).renderPass, 0, &imageCache.getSampler("nearestNeighbor"));

    const std::vector<std::string> texNames = {"diffuse", "metallic", "roughness", "normal", "ao"};
    const std::vector<VkFormat> formats = {
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM};
    for (uint32_t i = 0; i < texNames.size(); ++i)
    {
        const std::string& filename = "PbrMaterials/" + type + "/" + texNames[i] + ".png";
        const auto& path = m_renderer->getResourcesPath() / "Textures" / filename;
        if (std::filesystem::exists(path))
        {
            std::string key = type + "-" + texNames[i];
            imageCache.addImageWithView(
                key,
                createVulkanImage(
                    *m_renderer,
                    loadImage(m_renderer->getResourcesPath() / "Textures" / filename, 4, FlipOnLoad::Y).unwrap(),
                    formats[i]));
            material->writeDescriptor(1, 2 + i, imageCache.getImageView(key), imageCache.getSampler("linearRepeat"));
        }
        else
        {
            spdlog::warn("Texture type {} is using default values for '{}'", type, texNames[i]);
            std::string key = "default-" + texNames[i];
            material->writeDescriptor(1, 2 + i, imageCache.getImageView(key), imageCache.getSampler("linearRepeat"));
        }
    }

    material->writeDescriptor(2, 0, imageCache.getImageView("envIrrMap"), imageCache.getSampler("linearClamp"));
    material->writeDescriptor(2, 1, imageCache.getImageView("filteredMap"), imageCache.getSampler("linearMipmap"));
    material->writeDescriptor(2, 2, imageCache.getImageView("brdfLut"), imageCache.getSampler("linearClamp"));
    return material;
}

void ShadowMappingScene::createSkyboxNode() {}

void ShadowMappingScene::createTerrain()
{
    /*std::unique_ptr<ImageFileBuffer> image = std::make_unique<ImageFileBuffer>(m_renderer->getResourcesPath() /
    "Textures/heightmap.jpg", 1);

    m_images["heightMap"] = convertToVulkanImage(m_renderer, *image, VK_FORMAT_R8_UNORM);
    m_imageViews["heightMap"] = m_images["heightMap"]->createView(VK_IMAGE_VIEW_TYPE_2D);

    int tileCount = 128;



    int numVertsPerDim = tileCount + 1;
    std::vector<glm::vec3> data;
    for (int i = 0; i < numVertsPerDim; i++)
        for (int j = 0; j < numVertsPerDim; j++)
            data.push_back(glm::vec3(j, 0, i));

    std::vector<glm::uvec4> indexData;
    for (int i = 0; i < tileCount; i++)
        for (int j = 0; j < tileCount; j++)
            indexData.push_back(glm::uvec4(i * numVertsPerDim + j, (i + 1) * numVertsPerDim + j, (i + 1) *
    numVertsPerDim + j + 1, i * numVertsPerDim + j + 1));

    m_geometries["terrain"] = std::make_unique<Geometry>(m_renderer, data, indexData);

    auto pipeline = createPipeline("terrain", "Terrain.lua", MainPass, 0);
    auto material = createMaterial("terrain", pipeline);
    material->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
    material->writeDescriptor(0, 1,
    m_imageViews["heightMap"]->getDescriptorInfo(m_samplers["linearMirrorRepeat"].get())); material->writeDescriptor(0,
    2, *m_uniformBuffers["camera"]);

    m_renderer->getDevice()->flushDescriptorUpdates();*/

    /*RenderNode terrain(*m_transformBuffer, 4);
    terrain.transformPack->M = glm::translate(glm::vec3(-tileCount / 2.0f, 0.0f, -tileCount / 2.0f));
    terrain.geometry = m_geometries["terrain"].get();
    terrain.pipeline = pipeline;
    terrain.material = material;
    terrain.setPushConstantView(terrainPushConstants);*/

    // m_renderGraph->getNode(MainPass).renderNodes.push_back(terrain);
}

void ShadowMappingScene::createShaderballs()
{
    // std::vector<VertexAttributeDescriptor> shadowVertexFormat = { VertexAttribute::Position };
    // std::vector<VertexAttributeDescriptor> shadingVertexFormat = { VertexAttribute::Position,
    // VertexAttribute::Normal/*, VertexAttribute::TexCoord, VertexAttribute::Tangent, VertexAttribute::Bitangent*/ };
    // std::vector<VertexAttributeDescriptor> pbrVertexFormat = { VertexAttribute::Position, VertexAttribute::Normal,
    // VertexAttribute::TexCoord, VertexAttribute::Tangent, VertexAttribute::Bitangent };

    ////TriangleMesh mesh("D:/Downloads/3D Models/rungholt/house.obj", pbrVertexFormat);
    // TriangleMesh mesh = createSphereMesh(pbrVertexFormat);
    ////m_geometries.emplace("shaderballShadow", std::make_unique<Geometry>(m_renderer, m_renderer->getResourcesPath() /
    ///"Meshes/ShaderBall_FWVN_PosX.obj", shadowVertexFormat));
    // m_resourceContext->addGeometry("shaderBall", std::make_unique<Geometry>(m_renderer, mesh, shadingVertexFormat));
    // m_resourceContext->addGeometry("shaderBallPbr", std::make_unique<Geometry>(m_renderer, mesh, pbrVertexFormat));
    // m_resourceContext->addGeometry("shaderBallShadow", std::make_unique<Geometry>(m_renderer, mesh,
    // shadowVertexFormat));

    ////auto goochShaderBall = createRenderNode("goochShaderBall", 1);
    ////goochShaderBall->transformPack->M = glm::mat4(1.0f);// glm::scale(glm::vec3(0.01f));
    ////goochShaderBall->geometry = m_geometries.at("shaderball").get();
    ////goochShaderBall->pass(MainPass).material = m_materials.at("gooch").get();
    ////goochShaderBall->pass(MainPass).pipeline = m_pipelines.at("gooch").get();

    // auto pbrTexMaterial = createPbrTexMaterial("RedBricks");

    // auto pbrPipeline = m_resourceContext->createPipeline("pbrUnif", "PbrUnif.lua",
    // m_renderGraph->getRenderPass(MainPass), 0); auto pbrMaterial = m_resourceContext->createMaterial("pbrUnif",
    // pbrPipeline); pbrMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
    // pbrMaterial->writeDescriptor(0, 1, *m_resourceContext->getUniformBuffer("camera"));

    // m_pbrUnifMaterial.albedo = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
    // m_pbrUnifMaterial.metallic = 0.1f;
    // m_pbrUnifMaterial.roughness = 0.1f;
    // m_resourceContext->addUniformBuffer("pbrUnifParams", std::make_unique<UniformBuffer>(m_renderer,
    // m_pbrUnifMaterial, BufferUpdatePolicy::PerFrame)); pbrMaterial->writeDescriptor(0, 2,
    // *m_resourceContext->getUniformBuffer("pbrUnifParams")); pbrMaterial->writeDescriptor(1, 0,
    // *m_lightSystem->getCascadedDirectionalLightBuffer()); pbrMaterial->writeDescriptor(1, 1,
    // m_renderGraph->getRenderPass(CsmPass), 0, m_resourceContext->getSampler("nearestNeighbor"));
    // pbrMaterial->writeDescriptor(2, 0, *m_resourceContext->getImageView("envIrrMap"),
    // m_resourceContext->getSampler("linearClamp")); pbrMaterial->writeDescriptor(2, 1,
    // *m_resourceContext->getImageView("filteredMap"), m_resourceContext->getSampler("linearMipmap"));
    // pbrMaterial->writeDescriptor(2, 2, *m_resourceContext->getImageView("brdfLut"),
    // m_resourceContext->getSampler("linearClamp"));

    // auto pbrShaderBall = createRenderNode("pbrShaderBall", 3);
    // pbrShaderBall->transformPack->M = glm::mat4(1.0f); //glm::translate(glm::vec3(3.0f, 0.0f, 2.0f));//
    // *glm::scale(glm::vec3(0.02f)); pbrShaderBall->geometry = m_resourceContext->getGeometry("shaderBallPbr");
    // pbrShaderBall->pass(MainPass).material = pbrTexMaterial;
    // pbrShaderBall->pass(MainPass).setPushConstants(glm::vec2(1.0f));

    // for (uint32_t i = 0; i < CascadeCount; ++i)
    //{
    //     auto& data = pbrShaderBall->subpass(CsmPass, i);
    //     data.geometry = m_resourceContext->getGeometry("shaderBallShadow");
    //     data.material = m_resourceContext->getMaterial("cascadedShadowMap" + std::to_string(i));
    // }
}

void ShadowMappingScene::createTrees()
{
    const VertexLayoutDescription shadowVertexFormat = {{VertexAttribute::Position}};
    const VertexLayoutDescription shadowAlphaVertexFormat = {
        {VertexAttribute::Position, VertexAttribute::TexCoord}
    };
    const VertexLayoutDescription pbrVertexFormat = {
        {VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord, VertexAttribute::Tangent}
    };

    TriangleMesh treeMesh(
        loadTriangleMesh(m_renderer->getResourcesPath() / "Meshes/white_oak/white_oak.obj", flatten(pbrVertexFormat))
            .unwrap());
    m_resourceContext->addGeometry("tree", std::make_unique<Geometry>(*m_renderer, treeMesh, pbrVertexFormat));
    m_resourceContext->addGeometry("treeShadow", std::make_unique<Geometry>(*m_renderer, treeMesh, shadowVertexFormat));
    m_resourceContext->addGeometry(
        "treeShadowAlpha", std::make_unique<Geometry>(*m_renderer, treeMesh, shadowAlphaVertexFormat));

    // Create the alpha-mask material
    auto alphaPipeline =
        m_resourceContext->createPipeline("alphaMask", "AlphaMask.lua", m_renderGraph->getRenderPass(MainPass), 0);
    auto alphaMaterial = m_resourceContext->createMaterial("alphaMask", alphaPipeline);
    alphaMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());

    auto& imageCache = m_resourceContext->imageCache;
    const Image image(
        loadImage(m_renderer->getResourcesPath() / "white_oak/T_White_Oak_Leaves_Hero_1_D.png", 4, FlipOnLoad::Y)
            .unwrap());
    imageCache.addImageWithView("leaves", createVulkanImage(*m_renderer, image, VK_FORMAT_R8G8B8A8_SRGB));
    alphaMaterial->writeDescriptor(1, 0, imageCache.getImageView("leaves"), imageCache.getSampler("linearClamp"));

    const Image normalMap(
        loadImage(m_renderer->getResourcesPath() / "white_oak/T_White_Oak_Leaves_Hero_1_N.png", 4, FlipOnLoad::Y)
            .unwrap());
    imageCache.addImageWithView("leavesNormalMap", createVulkanImage(*m_renderer, normalMap, VK_FORMAT_R8G8B8A8_UNORM));
    alphaMaterial->writeDescriptor(
        1, 1, imageCache.getImageView("leavesNormalMap"), imageCache.getSampler("linearClamp"));

    alphaMaterial->writeDescriptor(1, 2, *m_lightSystem->getCascadedDirectionalLightBuffer());
    alphaMaterial->writeDescriptor(1, 3, *m_resourceContext->getUniformBuffer("camera"));
    alphaMaterial->writeDescriptor(
        1, 4, m_renderGraph->getRenderPass(CsmPass), 0, &imageCache.getSampler("nearestNeighbor"));

    alphaMaterial->writeDescriptor(2, 0, imageCache.getImageView("envIrrMap"), imageCache.getSampler("linearClamp"));
    alphaMaterial->writeDescriptor(2, 1, imageCache.getImageView("filteredMap"), imageCache.getSampler("linearMipmap"));
    alphaMaterial->writeDescriptor(2, 2, imageCache.getImageView("brdfLut"), imageCache.getSampler("linearClamp"));

    auto trunkPipeline =
        m_resourceContext->createPipeline("treeTrunk", "TreeTrunk.lua", m_renderGraph->getRenderPass(MainPass), 0);

    auto createOpaqueMaterial =
        [this, trunkPipeline](std::string materialKey, std::string diffuseMapFilename, std::string normalMapFilename)
    {
        auto& imageCache = m_resourceContext->imageCache;
        auto material = m_resourceContext->createMaterial(materialKey, trunkPipeline);

        material->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());

        const Image ambientMap =
            loadImage(m_renderer->getResourcesPath() / diffuseMapFilename, 4, FlipOnLoad::Y).unwrap();
        imageCache.addImageWithView(
            diffuseMapFilename, createVulkanImage(*m_renderer, ambientMap, VK_FORMAT_R8G8B8A8_SRGB));
        material->writeDescriptor(
            1, 0, imageCache.getImageView(diffuseMapFilename), imageCache.getSampler("linearRepeat"));

        const Image normalMap =
            loadImage(m_renderer->getResourcesPath() / normalMapFilename, 4, FlipOnLoad::Y).unwrap();
        imageCache.addImageWithView(
            normalMapFilename, createVulkanImage(*m_renderer, normalMap, VK_FORMAT_R8G8B8A8_UNORM));
        material->writeDescriptor(
            1, 1, imageCache.getImageView(normalMapFilename), imageCache.getSampler("linearRepeat"));

        material->writeDescriptor(1, 2, *m_lightSystem->getCascadedDirectionalLightBuffer());
        material->writeDescriptor(1, 3, *m_resourceContext->getUniformBuffer("camera"));
        material->writeDescriptor(
            1, 4, m_renderGraph->getRenderPass(CsmPass), 0, &imageCache.getSampler("nearestNeighbor"));

        material->writeDescriptor(2, 0, imageCache.getImageView("envIrrMap"), imageCache.getSampler("linearClamp"));
        material->writeDescriptor(2, 1, imageCache.getImageView("filteredMap"), imageCache.getSampler("linearMipmap"));
        material->writeDescriptor(2, 2, imageCache.getImageView("brdfLut"), imageCache.getSampler("linearClamp"));

        return material;
    };

    auto trunkMaterial =
        createOpaqueMaterial("trunkMaterial", "white_oak/T_WhiteOakBark_D.png", "white_oak/T_WhiteOakBark_N.png");
    auto capsMaterial = createOpaqueMaterial("capsMaterial", "white_oak/T_Cap_02_D.png", "white_oak/T_Cap_02_N.png");

    std::vector<glm::mat4> transforms(70);

    std::default_random_engine eng(std::random_device{}());
    std::uniform_real_distribution<float> distrib(-2.0f, 2.0f);
    std::uniform_real_distribution<float> xi(0.0f, 1.0f);

    float spacing = 7.0f;
    int rowCount = 8;
    int colCount = 8;
    for (int i = 0; i < rowCount; i++)
    {
        for (int j = 0; j < colCount; j++)
        {
            int id = i * colCount + j + 5;
            glm::vec3 jitter = glm::vec3(distrib(eng), 0.0f, distrib(eng));
            float rndScale = xi(eng);
            transforms[id] = glm::translate(glm::vec3(spacing * i, 0.0f, -spacing * j) + jitter) *
                             glm::rotate(distrib(eng), glm::vec3(0.0f, 1.0f, 0.0f)) *
                             glm::scale(glm::vec3(0.01f, 0.01f + 0.005f * rndScale, 0.01f));
            auto trunk = createRenderNode("oakTrunk_" + std::to_string(id - 5), id);

            std::cout << "Setting model matrix: " << id << std::endl;
            trunk->setModelMatrix(transforms[id]);
            trunk->geometry = m_resourceContext->getGeometry("tree");
            trunk->pass(1, MainPass).material = trunkMaterial;
            trunk->pass(0, MainPass).material = capsMaterial;

            for (uint32_t c = 0; c < CascadeCount; ++c)
            {
                auto csmMaterial = m_resourceContext->getMaterial("cascadedShadowMap" + std::to_string(c));

                auto& subpassMat = trunk->subpass(1, CsmPass, c);
                subpassMat.geometry = m_resourceContext->getGeometry("treeShadow");
                subpassMat.material = csmMaterial;

                auto& subpassMatCaps = trunk->subpass(0, CsmPass, c);
                subpassMatCaps.geometry = m_resourceContext->getGeometry("treeShadow");
                subpassMatCaps.material = csmMaterial;
            }
        }
    }

    for (uint32_t c = 0; c < CascadeCount; ++c)
    {
        std::string key = "cascadedShadowMapAlpha" + std::to_string(c);
        auto csmPipeline =
            m_resourceContext->createPipeline(key, "ShadowMapAlpha.lua", m_renderGraph->getRenderPass(CsmPass), c);
        auto csmMaterial = m_resourceContext->createMaterial(key, csmPipeline);
        csmMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
        csmMaterial->writeDescriptor(0, 1, *m_lightSystem->getCascadedDirectionalLightBuffer(c));
        csmMaterial->writeDescriptor(1, 0, imageCache.getImageView("leaves"), imageCache.getSampler("linearClamp"));
    }

    for (int i = 0; i < rowCount; i++)
    {
        for (int j = 0; j < colCount; j++)
        {

            int id = i * colCount + j + 5;

            auto trunk = createRenderNode("oakLeaves_" + std::to_string(id - 5), id);
            std::cout << "Reading model matrix: " << id << std::endl;
            trunk->setModelMatrix(transforms[id]);
            trunk->geometry = m_resourceContext->getGeometry("tree");
            trunk->pass(2, MainPass).material = alphaMaterial;
            trunk->pass(3, MainPass).material = alphaMaterial;

            for (uint32_t c = 0; c < CascadeCount; ++c)
            {
                std::string key = "cascadedShadowMapAlpha" + std::to_string(c);
                auto csmMaterial = m_resourceContext->getMaterial(key);

                auto& subpassMat2 = trunk->subpass(2, CsmPass, c);
                subpassMat2.geometry = m_resourceContext->getGeometry("treeShadowAlpha");
                subpassMat2.material = csmMaterial;

                auto& subpassMat3 = trunk->subpass(3, CsmPass, c);
                subpassMat3.geometry = m_resourceContext->getGeometry("treeShadowAlpha");
                subpassMat3.material = csmMaterial;
            }
        }
    }
}

void ShadowMappingScene::createPlane()
{
    // std::vector<VertexAttributeDescriptor> shadingVertexFormat = { VertexAttribute::Position,
    // VertexAttribute::Normal/*, VertexAttribute::TexCoord, VertexAttribute::Tangent, VertexAttribute::Bitangent*/ };
    // m_resourceContext->addGeometry("floor", std::make_unique<Geometry>(m_renderer,
    // createPlaneMesh(shadingVertexFormat)));
    //
    // auto floor = createRenderNode("floor", 0);
    // floor->transformPack->M = glm::scale(glm::vec3(100.0f, 1.0f, 100.0f));
    // floor->geometry = m_resourceContext->getGeometry("floor");
    // auto pipeline = m_resourceContext->createPipeline("gooch", "Gooch.lua", m_renderGraph->getRenderPass(MainPass),
    // 0); auto material = m_resourceContext->createMaterial("gooch", pipeline); material->writeDescriptor(0, 0,
    // m_transformBuffer->getDescriptorInfo()); glm::vec4 goochMaterial[] =
    //{
    //     glm::vec4(1.0, 0.0, 0.0, 1.0),
    //     glm::vec4(1.0, 1.0, 1.0, 1.0)
    // };
    // m_resourceContext->addUniformBuffer("goochColors", std::make_unique<UniformBuffer>(m_renderer, goochMaterial));
    // material->writeDescriptor(1, 0, *m_resourceContext->getUniformBuffer("goochColors"));
    ////material->writeDescriptor(1, 1, *m_lightSystem->getDirectionalLightBuffer());
    // material->writeDescriptor(1, 1, *m_resourceContext->getUniformBuffer("camera"));
    ////material->writeDescriptor(1, 3, *m_renderGraph->getNode(ShadowMapPass).renderPass, 0,
    /// m_resourceContext->getSampler("nearestNeighbor"));
    // material->writeDescriptor(1, 2, *m_lightSystem->getCascadedDirectionalLightBuffer());
    // material->writeDescriptor(1, 3, *m_renderGraph->getNode(CsmPass).renderPass, 0,
    // m_resourceContext->getSampler("nearestNeighbor"));
    //
    // floor->pass(MainPass).material = material;

    const VertexLayoutDescription PbrVertexFormat = {
        {VertexAttribute::Position},
        {VertexAttribute::Normal, VertexAttribute::TexCoord, VertexAttribute::Tangent}
    };
    m_resourceContext->addGeometry(
        "floor",
        std::make_unique<Geometry>(*m_renderer, createPlaneMesh(flatten(PbrVertexFormat), 200.0f), PbrVertexFormat));

    auto floor = createRenderNode("floor", 0);
    floor->transformPack->M = glm::scale(glm::vec3(1.0, 1.0f, 1.0f));
    floor->geometry = m_resourceContext->getGeometry("floor");
    floor->pass(MainPass).material = createPbrTexMaterial("MixedMoss");
    floor->pass(MainPass).setPushConstants(glm::vec2(100.0f));

    m_renderer->getDevice().flushDescriptorUpdates();
}

void ShadowMappingScene::createBox()
{
    // std::vector<VertexAttributeDescriptor> shadingVertexFormat = { VertexAttribute::Position,
    // VertexAttribute::Normal/*, VertexAttribute::TexCoord, VertexAttribute::Tangent, VertexAttribute::Bitangent*/ };
    // m_geometries.emplace("cube", std::make_unique<Geometry>(m_renderer, createCubeMesh(shadingVertexFormat)));
    //
    // auto cube = createRenderNode("cube", 2);
    // cube->geometry = m_geometries.at("cube").get();
    // cube->transformPack->M = glm::translate(glm::vec3(-2.0f, 0.50f, -3.0f)) * glm::rotate(glm::radians(60.0f),
    // glm::vec3(0.0f, 1.0f, 0.0f)); cube->pass(MainPass).material = m_materials.at("gooch").get();
    //
    ////cube->materials[ShadowMapPass].geometry = m_geometries.at("cubeShadow").get();
    ////cube->materials[ShadowMapPass].pipeline = m_pipelines.at("shadowMap").get();
    ////cube->materials[ShadowMapPass].material = m_materials.at("shadowMap").get();
    //
    // for (uint32_t i = 0; i < CascadeCount; ++i)
    //{
    //    auto& data = cube->subpass(CsmPass, i);
    //    data.geometry = m_geometries.at("cubeShadow").get();
    //    data.material = m_materials.at("cascadedShadowMap" + std::to_string(i)).get();
    //}
}

void ShadowMappingScene::setupInput()
{
    m_app->getWindow().keyPressed += [this](Key key, int)
    {
        switch (key)
        {
        case Key::Space:
            animate = !animate;
            break;

        case Key::C:
        {
            glm::vec4 cameraColors[4] = {
                glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
                glm::vec4(1.0f, 0.3f, 0.3f, 1.0f),
                glm::vec4(1.0f, 0.6f, 0.6f, 1.0f),
                glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)};
            for (int i = 0; i < 4; ++i)
            {
                float lo = m_lightSystem->getCascadeSplitLo(i);
                float hi = m_lightSystem->getCascadeSplitHi(i);
                m_boxVisualizer->setBoxCorners(i, m_cameraController->getCamera().computeFrustumPoints(lo, hi));
                m_boxVisualizer->setBoxColor(i, cameraColors[i]);
            }

            glm::vec4 colors[4] = {
                glm::vec4(1.0f, 1.0f, 0.0f, 1.0f),
                glm::vec4(0.0f, 1.0f, 0.0f, 1.0f),
                glm::vec4(0.0f, 1.0f, 1.0f, 1.0f),
                glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)};
            for (int i = 4; i < 8; ++i)
            {
                m_boxVisualizer->setBoxCorners(i, m_lightSystem->getCascadeFrustumPoints(i - 4));
                m_boxVisualizer->setBoxColor(i, colors[i - 4]);
            }
            break;
        }

        case Key::F5:
            m_resourceContext->recreatePipelines();
            break;

        case Key::R:
            renderAll = !renderAll;
            break;
        }

        // if (key == Key::M)
        //     lightMode = lightMode == 0 ? 1 : 0;
        //
        // if (key == Key::)
    };
}

std::unique_ptr<gui::Panel> createShadowMappingSceneGui(gui::Form* form, ShadowMappingScene* scene)
{
    using namespace crisp::gui;

    std::unique_ptr<Panel> panel = std::make_unique<Panel>(form);

    panel->setId("shadowMappingPanel");
    panel->setPadding({20, 20});
    panel->setPosition({20, 40});
    panel->setVerticalSizingPolicy(SizingPolicy::WrapContent);
    panel->setHorizontalSizingPolicy(SizingPolicy::WrapContent);

    int y = 0;

    auto numSamplesLabel = std::make_unique<Label>(form, "CSM Split Lambda");
    numSamplesLabel->setPosition({0, y});
    panel->addControl(std::move(numSamplesLabel));
    y += 20;

    auto lambdaSlider = std::make_unique<DoubleSlider>(form);
    lambdaSlider->setId("lambdaSlider");
    lambdaSlider->setAnchor(Anchor::TopCenter);
    lambdaSlider->setOrigin(Origin::TopCenter);
    lambdaSlider->setPosition({0, y});
    lambdaSlider->setMinValue(0.0f);
    lambdaSlider->setMaxValue(1.0f);
    lambdaSlider->setValue(0.5f);
    lambdaSlider->setIncrement(0.01f);
    lambdaSlider->valueChanged.subscribe<&ShadowMappingScene::setSplitLambda>(scene);
    panel->addControl(std::move(lambdaSlider));
    y += 30;

    auto roughnessSlider = std::make_unique<DoubleSlider>(form, 0.0f, 1.0f);
    roughnessSlider->setId("roughnessSlider");
    roughnessSlider->setAnchor(Anchor::TopCenter);
    roughnessSlider->setOrigin(Origin::TopCenter);
    roughnessSlider->setPosition({0, y});
    roughnessSlider->setValue(0.0f);
    roughnessSlider->setIncrement(0.01f);
    roughnessSlider->valueChanged.subscribe<&ShadowMappingScene::setRoughness>(scene);
    panel->addControl(std::move(roughnessSlider));
    y += 30;

    auto metallicSlider = std::make_unique<DoubleSlider>(form, 0.0f, 1.0f);
    metallicSlider->setId("metallicSlider");
    metallicSlider->setAnchor(Anchor::TopCenter);
    metallicSlider->setOrigin(Origin::TopCenter);
    metallicSlider->setPosition({0, y});
    metallicSlider->setValue(0.0f);
    metallicSlider->setIncrement(0.01f);
    metallicSlider->valueChanged.subscribe<&ShadowMappingScene::setMetallic>(scene);
    panel->addControl(std::move(metallicSlider));
    y += 30;

    auto redSlider = std::make_unique<DoubleSlider>(form, 0.0f, 1.0f);
    redSlider->setId("redSlider");
    redSlider->setAnchor(Anchor::TopCenter);
    redSlider->setOrigin(Origin::TopCenter);
    redSlider->setPosition({0, y});
    redSlider->setValue(0.0f);
    redSlider->setIncrement(0.01f);
    redSlider->valueChanged.subscribe<&ShadowMappingScene::setRedAlbedo>(scene);
    panel->addControl(std::move(redSlider));
    y += 30;

    auto greenSlider = std::make_unique<DoubleSlider>(form, 0.0f, 1.0f);
    greenSlider->setId("greenSlider");
    greenSlider->setAnchor(Anchor::TopCenter);
    greenSlider->setOrigin(Origin::TopCenter);
    greenSlider->setPosition({0, y});
    greenSlider->setValue(0.0f);
    greenSlider->setIncrement(0.01f);
    greenSlider->valueChanged.subscribe<&ShadowMappingScene::setGreenAlbedo>(scene);
    panel->addControl(std::move(greenSlider));
    y += 30;

    auto blueSlider = std::make_unique<DoubleSlider>(form, 0.0f, 1.0f);
    blueSlider->setId("blueSlider");
    blueSlider->setAnchor(Anchor::TopCenter);
    blueSlider->setOrigin(Origin::TopCenter);
    blueSlider->setPosition({0, y});
    blueSlider->setValue(0.0f);
    blueSlider->setIncrement(0.01f);
    blueSlider->valueChanged.subscribe<&ShadowMappingScene::setBlueAlbedo>(scene);
    panel->addControl(std::move(blueSlider));
    y += 30;

    return panel;
}
} // namespace crisp