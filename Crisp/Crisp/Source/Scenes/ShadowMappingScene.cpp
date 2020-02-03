#include "ShadowMappingScene.hpp"

#include "Core/Application.hpp"
#include "Core/Window.hpp"
#include "IO/ImageFileBuffer.hpp"
#include "Camera/CameraController.hpp"

#include "Renderer/RenderPasses/ShadowPass.hpp"
#include "Renderer/RenderPasses/SceneRenderPass.hpp"
#include "Renderer/RenderPasses/DepthPass.hpp"
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
#include "Renderer/Pipelines/ComputePipeline.hpp"

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
#include "Techniques/ShadowMapper.hpp"
#include "Techniques/ForwardClusteredShading.hpp"
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

#include <random>

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

        static constexpr uint32_t NumObjects  = 3;
        static constexpr uint32_t NumLights   = 1;

        static constexpr uint32_t NumCascades = 4;

        static constexpr const char* DepthPrePass = "depthPrePass";
        static constexpr const char* MainPass = "mainPass";
        static constexpr const char* CsmPass  = "csmPass";
        static constexpr const char* ShadowRenderPass = "shadowPass";
        static constexpr const char* VsmPass = "vsmPass";

        static constexpr const char* LightCullingPass = "lightCullingPass";

        int MaterialIndex = 1;

        PointLight pointLight(glm::vec3(1250.0f), glm::vec3(10.0f), glm::normalize(glm::vec3(-1.0f)));

        struct PointLightData
        {
            glm::mat4 VP;
            glm::mat4 V;
            glm::vec3 position;
            glm::vec3 radiance;

            // cubeShadowMap
        };

        struct DirectionalLightData
        {
            glm::mat4 VP;
            glm::mat4 V;
            glm::vec3 direction;
            glm::vec3 radiance;

            // cascadedShadowMap
        };

        struct ConeLightData
        {
            glm::mat4 VP;
            glm::mat4 V;
            glm::vec3 position;
            glm::vec3 direction;

            float cosOuter;
            float cosInner;

            // normalShadowMap
        };

        static const int DefaultShadowMap = 0;

        int lightMode = 0;
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

        auto& depthPrePass = m_renderGraph->addRenderPass(DepthPrePass, std::make_unique<DepthPass>(m_renderer));
        m_renderGraph->addRenderTargetLayoutTransition(DepthPrePass, MainPass, 0);

        // Shadow map pass
        auto& csmPassNode = m_renderGraph->addRenderPass(CsmPass, std::make_unique<ShadowPass>(m_renderer, ShadowMapSize, NumCascades));
        m_renderGraph->addRenderTargetLayoutTransition(CsmPass, MainPass, 0, NumCascades);

        auto& vsmPassNode = m_renderGraph->addRenderPass(VsmPass, std::make_unique<VarianceShadowMapPass>(m_renderer, ShadowMapSize));
        m_renderGraph->addRenderTargetLayoutTransition(VsmPass, MainPass, 0);

        auto& shadowPassNode = m_renderGraph->addRenderPass(ShadowRenderPass, std::make_unique<ShadowPass>(m_renderer, ShadowMapSize, 1));
        m_renderGraph->addRenderTargetLayoutTransition(ShadowRenderPass, MainPass, 0);



        m_shadowMapper = std::make_unique<ShadowMapper>(m_renderer, 4);
        m_shadowMapper->setLightTransform(0, pointLight.getProjectionMatrix() * pointLight.getViewMatrix());


        DirectionalLight light(glm::normalize(glm::vec3(-1.0f, -1.0f, -1.0f)), glm::vec3(10.0f, 10.0f, 10.0f), glm::vec3(-30.0f, -30.0f, -50.0f), glm::vec3(30.0f, 30.0f, 50.0f));
        m_cascadedShadowMapper = std::make_unique<CascadedShadowMapper>(m_renderer, NumCascades, light, 80.0f, ShadowMapSize);

        m_lights.resize(NumLights);
        m_lights[0] = pointLight.createDescriptorData();
        //m_lights[1] = pointLight.createDescriptorData();
        m_lightBuffer = std::make_unique<UniformBuffer>(m_renderer, m_lights.size() * sizeof(LightDescriptor), BufferUpdatePolicy::PerFrame);

        float w = 200.0f;
        float h = 200.0f;
        int r = 32;
        int c = 32;
        std::default_random_engine eng;
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        for (int i = 0; i < r; ++i)
        {
            float normI = static_cast<float>(i) / static_cast<float>(r - 1);

            for (int j = 0; j < c; ++j)
            {
                float normJ = static_cast<float>(j) / static_cast<float>(c - 1);
                ManyLightDescriptor desc;
                desc.spectrum = 5.0f * dist(eng) * glm::vec3(dist(eng), dist(eng), dist(eng));
                desc.position = glm::vec3((normJ - 0.5f) * h, 1.0f, (normI - 0.5f) * w);
                desc.calculateRadius();
                m_manyLights.push_back(desc);
            }
        }

        //for (int i = 0; i < m_manyLights.size(); i += 2)
        //{
        //    m_manyLights[i].radius = 0.0f;
        //    m_manyLights[i].spectrum = glm::vec3(0.0f);
        //}


        m_manyLightsBuffer = std::make_unique<UniformBuffer>(m_renderer, m_manyLights.size() * sizeof(ManyLightDescriptor), BufferUpdatePolicy::Constant, m_manyLights.data());

        auto* params = m_cameraController->getCameraParameters();
        glm::ivec2 tileSize(16);
        glm::ivec2 tileGrid = calculateTileGridDims(tileSize, params->screenSize);
        auto tilePlanes = createTileFrusta(tileSize, params->screenSize, params->P);
        auto tileCount  = tilePlanes.size();

        m_tilePlaneBuffer       = std::make_unique<UniformBuffer>(m_renderer, tileCount * sizeof(TileFrustum), true, tilePlanes.data());
        m_lightIndexCountBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(uint32_t), true);
        m_lightIndexListBuffer  = std::make_unique<UniformBuffer>(m_renderer, tileCount * sizeof(uint32_t) * 1024, true);

        VkImageCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        createInfo.flags         = 0;
        createInfo.imageType     = VK_IMAGE_TYPE_2D;
        createInfo.format        = VK_FORMAT_R32G32_UINT;
        createInfo.extent        = VkExtent3D{ (uint32_t)tileGrid.x, (uint32_t)tileGrid.y, 1u };
        createInfo.mipLevels     = 1;
        createInfo.arrayLayers   = Renderer::NumVirtualFrames;
        createInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        createInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        createInfo.usage         = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        m_lightGrid = std::make_unique<VulkanImage>(m_renderer->getDevice(), createInfo, VK_IMAGE_ASPECT_COLOR_BIT);
        m_lightGridViews.emplace_back(m_lightGrid->createView(VK_IMAGE_VIEW_TYPE_2D, 0, 1));
        m_lightGridViews.emplace_back(m_lightGrid->createView(VK_IMAGE_VIEW_TYPE_2D, 1, 1));
        m_renderer->enqueueResourceUpdate([this](VkCommandBuffer cmdBuffer)
            {
                m_lightGrid->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_GENERAL,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            });

        auto& cullingPass = m_renderGraph->addComputePass(LightCullingPass);
        cullingPass.workGroupSize = glm::ivec3(tileSize, 1);
        cullingPass.numWorkGroups = glm::ivec3(tileGrid, 1);
        cullingPass.pipeline = createLightCullingComputePipeline(m_renderer, cullingPass.workGroupSize);
        cullingPass.material = std::make_unique<Material>(cullingPass.pipeline.get());
        cullingPass.material->writeDescriptor(0, 0, m_tilePlaneBuffer->getDescriptorInfo());
        cullingPass.material->writeDescriptor(0, 1, m_lightIndexCountBuffer->getDescriptorInfo());
        cullingPass.material->writeDescriptor(0, 2, m_manyLightsBuffer->getDescriptorInfo());
        cullingPass.material->writeDescriptor(0, 3, m_cameraBuffer->getDescriptorInfo());
        cullingPass.material->writeDescriptor(0, 4, m_lightIndexListBuffer->getDescriptorInfo());
        cullingPass.material->writeDescriptor(1, 0, *depthPrePass.renderPass, 0, nullptr);
        cullingPass.material->writeDescriptor(2, 0, m_lightGridViews, nullptr, VK_IMAGE_LAYOUT_GENERAL );
        cullingPass.material->setDynamicBufferView(0, *m_lightIndexCountBuffer, 0);
        cullingPass.material->setDynamicBufferView(1, *m_cameraBuffer, 0);
        cullingPass.material->setDynamicBufferView(2, *m_lightIndexListBuffer, 0);
        cullingPass.callback = [this](VkCommandBuffer cmdBuffer, uint32_t frameIndex)
        {
            glm::uvec4 zero(0);

            VkBufferMemoryBarrier barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            barrier.buffer        = m_lightIndexCountBuffer->get();
            barrier.offset        = frameIndex * sizeof(zero);
            barrier.size          = sizeof(zero);

            vkCmdUpdateBuffer(cmdBuffer, barrier.buffer, barrier.offset, barrier.size, &zero);
            vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0, 0, nullptr, 1, &barrier, 0, nullptr);
        };

        m_renderGraph->addRenderTargetLayoutTransition(DepthPrePass, LightCullingPass, 0);
        m_renderGraph->addDependency(LightCullingPass, MainPass, [this](const VulkanRenderPass&, VkCommandBuffer cmdBuffer, uint32_t frameIndex)
        {
            VkBufferMemoryBarrier barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            auto info = m_lightIndexListBuffer->getDescriptorInfo();
            barrier.buffer        = info.buffer;
            barrier.offset        = info.offset;
            barrier.size          = info.range;

            vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0, 0, nullptr, 1, &barrier, 0, nullptr);
        });

        m_renderGraph->sortRenderPasses();
        //m_computeCellCountPipeline->bind(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE);
        //m_computeCellCountPipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, 0, m_gridParams.dim);
        //m_computeCellCountPipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(m_gridParams.dim), m_gridParams.cellSize);
        //m_computeCellCountPipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(m_gridParams.dim) + sizeof(m_gridParams.cellSize), m_numParticles);
        //m_computeCellCountDescGroup.setDynamicOffset(0, m_prevSection* m_numParticles * sizeof(glm::vec4));
        //m_computeCellCountDescGroup.setDynamicOffset(1, m_currentSection* m_gridParams.numCells * sizeof(uint32_t));
        //m_computeCellCountDescGroup.setDynamicOffset(2, m_currentSection* m_numParticles * sizeof(uint32_t));
        //m_computeCellCountDescGroup.bind<VK_PIPELINE_BIND_POINT_COMPUTE>(cmdBuffer, m_computeCellCountPipeline->getPipelineLayout()->getHandle());
        //
        //glm::uvec3 numGroups = getNumWorkGroups(glm::uvec3(m_numParticles, 1, 1), *m_computeCellCountPipeline);
        //vkCmdDispatch(cmdBuffer, numGroups.x, numGroups.y, numGroups.z);
        //insertComputeBarrier(cmdBuffer);



        // Object transforms
        m_transforms.resize(NumObjects);
        m_transformBuffer = std::make_unique<UniformBuffer>(m_renderer, m_transforms.size() * sizeof(TransformPack), BufferUpdatePolicy::PerFrame);

        // Geometry setup
        std::vector<VertexAttributeDescriptor> shadowVertexFormat = { VertexAttribute::Position };
        std::vector<VertexAttributeDescriptor> pbrVertexFormat    = { VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord, VertexAttribute::Tangent, VertexAttribute::Bitangent };

        // Plane geometry
        m_geometries.emplace("floor", std::make_unique<Geometry>(m_renderer, createPlaneMesh(pbrVertexFormat)));
        m_geometries.emplace("floorShadow", std::make_unique<Geometry>(m_renderer, createPlaneMesh(shadowVertexFormat)));

        // Shaderball geometry
        m_geometries.emplace("shaderball", std::make_unique<Geometry>(m_renderer, m_renderer->getResourcesPath() / "Meshes/ShaderBall_FWVN_PosX.obj", pbrVertexFormat));
        m_geometries.emplace("shaderballShadow", std::make_unique<Geometry>(m_renderer, m_renderer->getResourcesPath() / "Meshes/ShaderBall_FWVN_PosX.obj", shadowVertexFormat));
        //m_geometries.emplace("rungholt", std::make_unique<Geometry>(m_renderer, m_renderer->getResourcesPath() / "Meshes/rungholt.obj", pbrVertexFormat));
        m_geometries.emplace("sphere", std::make_unique<Geometry>(m_renderer, createSphereMesh(pbrVertexFormat)));
        m_geometries.emplace("sphereShadow", std::make_unique<Geometry>(m_renderer, createSphereMesh(shadowVertexFormat)));
        //m_geometries.emplace(std::make_unique<Geometry>(m_renderer, m_renderer->getResourcesPath() / "Meshes/nanosuit/nanosuit.obj", pbrVertexFormat));
        //m_geometries.emplace("cubeShadow", std::make_unique<Geometry>(m_renderer, m_renderer->getResourcesPath() / "Meshes/cube.obj", shadowVertexFormat));

        m_geometries.emplace("cube", std::make_unique<Geometry>(m_renderer, createCubeMesh(pbrVertexFormat)));
        m_geometries.emplace("cubeShadow", std::make_unique<Geometry>(m_renderer, createCubeMesh(shadowVertexFormat)));

        //// Shadow map material setup
        //auto csmPipeline = createShadowMapPipeline(m_renderer, csmPassNode.renderPass.get(), 0);
        //auto shadowMapMaterial = std::make_unique<Material>(csmPipeline.get());
        //shadowMapMaterial->writeDescriptor(0, 0, 0, m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        //shadowMapMaterial->writeDescriptor(0, 1, 0, m_cascadedShadowMapper->getLightTransformBuffer()->getDescriptorInfo());
        //shadowMapMaterial->setDynamicBufferView(1, *m_cascadedShadowMapper->getLightTransformBuffer(), 0);
        //m_pipelines.emplace("csm0", std::move(csmPipeline));
        //for (uint32_t i = 1; i < NumCascades; i++)
        //    m_pipelines.emplace("csm" + std::to_string(i), createShadowMapPipeline(m_renderer, csmPassNode.renderPass.get(), i));
        //m_materials.emplace("csm", std::move(shadowMapMaterial));

        // Point shadow map
        auto smPipeline = m_renderer->createPipelineFromLua("ShadowMap.lua", shadowPassNode.renderPass.get(), 0);
        auto smMaterial = std::make_unique<Material>(smPipeline.get());
        smMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        smMaterial->writeDescriptor(0, 1, m_shadowMapper->getLightTransformBuffer()->getDescriptorInfo());
        smMaterial->setDynamicBufferView(1, *m_shadowMapper->getLightTransformBuffer(), 0);
        m_pipelines.emplace("sm", std::move(smPipeline));
        m_materials.emplace("sm", std::move(smMaterial));

        auto vsmPipeline = m_renderer->createPipelineFromLua("VarianceShadowMap.lua", vsmPassNode.renderPass.get(), 0);
        auto vsmMaterial = std::make_unique<Material>(vsmPipeline.get());
        vsmMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        vsmMaterial->writeDescriptor(0, 1, m_shadowMapper->getLightFullTransformBuffer()->getDescriptorInfo());
        vsmMaterial->setDynamicBufferView(1, *m_shadowMapper->getLightFullTransformBuffer(), 0);
        m_pipelines.emplace("vsm", std::move(vsmPipeline));
        m_materials.emplace("vsm", std::move(vsmMaterial));

        auto depthPipeline = m_renderer->createPipelineFromLua("DepthPrepass.lua", depthPrePass.renderPass.get(), 0);
        auto depthMaterial = std::make_unique<Material>(depthPipeline.get());
        depthMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        m_pipelines.emplace("depth", std::move(depthPipeline));
        m_materials.emplace("depth", std::move(depthMaterial));


        // Common material components
        createDefaultPbrTextures();
        m_linearClampSampler     = std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 16.0f, 11.0f);
        m_linearRepeatSampler    = std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 16.0f, 12.0f);
        m_nearestNeighborSampler = std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        m_mipmapSampler          = std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 16.0f, 5.0f);

        LuaConfig config(m_renderer->getResourcesPath() / "Scripts/scene.lua");

        // Environment map
        auto hdrName = config.get<std::string>("environmentMap").value_or("NewportLoft_Ref") + ".hdr";
        auto envRefMap = createEnvironmentMap(m_renderer, hdrName, VK_FORMAT_R32G32B32A32_SFLOAT, true);
        std::shared_ptr<VulkanImageView> envRefMapView = envRefMap->createView(VK_IMAGE_VIEW_TYPE_2D);

        auto [cubeMap, cubeMapView] = convertEquirectToCubeMap(envRefMapView);
        setupDiffuseEnvMap(*cubeMapView);
        setupReflectEnvMap(*cubeMapView);

        m_brdfLut = integrateBrdfLut();
        m_brdfLutView = m_brdfLut->createView(VK_IMAGE_VIEW_TYPE_2D);

        // Physically-based material setup
        auto texFolder = config.get<std::string>("texture").value_or("RustedIron");
        auto physBasedPipeline = createPbrPipeline(m_renderer, mainPassNode.renderPass.get(), 3);
        m_materials.emplace("pbr", createPbrMaterial(texFolder, physBasedPipeline.get()));
        m_materials.emplace("pbrMetalGrid", createPbrMaterial("MetalGrid", physBasedPipeline.get()));
        m_pipelines.emplace("pbr", std::move(physBasedPipeline));

        m_pbrUnifMaterial.albedo = glm::vec3(0.3f);
        m_pbrUnifMaterial.metallic = 0.0f;
        m_pbrUnifMaterial.roughness = 0.0f;
        m_pbrUnifMatBuffer = std::make_unique<UniformBuffer>(m_renderer, sizeof(m_pbrUnifMaterial), BufferUpdatePolicy::PerFrame, &m_pbrUnifMaterial);
        auto pbrUnifPipeline = m_renderer->createPipelineFromLua("PbrUnif.lua", mainPassNode.renderPass.get(), 0);
        m_materials.emplace("pbrUnif", createUnifPbrMaterial(pbrUnifPipeline.get()));
        m_pipelines.emplace("pbrUnif", std::move(pbrUnifPipeline));

        auto pbrUnifSmPipeline = createPbrUnifPipeline(m_renderer, mainPassNode.renderPass.get());
        m_materials.emplace("pbrUnifSm", createUnifPbrSmMaterial(pbrUnifSmPipeline.get()));
        m_pipelines.emplace("pbrUnifSm", std::move(pbrUnifSmPipeline));

        m_boxVisualizer = std::make_unique<BoxVisualizer>(m_renderer, 4, 4, *mainPassNode.renderPass);

        m_skybox = std::make_unique<Skybox>(m_renderer, *mainPassNode.renderPass, std::move(cubeMap), std::move(cubeMapView));

        m_renderer->getDevice()->flushDescriptorUpdates();

        m_app->getForm()->add(gui::createShadowMappingSceneGui(m_app->getForm(), this));

        addRenderNode("floor", 0, glm::vec3(0.0f), glm::vec3(100.0f, 1.0f, 100.0f));
        addRenderNode("sphere", 1, glm::vec3(0.0f, 0.5f, 0.0f), glm::vec3(1.0f));
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

        m_lightBuffer->updateStagingBuffer(m_lights.data(), m_lights.size() * sizeof(LightDescriptor));

        m_pbrUnifMatBuffer->updateStagingBuffer(m_pbrUnifMaterial);

        m_skybox->updateTransforms(V, P);

        m_boxVisualizer->updateFrusta(m_cascadedShadowMapper.get(), m_cameraController.get());

        m_shadowMapper->setLightTransform(0, pointLight.getProjectionMatrix() * pointLight.getViewMatrix());
        m_shadowMapper->setLightFullTransform(0, pointLight.getViewMatrix(), pointLight.getProjectionMatrix());

        t += dt;
    }

    void ShadowMappingScene::render()
    {
        //m_renderer->enqueueResourceUpdate([this](VkCommandBuffer cmdBuffer)
        //    {
        //        uint32_t frameIdx = m_renderer->getCurrentVirtualFrameIndex();
        //        m_boxVisualizer->updateDeviceBuffers(cmdBuffer, frameIdx);
        //    });
        //
        m_renderGraph->clearCommandLists();
        m_renderGraph->buildCommandLists();
        m_renderGraph->executeCommandLists();
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

    void ShadowMappingScene::onMaterialSelected(const std::string& /*material*/)
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
        //modified->writeDescriptor(1, 0, 0, m_lightBuffer->getDescriptorInfo(0, sizeof(LightDescriptor)));
        //for (uint32_t i = 0; i < Renderer::NumVirtualFrames; ++i)
        //    modified->writeDescriptor(1, 1, i, m_renderGraph->getNode("csmPass").renderPass->getRenderTargetView(0, i), m_linearClampSampler->getHandle());
        //m_renderer->getDevice()->flushDescriptorUpdates();
        //m_activeMaterial = modified;
    }

    void ShadowMappingScene::addRenderNode(std::string_view geometryName, int transformIndex, glm::vec3 translation, glm::vec3 scale)
    {
        RenderNode floor(m_transformBuffer.get(), m_transforms, transformIndex);
        floor.geometry = m_geometries.at(std::string(geometryName)).get();
        floor.material = m_materials.at("pbrUnif").get();
        floor.pipeline = m_pipelines.at("pbrUnif").get();
        floor.transformPack->M = glm::translate(translation) * glm::scale(scale);
        floor.setPushConstantView(lightMode);
        m_renderGraph->getNode(MainPass).renderNodes.push_back(floor);

        RenderNode shaderBallShadowVsm = floor;
        shaderBallShadowVsm.geometry = m_geometries.at(std::string(geometryName) + "Shadow").get();
        shaderBallShadowVsm.material = m_materials.at("vsm").get();
        shaderBallShadowVsm.pipeline = m_pipelines.at("vsm").get();
        shaderBallShadowVsm.pushConstantView.set(DefaultShadowMap);
        m_renderGraph->getNode(VsmPass).renderNodes.push_back(shaderBallShadowVsm);

        RenderNode shaderBallDepth = floor;
        shaderBallDepth.geometry = m_geometries.at(std::string(geometryName) + "Shadow").get();
        shaderBallDepth.material = m_materials.at("depth").get();
        shaderBallDepth.pipeline = m_pipelines.at("depth").get();
        m_renderGraph->getNode(DepthPrePass).renderNodes.push_back(shaderBallDepth);
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
        material->setDynamicBufferView(1, *m_cameraBuffer, 0);

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
                material->writeDescriptor(0, 2 + i, 0, *m_imageViews.back(), m_linearRepeatSampler.get());
            }
            else
            {
                logWarning("Texture type {} is using default values for '{}'\n", type, texNames[i]);
                material->writeDescriptor(0, 2 + i, 0, *m_defaultTexImageViews.at(texNames[i]), m_linearRepeatSampler.get());
            }
        }

        for (uint32_t i = 0; i < Renderer::NumVirtualFrames; ++i)
        {
            material->writeDescriptor(1, 0, i, m_lightBuffer->getDescriptorInfo(0, sizeof(LightDescriptor)));
            material->writeDescriptor(1, 1, i, m_renderGraph->getNode(CsmPass).renderPass->getRenderTargetView(0, i).getDescriptorInfo(*m_nearestNeighborSampler));
            material->writeDescriptor(1, 2, i, m_cascadedShadowMapper->getSplitBuffer()->getDescriptorInfo());
            material->writeDescriptor(1, 3, i, m_cascadedShadowMapper->getLightTransformBuffer()->getDescriptorInfo());
        }
        material->setDynamicBufferView(2, *m_lightBuffer, 0);
        material->setDynamicBufferView(3, *m_cascadedShadowMapper->getSplitBuffer(), 0);
        material->setDynamicBufferView(4, *m_cascadedShadowMapper->getLightTransformBuffer(), 0);

        material->writeDescriptor(2, 0, 0, *m_envIrrMapView, m_linearClampSampler.get());
        material->writeDescriptor(2, 1, 0, *m_filteredMapView, m_mipmapSampler.get());
        material->writeDescriptor(2, 2, 0, *m_brdfLutView, m_linearClampSampler.get());

        return material;
    }

    std::unique_ptr<Material> ShadowMappingScene::createUnifPbrMaterial(VulkanPipeline* pipeline)
    {
        auto material = std::make_unique<Material>(pipeline);
        material->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        material->writeDescriptor(0, 1, *m_cameraBuffer);
        material->writeDescriptor(0, 2, *m_pbrUnifMatBuffer);
        material->setDynamicBufferView(1, *m_cameraBuffer, 0);
        material->setDynamicBufferView(2, *m_pbrUnifMatBuffer, 0);

        material->writeDescriptor(1, 0, *m_lightBuffer);
        material->writeDescriptor(1, 1, *m_renderGraph->getNode(CsmPass).renderPass, 0, m_nearestNeighborSampler.get());
        material->writeDescriptor(1, 2, *m_cascadedShadowMapper->getSplitBuffer());
        material->writeDescriptor(1, 3, *m_cascadedShadowMapper->getLightTransformBuffer());
        material->writeDescriptor(1, 4, *m_renderGraph->getNode(VsmPass).renderPass, 0, m_nearestNeighborSampler.get());
        material->writeDescriptor(1, 5, *m_shadowMapper->getLightTransformBuffer());
        material->writeDescriptor(1, 6, *m_manyLightsBuffer);
        material->writeDescriptor(1, 7, m_lightGridViews, nullptr, VK_IMAGE_LAYOUT_GENERAL);
        material->writeDescriptor(1, 8, *m_lightIndexListBuffer);

        material->setDynamicBufferView(3, *m_lightBuffer, 0);
        material->setDynamicBufferView(4, *m_cascadedShadowMapper->getSplitBuffer(), 0);
        material->setDynamicBufferView(5, *m_cascadedShadowMapper->getLightTransformBuffer(), 0);
        material->setDynamicBufferView(6, *m_shadowMapper->getLightTransformBuffer(), 0);
        material->setDynamicBufferView(7, *m_lightIndexListBuffer, 0);

        // Environment Lighting
        material->writeDescriptor(2, 0, *m_envIrrMapView, m_linearClampSampler.get());
        material->writeDescriptor(2, 1, *m_filteredMapView,    m_mipmapSampler.get());
        material->writeDescriptor(2, 2, *m_brdfLutView,   m_linearClampSampler.get());

        return material;
    }

    std::unique_ptr<Material> ShadowMappingScene::createUnifPbrSmMaterial(VulkanPipeline* pipeline)
    {
        auto material = std::make_unique<Material>(pipeline);
        material->writeDescriptor(0, 0, 0, m_transformBuffer->getDescriptorInfo(0, sizeof(TransformPack)));
        material->writeDescriptor(0, 1, *m_cameraBuffer);
        material->writeDescriptor(0, 2, *m_pbrUnifMatBuffer);
        material->setDynamicBufferView(1, *m_cameraBuffer, 0);
        material->setDynamicBufferView(2, *m_pbrUnifMatBuffer, 0);

        material->writeDescriptor(1, 0, *m_lightBuffer);
        material->writeDescriptor(1, 1, *m_renderGraph->getNode(CsmPass).renderPass, 0, m_nearestNeighborSampler.get());
        material->writeDescriptor(1, 2, *m_cascadedShadowMapper->getSplitBuffer());
        material->writeDescriptor(1, 3, *m_cascadedShadowMapper->getLightTransformBuffer());
        material->writeDescriptor(1, 4, *m_renderGraph->getNode(ShadowRenderPass).renderPass, 0, m_nearestNeighborSampler.get());
        material->writeDescriptor(1, 5, *m_shadowMapper->getLightTransformBuffer());

        material->setDynamicBufferView(3, *m_lightBuffer, 0);
        material->setDynamicBufferView(4, *m_cascadedShadowMapper->getSplitBuffer(), 0);
        material->setDynamicBufferView(5, *m_cascadedShadowMapper->getLightTransformBuffer(), 0);
        material->setDynamicBufferView(6, *m_shadowMapper->getLightTransformBuffer(), 0);

        material->writeDescriptor(2, 0, *m_envIrrMapView, m_linearClampSampler.get());
        material->writeDescriptor(2, 1, *m_filteredMapView, m_mipmapSampler.get());
        material->writeDescriptor(2, 2, *m_brdfLutView, m_linearClampSampler.get());

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

            if (key == Key::One)
            {
                MaterialIndex = 1;
            }

            if (key == Key::Two)
                MaterialIndex = 3;

            if (key == Key::M)
                lightMode = lightMode == 0 ? 1 : 0;
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
            cubeMapPipelines[i] = m_renderer->createPipelineFromLua("IrradianceMap.lua", cubeMapPass.get(), i);

        auto cubeMapMaterial = std::make_unique<Material>(cubeMapPipelines[0].get());
        cubeMapMaterial->writeDescriptor(0, 0, 0, *equirectMapView, m_linearRepeatSampler.get());
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
                m_geometries.at("cubeShadow")->bindAndDraw(cmdBuffer);

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
        for (int i = 0; i < CubeMapFaceCount; i++)
            convPipelines[i] = m_renderer->createPipelineFromLua("ConvolveDiffuse.lua", convPass.get(), i);

        auto convMaterial = std::make_unique<Material>(convPipelines[0].get());
        convMaterial->writeDescriptor(0, 0, 0, cubeMapView, m_linearRepeatSampler.get());
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
                m_geometries.at("cubeShadow")->bindAndDraw(cmdBuffer);

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
        static constexpr uint32_t CubeMapFaceCount = 6;
        constexpr uint32_t mipLevels = 5;
        m_filteredMap = createMipmapCubeMap(m_renderer, 512, 512, mipLevels);
        for (int i = 0; i < static_cast<int>(mipLevels); i++)
        {
            float roughness = i / static_cast<float>(mipLevels - 1);

            unsigned int w = static_cast<unsigned int>(512 * std::pow(0.5, i));
            unsigned int h = static_cast<unsigned int>(512 * std::pow(0.5, i));
            std::shared_ptr<CubeMapRenderPass> prefilterPass = std::make_unique<CubeMapRenderPass>(m_renderer, VkExtent2D{ w, h });

            std::vector<std::unique_ptr<VulkanPipeline>> filterPipelines(CubeMapFaceCount);
            for (int j = 0; j < CubeMapFaceCount; j++)
                filterPipelines[j] = m_renderer->createPipelineFromLua("PrefilterSpecular.lua", prefilterPass.get(), j);

            std::shared_ptr filterMat = std::make_unique<Material>(filterPipelines[0].get());
            filterMat->writeDescriptor(0, 0, 0, cubeMapView, m_linearRepeatSampler.get());
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
                    m_geometries.at("cubeShadow")->bindAndDraw(cmdBuffer);

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
        std::shared_ptr<VulkanPipeline> pipeline = m_renderer->createPipelineFromLua("BrdfLut.lua", texPass.get(), 0);

        auto material = std::make_shared<Material>(pipeline.get());

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