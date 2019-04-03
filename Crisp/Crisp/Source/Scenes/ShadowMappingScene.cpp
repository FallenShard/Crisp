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
#include "Renderer/Renderer.hpp"
#include "Renderer/UniformBuffer.hpp"
#include "vulkan/VulkanSampler.hpp"
#include "Renderer/Texture.hpp"
#include "Renderer/Material.hpp"

#include "Renderer/Pipelines/LightShaftPipeline.hpp"

#include "Renderer/RenderPasses/VarianceShadowMapPass.hpp"
#include "Renderer/RenderPasses/EnvironmentRenderPass.hpp"

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

        struct Params
        {
            glm::vec4 lightPos;
            float roughness = 0.1f;
            float metallic = 0.0f;
        };

        Params params;

        static constexpr uint32_t ShadowMapSize = 2048;

        static constexpr uint32_t NumObjects = 2;
        static constexpr uint32_t NumLights = 1;
        static constexpr uint32_t NumCascades = 4;

        static constexpr const char* MainPass = "mainPass";
        static constexpr const char* CsmPass = "csmPass";
    }

    ShadowMappingScene::ShadowMappingScene(Renderer* renderer, Application* app)
        : m_renderer(renderer)
        , m_app(app)
    {
        setupControls();

        // Camera
        m_cameraController = std::make_unique<CameraController>(app->getWindow());
        m_cameraBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);

        m_renderGraph = std::make_unique<RenderGraph>(m_renderer);

        // Main render pass
        auto& mainPassNode = m_renderGraph->addRenderPass(MainPass, std::make_unique<SceneRenderPass>(m_renderer));
        m_renderGraph->addRenderTargetLayoutTransition(MainPass, "SCREEN", 0);
        m_renderer->setSceneImageView(mainPassNode.renderPass->createRenderTargetView(0, Renderer::NumVirtualFrames));

        // Shadow map pass
        auto& csmPassNode = m_renderGraph->addRenderPass(CsmPass, std::make_unique<ShadowPass>(m_renderer, ShadowMapSize, NumCascades));
        m_renderGraph->addDependency(CsmPass, MainPass, [](const VulkanRenderPass& pass, VkCommandBuffer cmdBuffer, uint32_t frameIndex)
        {
            pass.getRenderTarget(0)->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, frameIndex, 1,
                VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });

        m_renderGraph->sortRenderPasses();

        DirectionalLight light(glm::normalize(glm::vec3(-1.0f, -1.0f, 0.0f)), glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(-30.0f, -30.0f, -50.0f), glm::vec3(30.0f, 30.0f, 50.0f));
        m_cascadedShadowMapper = std::make_unique<CascadedShadowMapper>(m_renderer, NumCascades, light, 80.0f);

        m_lights.resize(NumLights);
        m_lights[0] = light.createDescriptorData();
        m_lightBuffer = std::make_unique<UniformBuffer>(m_renderer, m_lights.size() * sizeof(LightDescriptorData), BufferUpdatePolicy::PerFrame);

        // Object transforms
        m_transforms.resize(NumObjects + 24);

        // Plane transform
        m_transforms[0].M = glm::rotate(glm::radians(planeTilt), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::scale(glm::vec3(100.0f, 1.0f, 100.0f));

        // Shader Ball transform
        m_transforms[1].M = glm::scale(glm::vec3(0.05f)) * glm::rotate(glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        for (int i = 1; i < 25; i++)
        {
            int d = i % 5;
            int v = i / 5;
            m_transforms[1 + i].M = glm::translate(glm::vec3(d * 10.0f, 0.0f, v * 10.0f)) * glm::scale(glm::vec3(0.05f)) * glm::rotate(glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        }
        m_transformBuffer = std::make_unique<UniformBuffer>(m_renderer, m_transforms.size() * sizeof(TransformPack), BufferUpdatePolicy::PerFrame);

        // Geometry setup
        auto shadowVertexFormat = { VertexAttribute::Position };
        auto pbrVertexFormat    = { VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord, VertexAttribute::Tangent, VertexAttribute::Bitangent };

        // Plane geometry
        m_geometries.emplace_back(createGeometry(m_renderer, createPlaneMesh(pbrVertexFormat)));

        // Shaderball geometry
        m_geometries.emplace_back(createGeometry(m_renderer, m_renderer->getResourcesPath() / "Meshes/ShaderBall_FWVN_PosX.obj", pbrVertexFormat));
        m_geometries.emplace_back(createGeometry(m_renderer, m_renderer->getResourcesPath() / "Meshes/ShaderBall_FWVN_PosX.obj", shadowVertexFormat));

        // Shadow map material setup
        m_csmPipeline = createShadowMapPipeline(m_renderer, csmPassNode.renderPass.get(), 0);
        auto shadowMapMaterial = std::make_unique<Material>(m_csmPipeline.get(), std::vector<uint32_t>{ 0 });
        shadowMapMaterial->writeDescriptor(0, 0, 0, m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        shadowMapMaterial->writeDescriptor(0, 1, 0, m_cascadedShadowMapper->getLightTransformBuffer()->getDescriptorInfo());
        shadowMapMaterial->addDynamicBufferInfo(*m_cascadedShadowMapper->getLightTransformBuffer(), 0);
        m_materials.push_back(std::move(shadowMapMaterial));

        // Common material components
        createDefaultPbrTextures();
        m_linearClampSampler  = std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 16.0f, 11.0f);
        m_linearRepeatSampler = std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT, 16.0f, 12.0f);

        // Environment map
        m_envMap = createEnvironmentMap(m_renderer, "NewportLoft.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, true);
        m_envMapView = m_envMap->createView(VK_IMAGE_VIEW_TYPE_2D);

        // Physically-based material setup
        LuaConfig config(m_renderer->getResourcesPath() / "Scripts/scene.lua");
        auto texFolder = config.get<std::string>("texture").value_or("RustedIron");
        m_physBasedPipeline = createPbrPipeline(m_renderer, mainPassNode.renderPass.get(), 3);
        m_materials.push_back(createPbrMaterial(texFolder));
        m_materials.push_back(createPbrMaterial("Limestone"));
        m_activeMaterial = m_materials[1].get();

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


        //auto envPass = std::make_shared<EnvironmentRenderPass>(m_renderer, VkExtent2D{ 512, 512 });
        //m_envMapPipeline = createEnvMapPipeline(m_renderer, envPass.get());
        //auto irradianceMaterial = std::make_unique<Material>(m_irradiancePipeline.get(), std::vector<uint32_t>{0});
        //irradianceMaterial->writeDescriptor(0, 0, 0, m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        //irradianceMaterial->writeDescriptor(0, 1, 0, *m_envMapView, m_linearRepeatSampler->getHandle());
        //auto envMat = std::make_shared<Material>(m_renderer,

        //m_renderer->enqueueResourceUpdate([this, envPass](VkCommandBuffer cmdBuffer)
        //{
        //    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
        //    glm::mat4 captureViews[] =
        //    {
        //       glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        //       glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        //       glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
        //       glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
        //       glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        //       glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
        //    };
        //
        //    envPass->begin(cmdBuffer);
        //
        //    for (int i = 0; i < 6; i++)
        //    {
        //        //DrawCommand drawCommand;
        //        //drawCommand.pipeline = material->getPipeline();
        //        //drawCommand.material = material;
        //        //drawCommand.dynamicBuffers.push_back({ *m_transformBuffer, transformIndex * sizeof(TransformPack) });
        //        //for (const auto& info : material->getDynamicBufferInfos())
        //        //    drawCommand.dynamicBuffers.push_back(info);
        //        //drawCommand.setPushConstants(pointLightPos);
        //        //drawCommand.geometry = geometry;
        //        //drawCommand.setGeometryView(geometry->createIndexedGeometryView());
        //        //return drawCommand;
        //
        //        if (i < 5)
        //            envPass->nextSubpass(cmdBuffer);
        //    }
        //
        //
        //    envPass->end(cmdBuffer);
        //
        //    //// convert HDR equirectangular environment map to cubemap equivalent
        //    //equirectangularToCubemapShader.use();
        //    //equirectangularToCubemapShader.setInt("equirectangularMap", 0);
        //    //equirectangularToCubemapShader.setMat4("projection", captureProjection);
        //    //glActiveTexture(GL_TEXTURE0);
        //    //glBindTexture(GL_TEXTURE_2D, hdrTexture);
        //    //
        //    //glViewport(0, 0, 512, 512); // don't forget to configure the viewport to the capture dimensions.
        //    //glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        //    //for (unsigned int i = 0; i < 6; ++i)
        //    //{
        //    //    equirectangularToCubemapShader.setMat4("view", captureViews[i]);
        //    //    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        //    //        GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, envCubemap, 0);
        //    //    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        //    //
        //    //    renderCube(); // renders a 1x1 cube
        //    //}
        //    //glBindFramebuffer(GL_FRAMEBUFFER, 0);
        //});

        m_renderer->getDevice()->flushDescriptorUpdates();

        auto panel = std::make_unique<gui::ShadowMappingPanel>(m_app->getForm(), this);
        m_app->getForm()->add(std::move(panel));
    }

    ShadowMappingScene::~ShadowMappingScene()
    {
        m_app->getForm()->remove("shadowMappingPanel");
    }

    void ShadowMappingScene::resize(int width, int height)
    {
        m_cameraController->resize(width, height);

        m_renderGraph->resize(width, height);
        m_renderer->setSceneImageView(m_renderGraph->getNode("mainPass").renderPass->createRenderTargetView(0, Renderer::NumVirtualFrames));
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

        m_lights[0].VP = m_cascadedShadowMapper->getLightTransform(0);
        m_lights[0].V  = m_cascadedShadowMapper->getLightTransform(1);
        m_lights[0].P  = m_cascadedShadowMapper->getLightTransform(2);
        m_lightBuffer->updateStagingBuffer(m_lights.data(), m_lights.size() * sizeof(LightDescriptorData));

        //m_skybox->updateTransforms(V, P);

        t += dt;
    }

    void ShadowMappingScene::render()
    {
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

        static constexpr uint32_t Shadow = 0;
        static constexpr uint32_t Pbr = 1;

        auto& shadowPass = m_renderGraph->getNode("csmPass");

        for (int c = 0; c < 4; c++)
            for (int i = 1; i <= 25; i++)
                shadowPass.addCommand(shadowCommand(m_materials[Shadow].get(), m_geometries[2].get(), i, c));
        //shadowPass.addCommand(shadowCommand(m_materials[Shadow].get(), m_csmPipeline.get(), m_geometries[2].get(), 1, 1));
        //shadowPass.addCommand(shadowCommand(m_materials[Shadow].get(), m_csmPipeline.get(), m_geometries[2].get(), 1, 2));
        //shadowPass.addCommand(shadowCommand(m_materials[Shadow].get(), m_csmPipeline.get(), m_geometries[2].get(), 1, 3));

        auto& mainPass = m_renderGraph->getNode("mainPass");

        mainPass.addCommand(createDrawCommand(m_materials[Pbr].get(), m_geometries[0].get(), 0));

        for (int i = 1; i <= 25; i++)
            mainPass.addCommand(createDrawCommand(m_materials[Pbr].get(), m_geometries[1].get(), i));

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

    void ShadowMappingScene::setMetallic(double metallic)
    {
        params.metallic = static_cast<float>(metallic);
    }

    void ShadowMappingScene::setRoughness(double roughness)
    {
        params.roughness = static_cast<float>(roughness);
    }

    void ShadowMappingScene::onMaterialSelected(const std::string& material)
    {
        return;

        Material* modified;
        if (m_activeMaterial == m_materials[1].get())
            modified = m_materials[2].get();
        else
            modified = m_materials[1].get();

        modified->writeDescriptor(0, 0, 0, m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        modified->writeDescriptor(0, 1, 0, m_cameraBuffer->getDescriptorInfo());

        const std::vector<std::string> texNames = { "diffuse", "metallic", "roughness", "normal", "ao" };
        const std::vector<VkFormat> formats = { VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM };
        for (uint32_t i = 0; i < texNames.size(); ++i)
        {
            const std::string& filename = "PbrMaterials/" + material + "/" + texNames[i] + ".png";
            const auto& path = m_renderer->getResourcesPath() / "Textures" / filename;
            if (std::filesystem::exists(path))
            {
                m_images.emplace_back(createTexture(m_renderer, filename, formats[i], true));
                m_imageViews.emplace_back(m_images.back()->createView(VK_IMAGE_VIEW_TYPE_2D));
                modified->writeDescriptor(0, 2 + i, 0, *m_imageViews.back(), m_linearRepeatSampler->getHandle());
            }
            else
            {
                logWarning("Texture type {} is using default values for '{}'\n", material, texNames[i]);
                modified->writeDescriptor(0, 2 + i, 0, *m_imageViews[i], m_linearRepeatSampler->getHandle());
            }
        }

        modified->writeDescriptor(0, 7, 0, *m_envMapView, m_linearRepeatSampler->getHandle());

        modified->writeDescriptor(1, 0, 0, m_lightBuffer->getDescriptorInfo(0, sizeof(LightDescriptorData)));
        for (uint32_t i = 0; i < Renderer::NumVirtualFrames; ++i)
            modified->writeDescriptor(1, 1, i, m_renderGraph->getNode("csmPass").renderPass->getRenderTargetView(0, i), m_linearClampSampler->getHandle());
        m_renderer->getDevice()->flushDescriptorUpdates();
        m_activeMaterial = modified;
    }

    DrawCommand ShadowMappingScene::createDrawCommand(Material* material, Geometry* geometry, int transformIndex)
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
    }

    void ShadowMappingScene::createDefaultPbrTextures()
    {
        // Diffuse
        m_images.emplace_back(createTexture(m_renderer, ImageFileBuffer(1, 1, 4, { 255, 0, 255, 255 }), VK_FORMAT_R8G8B8A8_SRGB));
        m_imageViews.emplace_back(m_images.back()->createView(VK_IMAGE_VIEW_TYPE_2D));

        // Metalness
        m_images.emplace_back(createTexture(m_renderer, ImageFileBuffer(1, 1, 4, { 0, 0, 0, 255 }), VK_FORMAT_R8G8B8A8_UNORM));
        m_imageViews.emplace_back(m_images.back()->createView(VK_IMAGE_VIEW_TYPE_2D));

        // Roughness
        m_images.emplace_back(createTexture(m_renderer, ImageFileBuffer(1, 1, 4, { 32, 32, 32, 255 }), VK_FORMAT_R8G8B8A8_UNORM));
        m_imageViews.emplace_back(m_images.back()->createView(VK_IMAGE_VIEW_TYPE_2D));

        // Normal
        m_images.emplace_back(createTexture(m_renderer, ImageFileBuffer(1, 1, 4, { 127, 127, 255, 255 }), VK_FORMAT_R8G8B8A8_UNORM));
        m_imageViews.emplace_back(m_images.back()->createView(VK_IMAGE_VIEW_TYPE_2D));

        // Ambient occlusion
        m_images.emplace_back(createTexture(m_renderer, ImageFileBuffer(1, 1, 4, { 255, 255, 255, 255 }), VK_FORMAT_R8G8B8A8_UNORM));
        m_imageViews.emplace_back(m_images.back()->createView(VK_IMAGE_VIEW_TYPE_2D));
    }

    std::unique_ptr<Material> ShadowMappingScene::createPbrMaterial(const std::string& type)
    {
        auto material = std::make_unique<Material>(m_physBasedPipeline.get());
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
                material->writeDescriptor(0, 2 + i, 0, *m_imageViews[i], m_linearRepeatSampler->getHandle());
            }
        }

        material->writeDescriptor(0, 7, 0, *m_envMapView, m_linearRepeatSampler->getHandle());

        for (uint32_t i = 0; i < Renderer::NumVirtualFrames; ++i)
        {
            material->writeDescriptor(1, 0, i, m_lightBuffer->getDescriptorInfo(0, sizeof(LightDescriptorData)));
            material->writeDescriptor(1, 1, i, m_renderGraph->getNode("csmPass").renderPass->getRenderTargetView(0, i), m_linearClampSampler->getHandle());
            material->writeDescriptor(1, 2, i, m_cascadedShadowMapper->getSplitBuffer()->getDescriptorInfo());
        }

        return material;
    }

    void ShadowMappingScene::setupControls()
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
}