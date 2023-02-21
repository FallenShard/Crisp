#include <Crisp/Scenes/GltfViewerScene.hpp>

#include <Crisp/Common/Checks.hpp>
#include <Crisp/Core/Application.hpp>
#include <Crisp/Core/Window.hpp>

#include <Crisp/Camera/FreeCameraController.hpp>
#include <Crisp/Camera/TargetCameraController.hpp>

#include <Crisp/Vulkan/VulkanImageView.hpp>
#include <Crisp/Vulkan/VulkanPipeline.hpp>
#include <Crisp/Vulkan/VulkanSampler.hpp>
#include <Crisp/vulkan/VulkanDevice.hpp>
#include <Crisp/vulkan/VulkanImage.hpp>

#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/RenderGraph.hpp>
#include <Crisp/Renderer/RenderPasses/ForwardLightingPass.hpp>
#include <Crisp/Renderer/RenderPasses/ShadowPass.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/ResourceContext.hpp>
#include <Crisp/Renderer/UniformBuffer.hpp>
#include <Crisp/Renderer/VulkanBufferUtils.hpp>
#include <Crisp/Renderer/VulkanImageUtils.hpp>

#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Geometry/TransformBuffer.hpp>
#include <Crisp/Models/Atmosphere.hpp>
#include <Crisp/Models/Skybox.hpp>

#include <Crisp/Lights/EnvironmentLighting.hpp>
#include <Crisp/Lights/LightSystem.hpp>

#include <Crisp/GUI/Button.hpp>
#include <Crisp/GUI/CheckBox.hpp>
#include <Crisp/GUI/ComboBox.hpp>
#include <Crisp/GUI/Form.hpp>
#include <Crisp/GUI/Label.hpp>
#include <Crisp/GUI/Slider.hpp>

#include <Crisp/Mesh/Io/GltfReader.hpp>
#include <Crisp/Mesh/Io/MeshLoader.hpp>
#include <Crisp/Utils/GlmFormatters.hpp>

#include <Crisp/Math/Constants.hpp>
#include <Crisp/Mesh/TriangleMeshUtils.hpp>
#include <Crisp/Utils/Profiler.hpp>

#include <Crisp/IO/FileUtils.hpp>
#include <Crisp/IO/JsonUtils.hpp>
#include <Crisp/Mesh/SkinningData.hpp>
#include <Crisp/Renderer/IO/JsonPipelineBuilder.hpp>
#include <Crisp/Renderer/PipelineBuilder.hpp>
#include <Crisp/Renderer/PipelineLayoutBuilder.hpp>

#include <imgui.h>

namespace crisp
{
namespace
{
const auto logger = createLoggerMt("GltfViewerScene");

constexpr const char* ForwardLightingPass = "forwardPass";
constexpr const char* OutputPass = ForwardLightingPass;

constexpr uint32_t ShadowMapSize = 1024;
constexpr uint32_t CascadeCount = 4;
constexpr std::array<const char*, CascadeCount> CsmPasses = {
    "csmPass0",
    "csmPass1",
    "csmPass2",
    "csmPass3",
};

const VertexLayoutDescription PbrVertexFormat = {
    {VertexAttribute::Position},
    {VertexAttribute::Normal, VertexAttribute::TexCoord, VertexAttribute::Tangent}
};

void setPbrMaterialSceneParams(
    Material& material, const ResourceContext& resourceContext, const LightSystem& lightSystem)
{
    auto& imageCache = resourceContext.imageCache;
    material.writeDescriptor(1, 0, *resourceContext.getUniformBuffer("camera"));
    material.writeDescriptor(1, 1, *lightSystem.getCascadedDirectionalLightBuffer());
    material.writeDescriptor(1, 2, imageCache.getImageView("diffEnvMap"), imageCache.getSampler("linearClamp"));
    material.writeDescriptor(1, 3, imageCache.getImageView("specEnvMap"), imageCache.getSampler("linearMipmap"));
    material.writeDescriptor(1, 4, imageCache.getImageView("brdfLut"), imageCache.getSampler("linearClamp"));
    material.writeDescriptor(1, 5, 0, imageCache.getImageView("csmFrame0"), &imageCache.getSampler("nearestNeighbor"));
    material.writeDescriptor(1, 5, 1, imageCache.getImageView("csmFrame1"), &imageCache.getSampler("nearestNeighbor"));
    material.writeDescriptor(1, 6, imageCache.getImageView("sheenLut"), imageCache.getSampler("linearClamp"));
}

Material* createPbrMaterial(
    const std::string& materialId,
    const std::string& materialKey,
    ResourceContext& resourceContext,
    const PbrParams& params,
    const TransformBuffer& transformBuffer)
{
    auto& imageCache = resourceContext.imageCache;

    auto material = resourceContext.createMaterial("pbrTex" + materialId, "pbrTex");
    material->writeDescriptor(0, 0, transformBuffer.getDescriptorInfo());

    const auto setMaterialTexture =
        [&material, &imageCache, &materialKey](const uint32_t index, const std::string_view texName)
    {
        const std::string key = fmt::format("{}-{}", materialKey, texName);
        const std::string fallbackKey = fmt::format("default-{}", texName);
        material->writeDescriptor(
            2, index, imageCache.getImageView(key, fallbackKey), imageCache.getSampler("linearRepeat"));
    };

    setMaterialTexture(0, "diffuse");
    setMaterialTexture(1, "metallic");
    setMaterialTexture(2, "roughness");
    setMaterialTexture(3, "normal");
    setMaterialTexture(4, "ao");
    setMaterialTexture(5, "emissive");

    const std::string paramsBufferKey{fmt::format("{}Params", materialId)};
    material->writeDescriptor(
        2, 6, *resourceContext.createUniformBuffer(paramsBufferKey, params, BufferUpdatePolicy::PerFrame));

    return material;
}

std::unique_ptr<VulkanPipeline> createComputePipeline(
    Renderer* renderer,
    const glm::uvec3& workGroupSize,
    const PipelineLayoutBuilder& layoutBuilder,
    const std::string& shaderName)
{
    VulkanDevice& device = renderer->getDevice();
    auto layout = layoutBuilder.create(device);

    std::vector<VkSpecializationMapEntry> specEntries = {
  //   id,               offset,             size
        {0, 0 * sizeof(uint32_t), sizeof(uint32_t)},
        {1, 1 * sizeof(uint32_t), sizeof(uint32_t)},
        {2, 2 * sizeof(uint32_t), sizeof(uint32_t)}
    };

    VkSpecializationInfo specInfo = {};
    specInfo.mapEntryCount = static_cast<uint32_t>(specEntries.size());
    specInfo.pMapEntries = specEntries.data();
    specInfo.dataSize = sizeof(workGroupSize);
    specInfo.pData = glm::value_ptr(workGroupSize);

    VkComputePipelineCreateInfo pipelineInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pipelineInfo.stage = createShaderStageInfo(VK_SHADER_STAGE_COMPUTE_BIT, renderer->getShaderModule(shaderName));
    pipelineInfo.stage.pSpecializationInfo = &specInfo;
    pipelineInfo.layout = layout->getHandle();
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;
    VkPipeline pipeline;
    vkCreateComputePipelines(device.getHandle(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

    return std::make_unique<VulkanPipeline>(
        device, pipeline, std::move(layout), VK_PIPELINE_BIND_POINT_COMPUTE, VertexLayout{});
}

std::unique_ptr<VulkanPipeline> createSkinningPipeline(Renderer* renderer, const glm::uvec3& workGroupSize)
{
    PipelineLayoutBuilder layoutBuilder;
    layoutBuilder.defineDescriptorSet(
        0,
        false,
        {
            {0,         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {1,         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {2,         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {4,         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
    });

    layoutBuilder.addPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, 2 * sizeof(uint32_t));
    return createComputePipeline(renderer, workGroupSize, layoutBuilder, "linear-blend-skinning.comp");
}

} // namespace

GltfViewerScene::GltfViewerScene(Renderer* renderer, Application* app)
    : AbstractScene(app, renderer)
{
    setupInput();

    m_cameraController = std::make_unique<TargetCameraController>(app->getWindow());
    m_cameraController->setDistance(3.0f);
    m_resourceContext->createUniformBuffer("camera", sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);
    m_renderer->getDebugMarker().setObjectName(m_resourceContext->getUniformBuffer("camera")->get(), "cameraBuffer");

    m_renderGraph->addRenderPass(
        ForwardLightingPass,
        createForwardLightingPass(
            m_renderer->getDevice(), m_resourceContext->renderTargetCache, m_renderer->getSwapChainExtent()));
    for (uint32_t i = 0; i < CsmPasses.size(); ++i)
    {
        m_renderGraph->addRenderPass(
            CsmPasses[i],
            createShadowMapPass(m_renderer->getDevice(), m_resourceContext->renderTargetCache, ShadowMapSize, i));
        m_renderGraph->addDependency(CsmPasses[i], ForwardLightingPass, 0);
    }

    auto& imageCache = m_resourceContext->imageCache;
    imageCache.addImageView(
        "csmFrame0",
        m_resourceContext->renderTargetCache.get("ShadowMap")
            ->image->createView(VK_IMAGE_VIEW_TYPE_2D_ARRAY, 0, CascadeCount));
    imageCache.addImageView(
        "csmFrame1",
        m_resourceContext->renderTargetCache.get("ShadowMap")
            ->image->createView(VK_IMAGE_VIEW_TYPE_2D_ARRAY, CascadeCount, CascadeCount));

    // Wrap-up render graph definition
    m_renderGraph->addDependencyToPresentation(ForwardLightingPass, 0);

    m_renderer->setSceneImageView(m_renderGraph->getNode(ForwardLightingPass).renderPass.get(), 0);

    m_lightSystem = std::make_unique<LightSystem>(
        m_renderer,
        DirectionalLight(-glm::vec3(1, 1, 0), glm::vec3(3.0f), glm::vec3(-5), glm::vec3(5)),
        ShadowMapSize,
        CascadeCount);

    // Object transforms
    m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, 100);
    m_renderer->getDebugMarker().setObjectName(m_transformBuffer->getUniformBuffer()->get(), "transformBuffer");

    createCommonTextures();

    for (uint32_t i = 0; i < CascadeCount; ++i)
    {
        std::string key = "cascadedShadowMap" + std::to_string(i);
        auto csmPipeline =
            m_resourceContext->createPipeline(key, "ShadowMap.json", m_renderGraph->getRenderPass(CsmPasses[i]), 0);
        auto csmMaterial = m_resourceContext->createMaterial(key, csmPipeline);
        csmMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
        csmMaterial->writeDescriptor(0, 1, *m_lightSystem->getCascadedDirectionalLightBuffer(i));
    }

    loadGltf("CesiumMan");

    m_renderGraph->sortRenderPasses().unwrap();
    m_renderGraph->printExecutionOrder();

    m_skybox = std::make_unique<Skybox>(
        m_renderer,
        m_renderGraph->getRenderPass(ForwardLightingPass),
        imageCache.getImageView("cubeMap"),
        imageCache.getSampler("linearClamp"));

    m_renderer->getDevice().flushDescriptorUpdates();
}

GltfViewerScene::~GltfViewerScene()
{
    m_renderer->setSceneImageView(nullptr, 0);
}

void GltfViewerScene::resize(int width, int height)
{
    m_cameraController->onViewportResized(width, height);

    m_resourceContext->renderTargetCache.resizeRenderTargets(m_renderer->getDevice(), m_renderer->getSwapChainExtent());

    m_renderGraph->resize(width, height);
    m_renderer->setSceneImageView(m_renderGraph->getNode(ForwardLightingPass).renderPass.get(), 0);
}

void GltfViewerScene::update(float dt)
{
    static float totalT = 0.0f;

    // Camera
    m_cameraController->update(dt);
    const auto camParams = m_cameraController->getCameraParameters();
    m_resourceContext->getUniformBuffer("camera")->updateStagingBuffer(camParams);

    // Object transforms
    m_transformBuffer->update(camParams.V, camParams.P);
    m_lightSystem->update(m_cameraController->getCamera(), dt);
    m_skybox->updateTransforms(camParams.V, camParams.P);

    if (m_skinningData.skeleton.joints.empty())
    {
        return;
    }

    m_animation.updateJoints(m_skinningData.skeleton.joints, totalT);
    m_skinningData.skeleton.updateJointTransforms(m_skinningData.inverseBindTransforms);

    m_resourceContext->getStorageBuffer("jointMatrices")
        ->updateStagingBuffer(
            m_skinningData.skeleton.jointTransforms.data(),
            m_skinningData.skeleton.jointTransforms.size() * sizeof(glm::mat4));

    totalT += dt;
    if (totalT >= 2.0f)
        totalT = 0.0f;
}

void GltfViewerScene::render()
{
    m_renderGraph->clearCommandLists();
    m_renderGraph->buildCommandLists(m_renderNodes);
    m_renderGraph->addToCommandLists(m_skybox->getRenderNode());
    m_renderGraph->executeCommandLists();
}

void GltfViewerScene::renderGui()
{
    static std::vector<std::string> paths =
        enumerateDirectories(m_renderer->getAssetPaths().resourceDir / "glTFSamples/2.0");
    static int32_t selectedIdx{0};
    ImGui::Begin("Hi");
    if (ImGui::BeginListBox("GLTF Examples"))
    {
        for (int32_t i = 0; i < paths.size(); ++i)
        {
            const bool isSelected{selectedIdx == i};
            if (ImGui::Selectable(paths[i].c_str(), isSelected))
            {
                selectedIdx = i;
                m_renderer->finish();
                loadGltf(paths.at(selectedIdx));
            }

            if (isSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndListBox();
    }
    ImGui::End();
}

RenderNode* GltfViewerScene::createRenderNode(std::string id, bool hasTransform)
{
    if (!hasTransform)
    {
        return m_renderNodes.emplace(id, std::make_unique<RenderNode>()).first->second.get();
    }
    else
    {
        const uint32_t transformIndex{m_transformBuffer->getNextIndex()};
        return m_renderNodes.emplace(id, std::make_unique<RenderNode>(*m_transformBuffer, transformIndex))
            .first->second.get();
    }
}

void GltfViewerScene::createCommonTextures()
{
    constexpr float Anisotropy{16.0f};
    constexpr float MaxLod{9.0f};
    auto& imageCache = m_resourceContext->imageCache;
    imageCache.addSampler("nearestNeighbor", createNearestClampSampler(m_renderer->getDevice()));
    imageCache.addSampler("linearRepeat", createLinearRepeatSampler(m_renderer->getDevice(), Anisotropy));
    imageCache.addSampler("linearMipmap", createLinearClampSampler(m_renderer->getDevice(), Anisotropy, MaxLod));
    imageCache.addSampler("linearClamp", createLinearClampSampler(m_renderer->getDevice(), Anisotropy));
    addPbrTexturesToImageCache(createDefaultPbrTextureGroup(), "default", imageCache);

    m_resourceContext->createPipeline("pbrTex", "PbrTex.json", m_renderGraph->getRenderPass(ForwardLightingPass), 0);

    setEnvironmentMap("TableMountain");
    imageCache.addImageWithView("brdfLut", integrateBrdfLut(m_renderer));
    imageCache.addImageWithView("sheenLut", createSheenLookup(*m_renderer, m_renderer->getResourcesPath()));
}

void GltfViewerScene::setEnvironmentMap(const std::string& envMapName)
{
    const auto iblData{
        loadImageBasedLightingData(m_renderer->getResourcesPath() / "Textures/EnvironmentMaps" / envMapName)};

    auto equirectMap =
        createVulkanImage(*m_renderer, iblData.equirectangularEnvironmentMap, VK_FORMAT_R32G32B32A32_SFLOAT);
    auto [cubeMap, cubeMapView] =
        convertEquirectToCubeMap(m_renderer, equirectMap->createView(VK_IMAGE_VIEW_TYPE_2D), 1024);

    auto diffEnvMap =
        createVulkanCubeMap(*m_renderer, {iblData.diffuseIrradianceCubeMap}, VK_FORMAT_R32G32B32A32_SFLOAT);
    auto diffEnvView = diffEnvMap->createView(VK_IMAGE_VIEW_TYPE_CUBE);

    auto specEnvMap =
        createVulkanCubeMap(*m_renderer, iblData.specularReflectanceMapMipLevels, VK_FORMAT_R32G32B32A32_SFLOAT);
    auto specEnvView = specEnvMap->createView(VK_IMAGE_VIEW_TYPE_CUBE);

    auto& imageCache = m_resourceContext->imageCache;
    imageCache.addImageWithView("cubeMap", std::move(cubeMap), std::move(cubeMapView));
    imageCache.addImageWithView("diffEnvMap", std::move(diffEnvMap), std::move(diffEnvView));
    imageCache.addImageWithView("specEnvMap", std::move(specEnvMap), std::move(specEnvView));
}

void GltfViewerScene::loadGltf(const std::string& gltfAsset)
{
    const std::string gltfRelativePath{fmt::format("glTFSamples/2.0/{}/glTF/{}.gltf", gltfAsset, gltfAsset)};
    auto renderObjects =
        loadGltfModel(m_renderer->getResourcesPath() / gltfRelativePath, flatten(PbrVertexFormat)).unwrap();

    auto& renderObject = renderObjects.at(0);
    // renderObject.mesh.normalizeToUnitBox();
    renderObject.material.key = gltfAsset;

    m_cameraController->setTarget(renderObject.mesh.getBoundingBox().getCenter());
    m_cameraController->setOrientation(glm::pi<float>() * 0.25f, -glm::pi<float>() * 0.25f);
    m_cameraController->setDistance(glm::length(renderObject.mesh.getBoundingBox().getExtents()) * 2.0f);

    const std::string entityName{"gltfNode"};
    auto gltfNode = createRenderNode(entityName, true);
    gltfNode->transformPack->M = renderObject.transform;

    addPbrTexturesToImageCache(
        renderObject.material.textures, renderObject.material.key, m_resourceContext->imageCache);

    m_resourceContext->addGeometry(
        entityName, std::make_unique<Geometry>(*m_renderer, renderObject.mesh, PbrVertexFormat));
    gltfNode->geometry = m_resourceContext->getGeometry(entityName);
    gltfNode->pass(ForwardLightingPass).material = createPbrMaterial(
        entityName, renderObject.material.key, *m_resourceContext, renderObject.material.params, *m_transformBuffer);
    setPbrMaterialSceneParams(*gltfNode->pass(ForwardLightingPass).material, *m_resourceContext, *m_lightSystem);

    for (uint32_t c = 0; c < CascadeCount; ++c)
    {
        auto& subpass = gltfNode->pass(CsmPasses[c]);
        subpass.setGeometry(m_resourceContext->getGeometry(entityName), 0, 1);
        subpass.material = m_resourceContext->getMaterial("cascadedShadowMap" + std::to_string(c));
        CRISP_CHECK(subpass.material->getPipeline()->getVertexLayout().isSubsetOf(subpass.geometry->getVertexLayout()));
    }

    m_renderer->getDevice().flushDescriptorUpdates();

    const bool hasSkinning{!renderObject.skinningData.weights.empty()};
    if (hasSkinning)
    {
        m_skinningData = renderObject.skinningData;
        if (!renderObject.animations.empty())
            m_animation = renderObject.animations.at(0);

        const auto& restPositions = renderObject.mesh.getPositions();
        const uint32_t vertexCount = static_cast<uint32_t>(restPositions.size());
        const auto& data = renderObject.skinningData;
        auto restVertexBuffer = m_resourceContext->addStorageBuffer(
            "restPositions",
            std::make_unique<StorageBuffer>(
                m_renderer,
                restPositions.size() * sizeof(glm::vec3),
                0,
                BufferUpdatePolicy::Constant,
                restPositions.data()));
        auto weightsBuffer = m_resourceContext->addStorageBuffer(
            "weights",
            std::make_unique<StorageBuffer>(
                m_renderer,
                data.weights.size() * sizeof(glm::vec4),
                0,
                BufferUpdatePolicy::Constant,
                data.weights.data()));
        std::vector<glm::uvec4> wideIndices(data.indices.size());
        for (uint32_t i = 0; i < wideIndices.size(); ++i)
        {
            wideIndices[i] = data.indices[i];
        }
        auto indicesBuffer = m_resourceContext->addStorageBuffer(
            "indices",
            std::make_unique<StorageBuffer>(
                m_renderer,
                wideIndices.size() * sizeof(glm::uvec4),
                0,
                BufferUpdatePolicy::Constant,
                wideIndices.data()));
        auto jointMatrices = m_resourceContext->addStorageBuffer(
            "jointMatrices",
            std::make_unique<StorageBuffer>(
                m_renderer,
                data.skeleton.jointTransforms.size() * sizeof(glm::mat4),
                0,
                BufferUpdatePolicy::PerFrame,
                data.skeleton.jointTransforms.data()));
        auto& skinningPass = m_renderGraph->addComputePass("SkinningPass");
        skinningPass.workGroupSize = glm::ivec3(256, 1, 1);
        skinningPass.numWorkGroups = glm::ivec3((vertexCount - 1) / 256 + 1, 1, 1);
        skinningPass.pipeline = createSkinningPipeline(m_renderer, skinningPass.workGroupSize);
        skinningPass.material = std::make_unique<Material>(skinningPass.pipeline.get());
        skinningPass.material->writeDescriptor(0, 0, restVertexBuffer->getDescriptorInfo());
        skinningPass.material->writeDescriptor(0, 1, weightsBuffer->getDescriptorInfo());
        skinningPass.material->writeDescriptor(0, 2, indicesBuffer->getDescriptorInfo());
        skinningPass.material->writeDescriptor(0, 3, jointMatrices->getDescriptorInfo());
        skinningPass.material->writeDescriptor(
            0, 4, VkDescriptorBufferInfo{gltfNode->geometry->getVertexBuffer()->getHandle(), 0, VK_WHOLE_SIZE});

        skinningPass.preDispatchCallback =
            [gltfNode, vertexCount](RenderGraph::Node& node, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/)
        {
            VkBufferMemoryBarrier barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            barrier.buffer = gltfNode->geometry->getVertexBuffer()->getHandle();
            barrier.offset = 0;
            barrier.size = VK_WHOLE_SIZE;

            vkCmdPipelineBarrier(
                cmdBuffer.getHandle(),
                VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                0,
                nullptr,
                1,
                &barrier,
                0,
                nullptr);

            struct SkinningParams
            {
                uint32_t vertexCount;
                uint32_t jointsPerVertex;
            };
            SkinningParams params{vertexCount, SkinningData::JointsPerVertex};
            node.pipeline->setPushConstants(cmdBuffer.getHandle(), VK_SHADER_STAGE_COMPUTE_BIT, params);
        };

        m_renderGraph->addDependency(
            "SkinningPass",
            ForwardLightingPass,
            [gltfNode](const VulkanRenderPass& /*renderPass*/, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIdx*/)
            {
                VkBufferMemoryBarrier barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
                barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
                barrier.buffer = gltfNode->geometry->getVertexBuffer()->getHandle();
                barrier.offset = 0;
                barrier.size = VK_WHOLE_SIZE;

                vkCmdPipelineBarrier(
                    cmdBuffer.getHandle(),
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                    0,
                    0,
                    nullptr,
                    1,
                    &barrier,
                    0,
                    nullptr);
            });
    }
}

void GltfViewerScene::setupInput()
{
    m_connectionHandlers.emplace_back(m_app->getWindow()->keyPressed.subscribe(
        [this](Key key, int)
        {
            switch (key)
            {
            case Key::F5:
                m_resourceContext->recreatePipelines();
                break;
            }
        }));
}

} // namespace crisp