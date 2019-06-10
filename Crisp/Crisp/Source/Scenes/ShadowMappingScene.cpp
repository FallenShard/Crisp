#include "ShadowMappingScene.hpp"

#include "Core/Application.hpp"
#include "Core/Window.hpp"
#include "IO/ImageFileBuffer.hpp"
#include "Camera/CameraController.hpp"

#include "Renderer/RenderPasses/ShadowPass.hpp"
#include "Renderer/RenderPasses/SceneRenderPass.hpp"
#include "Renderer/Pipelines/BlinnPhongPipelines.hpp"
#include "Renderer/Pipelines/ShadowMapPipelines.hpp"
#include "Renderer/Pipelines/FullScreenQuadPipeline.hpp"
#include "Renderer/Pipelines/NormalMapPipeline.hpp"
#include "Renderer/Pipelines/UniformColorPipeline.hpp"
#include "Renderer/Pipelines/PhysicallyBasedPipeline.hpp"
#include "Renderer/Pipelines/GrassPipeline.hpp"
#include "Vulkan/VulkanImageView.hpp"
#include "vulkan/VulkanImage.hpp"
#include "vulkan/VulkanDevice.hpp"
#include "Renderer/Renderer.hpp"
#include "Renderer/UniformBuffer.hpp"
#include "vulkan/VulkanSampler.hpp"
#include "Renderer/Texture.hpp"
#include "Renderer/Material.hpp"

#include "Renderer/Pipelines/LightShaftPipeline.hpp"

#include "Renderer/RenderPasses/VarianceShadowMapPass.hpp"
#include "Renderer/RenderPasses/CubeMapRenderPass.hpp"
#include "Renderer/RenderPasses/TexturePass.hpp"

#include "Renderer/RenderPasses/BlurPass.hpp"
#include "Renderer/Pipelines/BlurPipeline.hpp"
#include "Renderer/RenderGraph.hpp"

#include "Renderer/RenderPasses/LightShaftPass.hpp"


#include "Lights/DirectionalLight.hpp"
#include "Lights/PointLight.hpp"

#include "Techniques/CascadedShadowMapper.hpp"
#include "Models/BoxVisualizer.hpp"
#include "Geometry/TriangleMesh.hpp"

#include "GUI/Form.hpp"
#include "ShadowMappingPanel.hpp"

#include "Models/Skybox.hpp"
#include "Geometry/Geometry.hpp"
#include "Geometry/MeshGenerators.hpp"
#include "Models/Grass.hpp"

#include <thread>

#include <CrispCore/Math/Constants.hpp>

#include <Core/LuaConfig.hpp>
#include "Core/EventHub.hpp"

#include "Geometry/VertexLayout.hpp"

#include "Renderer/VulkanBufferUtils.hpp"
#include "Renderer/VulkanImageUtils.hpp"

namespace crisp
{
    namespace
    {
        glm::vec3 lightWorldPos = 300.0f * glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f));

        float planeTilt = 0.0f;
        float angle = glm::radians(90.0f);
        glm::vec4 pointLightPos = glm::vec4(5.0f * std::cosf(angle), 3.0f, 5.0f * std::sinf(angle), 1.0f);
        bool animate = false;

        static constexpr uint32_t ShadowMapSize = 2048;

        static constexpr uint32_t NumObjects  = 2;
        static constexpr uint32_t NumLights   = 1;
        static constexpr uint32_t NumCascades = 4;

        static constexpr const char* MainPass = "mainPass";
        static constexpr const char* CsmPass  = "csmPass";
    }

    ShadowMappingScene::ShadowMappingScene(Renderer* renderer, Application* app)
        : m_renderer(renderer)
        , m_app(app)
    {
        setupInput();

        // Camera
        m_cameraController = std::make_unique<CameraController>(app->getWindow());
        m_cameraBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);

        m_renderGraph = std::make_unique<RenderGraph>(m_renderer);

        // Main render pass
        auto& mainPassNode = m_renderGraph->addRenderPass(MainPass, std::make_unique<SceneRenderPass>(m_renderer));
        m_renderGraph->addRenderTargetLayoutTransition(MainPass, "SCREEN", 0);
        m_renderer->setSceneImageView(mainPassNode.renderPass.get(), 0);

        // Shadow map pass
        auto& csmPassNode = m_renderGraph->addRenderPass(CsmPass, std::make_unique<ShadowPass>(m_renderer, ShadowMapSize, NumCascades));
        m_renderGraph->addRenderTargetLayoutTransition(CsmPass, MainPass, 0);

        m_renderGraph->sortRenderPasses();

        DirectionalLight light(glm::normalize(glm::vec3(-1.0f, -1.0f, -1.0f)), glm::vec3(10.0f, 10.0f, 10.0f), glm::vec3(-30.0f, -30.0f, -50.0f), glm::vec3(30.0f, 30.0f, 50.0f));
        m_cascadedShadowMapper = std::make_unique<CascadedShadowMapper>(m_renderer, NumCascades, light, 80.0f, ShadowMapSize);

        m_lights.resize(NumLights);
        m_lights[0] = light.createDescriptorData();
        m_lightBuffer = std::make_unique<UniformBuffer>(m_renderer, m_lights.size() * sizeof(LightDescriptorData), BufferUpdatePolicy::PerFrame);

        // Object transforms
        m_transforms.resize(NumObjects + 24);

        // Plane transform
        m_transforms[0].M = glm::rotate(glm::radians(planeTilt), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::scale(glm::vec3(100.0f, 1.0f, 100.0f));

        // Shader Ball transform
        for (int i = 0; i < 25; i++)
        {
            int d = i % 5;
            int v = i / 5;
            m_transforms[1 + i].M = glm::translate(glm::vec3(d * 10.0f, 2.5f, v * 10.0f)) * glm::scale(glm::vec3(0.05f)) * glm::rotate(glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        }
        m_transforms.push_back(TransformPack());

        m_transformBuffer = std::make_unique<UniformBuffer>(m_renderer, m_transforms.size() * sizeof(TransformPack), BufferUpdatePolicy::PerFrame);

        // Geometry setup
        std::vector<VertexAttributeDescriptor> shadowVertexFormat = { VertexAttribute::Position };
        std::vector<VertexAttributeDescriptor> pbrVertexFormat    = { VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord, VertexAttribute::Tangent, VertexAttribute::Bitangent };

        // Plane geometry
        m_geometries.emplace_back(std::make_unique<Geometry>(m_renderer, createPlaneMesh(pbrVertexFormat)));

        // Shaderball geometry
        m_geometries.emplace_back(std::make_unique<Geometry>(m_renderer, m_renderer->getResourcesPath() / "Meshes/ShaderBall_FWVN_PosX.obj", pbrVertexFormat));
        m_geometries.emplace_back(std::make_unique<Geometry>(m_renderer, m_renderer->getResourcesPath() / "Meshes/ShaderBall_FWVN_PosX.obj", shadowVertexFormat));
        m_geometries.emplace_back(std::make_unique<Geometry>(m_renderer, createSphereMesh(pbrVertexFormat)));
        m_geometries.emplace_back(std::make_unique<Geometry>(m_renderer, createSphereMesh(shadowVertexFormat)));
        m_geometries.emplace_back(std::make_unique<Geometry>(m_renderer, m_renderer->getResourcesPath() / "Meshes/nanosuit/nanosuit.obj", pbrVertexFormat));
        m_geometries.emplace_back(std::make_unique<Geometry>(m_renderer, m_renderer->getResourcesPath() / "Meshes/cube.obj", shadowVertexFormat));

        // Shadow map material setup
        auto csmPipeline = createShadowMapPipeline(m_renderer, csmPassNode.renderPass.get(), 0);
        auto shadowMapMaterial = std::make_unique<Material>(csmPipeline.get());
        shadowMapMaterial->writeDescriptor(0, 0, 0, m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        shadowMapMaterial->writeDescriptor(0, 1, 0, m_cascadedShadowMapper->getLightTransformBuffer()->getDescriptorInfo());
        shadowMapMaterial->addDynamicBufferInfo(*m_cascadedShadowMapper->getLightTransformBuffer(), 0);
        m_pipelines.push_back(std::move(csmPipeline));
        m_materials.push_back(std::move(shadowMapMaterial));

        // Common material components
        createDefaultPbrTextures();
        m_linearClampSampler  = std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 16.0f, 11.0f);
        m_linearRepeatSampler = std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT, 16.0f, 12.0f);
        m_nearestNeighborSampler = std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        m_mipmapSampler = std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 16.0f, 5.0f);

        LuaConfig config(m_renderer->getResourcesPath() / "Scripts/scene.lua");

        // Environment map
        auto hdrName = config.get<std::string>("environmentMap").value_or("NewportLoft_Ref") + ".hdr";
        m_envRefMap = createEnvironmentMap(m_renderer, hdrName, VK_FORMAT_R32G32B32A32_SFLOAT, true);
        m_envRefMapView = m_envRefMap->createView(VK_IMAGE_VIEW_TYPE_2D);

        auto [cubeMap, cubeMapView] = convertEquirectToCubeMap(m_envRefMapView);
        setupDiffuseEnvMap(*cubeMapView);
        setupReflectEnvMap(*cubeMapView);

        m_brdfLut = integrateBrdfLut();// createTexture(m_renderer, "ibl_brdf_lut.png", VK_FORMAT_R8G8B8A8_UNORM, true);
        m_brdfLutView = m_brdfLut->createView(VK_IMAGE_VIEW_TYPE_2D);

        // Physically-based material setup

        auto texFolder = config.get<std::string>("texture").value_or("RustedIron");
        auto physBasedPipeline = createPbrPipeline(m_renderer, mainPassNode.renderPass.get(), 3);
        m_materials.push_back(createPbrMaterial(texFolder, physBasedPipeline.get()));
        m_materials.push_back(createPbrMaterial("Limestone", physBasedPipeline.get()));
        m_pipelines.push_back(std::move(physBasedPipeline));
        m_activeMaterial = m_materials[1].get();

        m_pbrUnifMaterial.albedo = glm::vec3(0.0f, 0.0f, 0.0f);
        m_pbrUnifMaterial.metallic = 0.0f;
        m_pbrUnifMaterial.roughness = 0.0f;
        m_pbrUnifMatBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(m_pbrUnifMaterial), BufferUpdatePolicy::PerFrame, &m_pbrUnifMaterial);
        auto pbrUnifPipeline = createPbrUnifPipeline(m_renderer, mainPassNode.renderPass.get());
        m_materials.push_back(createUnifPbrMaterial(pbrUnifPipeline.get()));
        m_pipelines.push_back(std::move(pbrUnifPipeline));

        m_boxVisualizer = std::make_unique<BoxVisualizer>(m_renderer, 4, 4, *mainPassNode.renderPass);

        //m_irradiancePipeline = createIrradiancePipeline(m_renderer, mainPassNode.renderPass.get());
        //auto irradianceMaterial = std::make_unique<Material>(m_irradiancePipeline.get(), std::vector<uint32_t>{0});
        //irradianceMaterial->writeDescriptor(0, 0, 0, m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        //irradianceMaterial->writeDescriptor(0, 1, 0, *m_envMapView, m_linearRepeatSampler->getHandle());
        //m_materials.push_back(std::move(irradianceMaterial));

        //m_physBasedUnifPipeline = createPbrUnifPipeline(m_renderer, mainPassNode.renderPass.get());
        //auto pbrUnifMaterial = std::make_unique<Material>(m_physBasedUnifPipeline.get(), std::vector<uint32_t>{0});
        //pbrUnifMaterial->writeDescriptor(0, 0, 0, m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        //pbrUnifMaterial->writeDescriptor(0, 1, 0, m_cameraBuffer->getDescriptorInfo());
        //pbrUnifMaterial->addDynamicBufferInfo(*m_cameraBuffer, 0);
        //m_materials.push_back(std::move(pbrUnifMaterial));

        //m_skybox = std::make_unique<Skybox>(m_renderer, *mainPassNode.renderPass, "Forest");

        m_renderer->getDevice()->flushDescriptorUpdates();

        m_app->getForm()->add(gui::createShadowMappingSceneGui(m_app->getForm(), this));
    }

    ShadowMappingScene::~ShadowMappingScene()
    {
        m_app->getForm()->remove("shadowMappingPanel");
    }

    void ShadowMappingScene::resize(int width, int height)
    {
        m_cameraController->resize(width, height);

        m_renderGraph->resize(width, height);
        m_renderer->setSceneImageView(m_renderGraph->getNode("mainPass").renderPass.get(), 0);
    }

    void ShadowMappingScene::update(float dt)
    {
        static float t = 0;

        //if (animate)
        //{
        //    angle += 2.0f * PI * dt / 5.0f;
        //    pointLightPos.x = 5.0f * std::cosf(angle);
        //    pointLightPos.z = 5.0f * std::sinf(angle);
        //    m_transforms[3].M = glm::translate(glm::vec3(pointLightPos)) * glm::scale(glm::vec3(0.2f));
        //}

        m_cameraController->update(dt);
        const auto& V = m_cameraController->getViewMatrix();
        const auto& P = m_cameraController->getProjectionMatrix();

        for (auto& trans : m_transforms)
        {
            trans.MV  = V * trans.M;
            trans.MVP = P * trans.MV;
            trans.N = glm::transpose(glm::inverse(glm::mat3(trans.MV)));
        }

        m_transformBuffer->updateStagingBuffer(m_transforms.data(), m_transforms.size() * sizeof(TransformPack));
        m_cameraBuffer->updateStagingBuffer(m_cameraController->getCameraParameters(), sizeof(CameraParameters));


        //auto screen = m_renderer->getVulkanSwapChainExtent();
        //
        //glm::vec3 pt = glm::project(lightWorldPos, V, P, glm::vec4(0.0f, 0.0f, screen.width, screen.height));
        //pt.x /= screen.width;
        //pt.y /= screen.height;
        //m_lightShaftParams.lightPos = glm::vec2(pt);
        //m_lightShaftBuffer->updateStagingBuffer(m_lightShaftParams);

        m_cascadedShadowMapper->setZ(2.0f * PI * t / 5.0f);
        m_cascadedShadowMapper->recalculateLightProjections(m_cameraController->getCamera());

        m_lightBuffer->updateStagingBuffer(m_lights.data(), m_lights.size() * sizeof(LightDescriptorData));

        m_pbrUnifMatBuffer->updateStagingBuffer(m_pbrUnifMaterial);

        //m_skybox->updateTransforms(V, P);

        m_boxVisualizer->updateFrusta(m_cascadedShadowMapper.get(), m_cameraController.get());

        t += dt;
    }

    void ShadowMappingScene::render()
    {
        m_renderer->enqueueResourceUpdate([this](VkCommandBuffer cmdBuffer)
            {
                uint32_t frameIdx = m_renderer->getCurrentVirtualFrameIndex();
                m_boxVisualizer->updateDeviceBuffers(cmdBuffer, frameIdx);
            });

        m_renderGraph->clearCommandLists();

        auto shadowCommand = [this](Material* material, Geometry* geometry, int transformIndex, int cascadeIndex)
        {
            VkOffset2D offsets[4] = { {0, 0}, {1, 0}, {0, 1}, {1, 1} };
            DrawCommand drawCommand;
            drawCommand.viewport = { static_cast<float>(offsets[cascadeIndex].x * ShadowMapSize), static_cast<float>(offsets[cascadeIndex].y * ShadowMapSize), ShadowMapSize, ShadowMapSize, 0.0f, 1.0f };
            drawCommand.scissor.offset = { static_cast<int32_t>(offsets[cascadeIndex].x * ShadowMapSize), static_cast<int32_t>(offsets[cascadeIndex].y * ShadowMapSize) };
            drawCommand.scissor.extent = { ShadowMapSize, ShadowMapSize };
            drawCommand.pipeline = material->getPipeline();
            drawCommand.material = material;
            drawCommand.dynamicBuffers.push_back({ *m_transformBuffer, transformIndex * sizeof(TransformPack) });
            for (const auto& info : material->getDynamicBufferInfos())
                drawCommand.dynamicBuffers.push_back(info);
            drawCommand.setPushConstants(cascadeIndex);
            drawCommand.geometry = geometry;
            drawCommand.setGeometryView(geometry->createIndexedGeometryView());
            return drawCommand;
        };

        auto shadeCommand = [this](Material* material, Geometry* geometry, int transformIndex)
        {
            DrawCommand drawCommand;
            drawCommand.pipeline = material->getPipeline();
            drawCommand.material = material;
            drawCommand.dynamicBuffers.push_back({ *m_transformBuffer, transformIndex * sizeof(TransformPack) });
            for (const auto& info : material->getDynamicBufferInfos())
                drawCommand.dynamicBuffers.push_back(info);
            drawCommand.geometry = geometry;
            drawCommand.setGeometryView(geometry->createIndexedGeometryView());
            return drawCommand;
        };

        auto pbrUnifCommand = [this](Material* material, Geometry* geometry, int transformIndex)
        {
            DrawCommand drawCommand;
            drawCommand.pipeline = material->getPipeline();
            drawCommand.material = material;
            drawCommand.dynamicBuffers.push_back({ *m_transformBuffer, transformIndex * sizeof(TransformPack) });
            for (const auto& info : material->getDynamicBufferInfos())
                drawCommand.dynamicBuffers.push_back(info);
            drawCommand.geometry = geometry;
            drawCommand.setGeometryView(geometry->createIndexedGeometryView());
            return drawCommand;
        };

        static constexpr uint32_t Shadow = 0;
        static constexpr uint32_t Pbr = 1;

        auto& shadowPass = m_renderGraph->getNode("csmPass");

        for (int c = 0; c < 4; c++)
            shadowPass.addCommand(shadowCommand(m_materials[Shadow].get(), m_geometries[2].get(), 1, c));
            //for (int i = 1; i <= 25; i++)
            //    shadowPass.addCommand(shadowCommand(m_materials[Shadow].get(), m_geometries[2].get(), i, c));

        auto& mainPass = m_renderGraph->getNode("mainPass");

        mainPass.addCommand(pbrUnifCommand(m_materials[1].get(), m_geometries[1].get(), 1));
        mainPass.addCommand(pbrUnifCommand(m_materials[1].get(), m_geometries[0].get(), 0));

        //mainPass.addCommand(shadeCommand(m_materials[Pbr].get(), m_geometries[0].get(), 0));
        //
        //for (int i = 1; i <= 25; i++)
        //    mainPass.addCommand(shadeCommand(m_materials[Pbr].get(), m_geometries[1].get(), i));
        //
        //
        //auto v = m_boxVisualizer->createDrawCommands();
        //for (auto& dc : v)
        //    mainPass.addCommand(std::move(dc));

        m_renderGraph->executeDrawCommands();
    }

    void ShadowMappingScene::setBlurRadius(int /*radius*/)
    {
        //m_blurParameters.radius = radius;
        //m_blurParameters.sigma = radius == 0 ? 1.0f : static_cast<float>(radius) / 3.0f;
    }

    void ShadowMappingScene::setSplitLambda(double lambda)
    {
        m_cascadedShadowMapper->setSplitLambda(static_cast<float>(lambda), m_cameraController->getCamera());
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

    void ShadowMappingScene::onMaterialSelected(const std::string& material)
    {
        //Material* modified;
        //if (m_activeMaterial == m_materials[1].get())
        //    modified = m_materials[2].get();
        //else
        //    modified = m_materials[1].get();
        //
        //modified->writeDescriptor(0, 0, 0, m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        //modified->writeDescriptor(0, 1, 0, m_cameraBuffer->getDescriptorInfo());
        //
        //const std::vector<std::string> texNames = { "diffuse", "metallic", "roughness", "normal", "ao" };
        //const std::vector<VkFormat> formats = { VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM };
        //for (uint32_t i = 0; i < texNames.size(); ++i)
        //{
        //    const std::string& filename = "PbrMaterials/" + material + "/" + texNames[i] + ".png";
        //    const auto& path = m_renderer->getResourcesPath() / "Textures" / filename;
        //    if (std::filesystem::exists(path))
        //    {
        //        m_images.emplace_back(createTexture(m_renderer, filename, formats[i], true));
        //        m_imageViews.emplace_back(m_images.back()->createView(VK_IMAGE_VIEW_TYPE_2D));
        //        modified->writeDescriptor(0, 2 + i, 0, *m_imageViews.back(), m_linearRepeatSampler->getHandle());
        //    }
        //    else
        //    {
        //        logWarning("Texture type {} is using default values for '{}'\n", material, texNames[i]);
        //        modified->writeDescriptor(0, 2 + i, 0, *m_imageViews[i], m_linearRepeatSampler->getHandle());
        //    }
        //}
        //
        //modified->writeDescriptor(0, 7, 0, *m_envMapView, m_linearRepeatSampler->getHandle());
        //
        //modified->writeDescriptor(1, 0, 0, m_lightBuffer->getDescriptorInfo(0, sizeof(LightDescriptorData)));
        //for (uint32_t i = 0; i < Renderer::NumVirtualFrames; ++i)
        //    modified->writeDescriptor(1, 1, i, m_renderGraph->getNode("csmPass").renderPass->getRenderTargetView(0, i), m_linearClampSampler->getHandle());
        //m_renderer->getDevice()->flushDescriptorUpdates();
        //m_activeMaterial = modified;
    }

    void ShadowMappingScene::createDefaultPbrTextures()
    {
        m_images.emplace_back(createTexture(m_renderer, 1, 1, { 255, 0, 255, 255 }, VK_FORMAT_R8G8B8A8_SRGB));
        m_defaultTexImageViews.emplace("diffuse", m_images.back()->createView(VK_IMAGE_VIEW_TYPE_2D));

        m_images.emplace_back(createTexture(m_renderer, 1, 1, { 0, 0, 0, 255 }, VK_FORMAT_R8G8B8A8_UNORM));
        m_defaultTexImageViews.emplace("metallic", m_images.back()->createView(VK_IMAGE_VIEW_TYPE_2D));

        m_images.emplace_back(createTexture(m_renderer, 1, 1, { 32, 32, 32, 255 }, VK_FORMAT_R8G8B8A8_UNORM));
        m_defaultTexImageViews.emplace("roughness", m_images.back()->createView(VK_IMAGE_VIEW_TYPE_2D));

        m_images.emplace_back(createTexture(m_renderer, 1, 1, { 127, 127, 255, 255 }, VK_FORMAT_R8G8B8A8_UNORM));
        m_defaultTexImageViews.emplace("normal", m_images.back()->createView(VK_IMAGE_VIEW_TYPE_2D));

        m_images.emplace_back(createTexture(m_renderer, 1, 1, { 255, 255, 255, 255 }, VK_FORMAT_R8G8B8A8_UNORM));
        m_defaultTexImageViews.emplace("ao", m_images.back()->createView(VK_IMAGE_VIEW_TYPE_2D));
    }

    std::unique_ptr<Material> ShadowMappingScene::createPbrMaterial(const std::string& type, VulkanPipeline* pipeline)
    {
        auto material = std::make_unique<Material>(pipeline);
        material->writeDescriptor(0, 0, 0, m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        material->writeDescriptor(0, 1, 0, m_cameraBuffer->getDescriptorInfo());
        material->addDynamicBufferInfo(*m_cameraBuffer, 0);
        material->addDynamicBufferInfo(*m_lightBuffer, 0);
        material->addDynamicBufferInfo(*m_cascadedShadowMapper->getSplitBuffer(), 0);

        const std::vector<std::string> texNames = { "diffuse", "metallic", "roughness", "normal", "ao" };
        const std::vector<VkFormat> formats = { VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM };
        for (uint32_t i = 0; i < texNames.size(); ++i)
        {
            const std::string& filename = "PbrMaterials/" + type + "/" + texNames[i] + ".png";
            const auto& path = m_renderer->getResourcesPath() / "Textures" / filename;
            if (std::filesystem::exists(path))
            {
                m_images.emplace_back(createTexture(m_renderer, filename, formats[i], true));
                m_imageViews.emplace_back(m_images.back()->createView(VK_IMAGE_VIEW_TYPE_2D));
                material->writeDescriptor(0, 2 + i, 0, *m_imageViews.back(), m_linearRepeatSampler->getHandle());
            }
            else
            {
                logWarning("Texture type {} is using default values for '{}'\n", type, texNames[i]);
                material->writeDescriptor(0, 2 + i, 0, *m_defaultTexImageViews.at(texNames[i]), m_linearRepeatSampler->getHandle());
            }
        }

        for (uint32_t i = 0; i < Renderer::NumVirtualFrames; ++i)
        {
            material->writeDescriptor(1, 0, i, m_lightBuffer->getDescriptorInfo(0, sizeof(LightDescriptorData)));
            material->writeDescriptor(1, 1, i, m_renderGraph->getNode("csmPass").renderPass->getRenderTargetView(0, i), m_nearestNeighborSampler->getHandle());
            material->writeDescriptor(1, 2, i, m_cascadedShadowMapper->getSplitBuffer()->getDescriptorInfo());
            material->writeDescriptor(1, 3, i, m_cascadedShadowMapper->getLightTransformBuffer()->getDescriptorInfo());
        }

        material->addDynamicBufferInfo(*m_cascadedShadowMapper->getLightTransformBuffer(), 0);

        material->writeDescriptor(2, 0, 0, *m_envIrrMapView, m_linearClampSampler->getHandle());
        material->writeDescriptor(2, 1, 0, *m_filteredMapView, m_mipmapSampler->getHandle());
        material->writeDescriptor(2, 2, 0, *m_brdfLutView, m_linearClampSampler->getHandle());

        return material;
    }

    std::unique_ptr<Material> ShadowMappingScene::createUnifPbrMaterial(VulkanPipeline* pipeline)
    {
        auto material = std::make_unique<Material>(pipeline);
        material->writeDescriptor(0, 0, 0, m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        material->writeDescriptor(0, 1, 0, m_cameraBuffer->getDescriptorInfo());
        material->writeDescriptor(0, 2, 0, m_pbrUnifMatBuffer->getDescriptorInfo());
        material->addDynamicBufferInfo(*m_cameraBuffer, 0);
        material->addDynamicBufferInfo(*m_pbrUnifMatBuffer, 0);

        material->writeDescriptor(1, 0, 0, m_lightBuffer->getDescriptorInfo(0, sizeof(LightDescriptorData)));
        material->addDynamicBufferInfo(*m_lightBuffer, 0);

        material->writeDescriptor(2, 0, 0, *m_envIrrMapView, m_linearClampSampler->getHandle());
        material->writeDescriptor(2, 1, 0, *m_filteredMapView, m_mipmapSampler->getHandle());
        material->writeDescriptor(2, 2, 0, *m_brdfLutView, m_linearClampSampler->getHandle());

        return material;
    }

    void ShadowMappingScene::setupInput()
    {
        m_app->getWindow()->getEventHub().keyPressed += [this](Key key, int)
        {
            if (key == Key::Space)
                animate = !animate;

            if (key == Key::Up)
            {
                planeTilt += 15.0f;
                m_transforms[0].M = glm::rotate(glm::radians(planeTilt), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::scale(glm::vec3(10.0f, 1.0f, 10.0f));
            }

            if (key == Key::Down)
            {
                planeTilt -= 15.0f;
                m_transforms[0].M = glm::rotate(glm::radians(planeTilt), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::scale(glm::vec3(10.0f, 1.0f, 10.0f));
            }
        };

        m_app->getWindow()->getEventHub().mouseWheelScrolled += [this](double delta)
        {
            float fov = m_cameraController->getCamera().getFov();
            if (delta < 0)
                fov = std::min(90.0f, fov + 5.0f);
            else
                fov = std::max(5.0f, fov - 5.0f);
            m_cameraController->getCamera().setFov(fov);
        };
    }

    std::pair<std::unique_ptr<VulkanImage>, std::unique_ptr<VulkanImageView>> ShadowMappingScene::convertEquirectToCubeMap(std::shared_ptr<VulkanImageView> equirectMapView)
    {
        static constexpr uint32_t CubeMapFaceCount = 6;

        auto cubeMapPass = std::make_unique<CubeMapRenderPass>(m_renderer, VkExtent2D{ 512, 512 });
        std::vector<std::unique_ptr<VulkanPipeline>> cubeMapPipelines(CubeMapFaceCount);
        for (int i = 0; i < CubeMapFaceCount; i++)
            cubeMapPipelines[i] = createEquirectToCubeMapPipeline(m_renderer, cubeMapPass.get(), i);

        auto cubeMapMaterial = std::make_unique<Material>(cubeMapPipelines[0].get());
        cubeMapMaterial->writeDescriptor(0, 0, 0, *equirectMapView, m_linearRepeatSampler->getHandle());
        m_renderer->getDevice()->flushDescriptorUpdates();

        m_renderer->enqueueResourceUpdate([this, &cubeMapPipelines, &cubeMapPass, &cubeMapMaterial](VkCommandBuffer cmdBuffer)
        {
            glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
            glm::mat4 captureViews[] =
            {
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f),  glm::vec3(0.0f, -1.0f,  0.0f)),
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f),  glm::vec3(0.0f,  0.0f,  1.0f)),
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f),  glm::vec3(0.0f,  0.0f, -1.0f)),
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f),  glm::vec3(0.0f, -1.0f,  0.0f)),
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f),  glm::vec3(0.0f, -1.0f,  0.0f))
            };

            VkViewport viewport = { 0.0f, 0.0f, 512.0f, 512.0f, 0.0f, 1.0f };
            VkRect2D scissor = { { 0, 0  }, { 512, 512 } };


            cubeMapPass->begin(cmdBuffer, 0);

            for (int i = 0; i < 6; i++)
            {
                glm::mat4 MVP = captureProjection * captureViews[i];
                std::vector<char> pushConst(sizeof(glm::mat4));
                memcpy(pushConst.data(), glm::value_ptr(MVP), sizeof(glm::mat4));

                cubeMapPipelines[i]->bind(cmdBuffer);
                vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
                vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
                cubeMapPipelines[i]->getPipelineLayout()->setPushConstants(cmdBuffer, pushConst.data());

                cubeMapMaterial->bind(0, cmdBuffer);
                m_geometries[6]->bindAndDraw(cmdBuffer);

                if (i < 5)
                    cubeMapPass->nextSubpass(cmdBuffer);
            }

            cubeMapPass->end(cmdBuffer, 0);
            cubeMapPass->getRenderTarget(0)->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        });
        m_renderer->flushResourceUpdates();
        m_renderer->finish();

        auto cubeMap = cubeMapPass->extractRenderTarget(0);
        auto cubeMapView = cubeMap->createView(VK_IMAGE_VIEW_TYPE_CUBE, 0, 6);
        return std::make_pair(std::move(cubeMap), std::move(cubeMapView));
    }

    void ShadowMappingScene::setupDiffuseEnvMap(const VulkanImageView& cubeMapView)
    {
        static constexpr uint32_t CubeMapFaceCount = 6;

        auto convPass = std::make_shared<CubeMapRenderPass>(m_renderer, VkExtent2D{ 512, 512 });
        std::vector<std::unique_ptr<VulkanPipeline>> convPipelines(6);
        for (int i = 0; i < 6; i++)
            convPipelines[i] = createConvolvePipeline(m_renderer, convPass.get(), i);

        auto convMaterial = std::make_unique<Material>(convPipelines[0].get());
        convMaterial->writeDescriptor(0, 0, 0, cubeMapView, m_linearRepeatSampler->getHandle());
        m_renderer->getDevice()->flushDescriptorUpdates();

        m_renderer->enqueueResourceUpdate([this, &convPipelines, &convPass, &convMaterial](VkCommandBuffer cmdBuffer)
        {
            glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
            glm::mat4 captureViews[] =
            {
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
            };

            VkViewport viewport = convPass->createViewport();
            VkRect2D scissor    = convPass->createScissor();

            convPass->begin(cmdBuffer, 0);

            for (int i = 0; i < 6; i++)
            {
                glm::mat4 MVP = captureProjection * captureViews[i];
                std::vector<char> pushConst(sizeof(glm::mat4));
                memcpy(pushConst.data(), glm::value_ptr(MVP), sizeof(glm::mat4));

                convPipelines[i]->bind(cmdBuffer);
                vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
                vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
                convPipelines[i]->getPipelineLayout()->setPushConstants(cmdBuffer, pushConst.data());

                convMaterial->bind(0, cmdBuffer);
                m_geometries[6]->bindAndDraw(cmdBuffer);

                if (i < 5)
                    convPass->nextSubpass(cmdBuffer);
            }

            convPass->end(cmdBuffer, 0);
            convPass->getRenderTarget(0)->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });
        m_renderer->flushResourceUpdates();
        m_renderer->finish();

        m_envIrrMap = convPass->extractRenderTarget(0);
        m_envIrrMapView = m_envIrrMap->createView(VK_IMAGE_VIEW_TYPE_CUBE, 0, 6);
    }

    void ShadowMappingScene::setupReflectEnvMap(const VulkanImageView& cubeMapView)
    {
        uint32_t mipLevels = 5;
        m_filteredMap = createMipmapCubeMap(m_renderer, 512, 512, mipLevels);
        for (int i = 0; i < mipLevels; i++)
        {
            float roughness = i / static_cast<float>(mipLevels - 1);

            unsigned int w = 512 * std::pow(0.5, i);
            unsigned int h = 512 * std::pow(0.5, i);
            std::shared_ptr<CubeMapRenderPass> prefilterPass = std::make_unique<CubeMapRenderPass>(m_renderer, VkExtent2D{ w, h });

            std::vector<std::unique_ptr<VulkanPipeline>> filterPipelines(6);
            for (int j = 0; j < 6; j++)
                filterPipelines[j] = createPrefilterPipeline(m_renderer, prefilterPass.get(), j);

            std::shared_ptr filterMat = std::make_unique<Material>(filterPipelines[0].get());
            filterMat->writeDescriptor(0, 0, 0, cubeMapView, m_linearRepeatSampler->getHandle());
            m_renderer->getDevice()->flushDescriptorUpdates();

            m_renderer->enqueueResourceUpdate([this, &filterPipelines, prefilterPass, filterMat, i, w, h, roughness](VkCommandBuffer cmdBuffer)
            {
                glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
                glm::mat4 captureViews[] =
                {
                    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
                    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
                    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
                    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
                    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
                    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
                };

                VkViewport viewport = prefilterPass->createViewport();
                VkRect2D scissor    = prefilterPass->createScissor();

                prefilterPass->begin(cmdBuffer, 0);

                for (int j = 0; j < 6; j++)
                {
                    glm::mat4 MVP = captureProjection * captureViews[j];
                    std::vector<char> pushConst(sizeof(glm::mat4) + sizeof(float));
                    memcpy(pushConst.data(), glm::value_ptr(MVP), sizeof(glm::mat4));
                    memcpy(pushConst.data() + sizeof(glm::mat4), &roughness, sizeof(float));

                    filterPipelines[j]->bind(cmdBuffer);
                    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
                    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
                    filterPipelines[j]->getPipelineLayout()->setPushConstants(cmdBuffer, pushConst.data());

                    filterMat->bind(0, cmdBuffer);
                    m_geometries[6]->bindAndDraw(cmdBuffer);

                    if (j < 5)
                        prefilterPass->nextSubpass(cmdBuffer);
                }

                prefilterPass->end(cmdBuffer, 0);
                prefilterPass->getRenderTarget(0)->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
                prefilterPass->getRenderTarget(0)->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
                m_renderer->finish();
            });
            m_renderer->flushResourceUpdates();
            std::shared_ptr<VulkanImage> rt = prefilterPass->extractRenderTarget(0);

            m_renderer->enqueueResourceUpdate([this, rt, i](VkCommandBuffer cmdBuffer)
            {
                m_filteredMap->blit(cmdBuffer, *rt, i);

            });
            m_renderer->flushResourceUpdates();
            m_renderer->finish();
        }

        m_filteredMapView = m_filteredMap->createView(VK_IMAGE_VIEW_TYPE_CUBE, 0, 6, 0, 5);
    }

    std::unique_ptr<VulkanImage> ShadowMappingScene::integrateBrdfLut()
    {
        auto texPass = std::make_shared<TexturePass>(m_renderer, VkExtent2D{ 1024, 1024 });
        std::shared_ptr<VulkanPipeline> pipeline = createBrdfLutPipeline(m_renderer, texPass.get());

        auto material = std::make_shared<Material>(pipeline.get());
        m_renderer->getDevice()->flushDescriptorUpdates();

        m_renderer->enqueueResourceUpdate([this, pipeline, texPass, material](VkCommandBuffer cmdBuffer)
        {
            VkViewport viewport = texPass->createViewport();
            VkRect2D scissor = texPass->createScissor();

            texPass->begin(cmdBuffer, 0);

            pipeline->bind(cmdBuffer);
            vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
            vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
            //pipeline->getPipelineLayout()->setPushConstants(cmdBuffer, pushConst.data());

            //material->bind(0, cmdBuffer);
            m_renderer->drawFullScreenQuad(cmdBuffer);

            texPass->end(cmdBuffer, 0);
            texPass->getRenderTarget(0)->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });
        m_renderer->flushResourceUpdates();
        m_renderer->finish();

        return texPass->extractRenderTarget(0);
    }
}