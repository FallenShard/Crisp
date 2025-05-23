#include <Crisp/Scenes/GltfViewerScene.hpp>

#include <Crisp/Core/Application.hpp>
#include <Crisp/Core/Checks.hpp>
#include <Crisp/Lights/EnvironmentLightIo.hpp>
#include <Crisp/Mesh/Io/MeshLoader.hpp>
#include <Crisp/Renderer/ComputePipeline.hpp>
#include <Crisp/Renderer/PipelineBuilder.hpp>
#include <Crisp/Renderer/PipelineLayoutBuilder.hpp>
#include <Crisp/Renderer/RenderPasses/ForwardLightingPass.hpp>
#include <Crisp/Renderer/RenderPasses/ShadowPass.hpp>
#include <Crisp/Renderer/VulkanImageUtils.hpp>

#include <imgui.h>

namespace crisp {
namespace {
const auto logger = createLoggerMt("GltfViewerScene");

constexpr uint32_t kShadowMapSize = 1024;

std::unique_ptr<VulkanPipeline> createSkinningPipeline(Renderer* renderer, const VkExtent3D& workGroupSize) {
    return createComputePipeline(
        *renderer, "linear-blend-skinning.comp", workGroupSize, [](PipelineLayoutBuilder& builder) {
            builder.setDescriptorDynamic(0, 3, true);
        });
}

void createDrawCommand(
    std::vector<DrawCommand>& drawCommands,
    const RenderNode& renderNode,
    const std::string_view renderPass,
    const uint32_t virtualFrameIndex) {
    if (!renderNode.isVisible) {
        return;
    }

    for (const auto& [key, materialMap] : renderNode.materials) {
        for (const auto& [part, material] : materialMap) {
            if (key.renderPassName == renderPass) {
                drawCommands.push_back(material.createDrawCommand(virtualFrameIndex, renderNode));
            }
        }
    }
}

} // namespace

GltfViewerScene::GltfViewerScene(Renderer* renderer, Window* window)
    : Scene(renderer, window) {
    setupInput();

    m_cameraController = std::make_unique<TargetCameraController>(*m_window);
    m_cameraController->setDistance(3.0f);
    m_resourceContext->createUniformBuffer("camera", sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);

    addCascadedShadowMapPasses(
        *m_rg, kShadowMapSize, [this](const RenderPassExecutionContext& ctx, const uint32_t cascadeIndex) {
            const uint32_t virtualFrameIndex = m_renderer->getCurrentVirtualFrameIndex();
            std::vector<DrawCommand> drawCommands{};
            for (const auto& [id, renderNode] : m_renderNodes) {
                createDrawCommand(drawCommands, *renderNode, kCsmPasses[cascadeIndex], virtualFrameIndex);
            }

            const VulkanCommandBuffer commandBuffer(ctx.cmdBuffer);
            for (const auto& drawCommand : drawCommands) {
                RenderGraph::executeDrawCommand(drawCommand, *m_renderer, commandBuffer, virtualFrameIndex);
            }
        });

    addForwardLightingPass(*m_rg, [this](const RenderPassExecutionContext& ctx) {
        const uint32_t virtualFrameIndex = m_renderer->getCurrentVirtualFrameIndex();
        std::vector<DrawCommand> drawCommands{};
        for (const auto& [id, renderNode] : m_renderNodes) {
            createDrawCommand(drawCommands, *renderNode, kForwardLightingPass, virtualFrameIndex);
        }

        createDrawCommand(drawCommands, m_skybox->getRenderNode(), kForwardLightingPass, virtualFrameIndex);

        const VulkanCommandBuffer commandBuffer(ctx.cmdBuffer);
        for (const auto& drawCommand : drawCommands) {
            RenderGraph::executeDrawCommand(drawCommand, *m_renderer, commandBuffer, virtualFrameIndex);
        }
    });

    m_renderer->enqueueResourceUpdate([this](const VkCommandBuffer cmdBuffer) {
        m_rg->compile(m_renderer->getDevice(), m_renderer->getSwapChainExtent(), cmdBuffer);

        const auto& data = m_rg->getBlackboard().get<ForwardLightingData>();
        m_sceneImageViews.resize(kRendererVirtualFrameCount);
        for (auto& sv : m_sceneImageViews) {
            sv = m_rg->createViewFromResource(m_renderer->getDevice(), data.hdrImage);
        }

        m_renderer->setSceneImageViews(m_sceneImageViews);
    });
    m_renderer->flushResourceUpdates(true);

    auto& imageCache = m_resourceContext->imageCache;
    imageCache.addImageView(
        "csmFrame0",
        createView(
            m_renderer->getDevice(),
            *m_resourceContext->renderTargetCache.get("ShadowMap")->image,
            VK_IMAGE_VIEW_TYPE_2D_ARRAY,
            0,
            kDefaultCascadeCount));
    imageCache.addImageView(
        "csmFrame1",
        createView(
            m_renderer->getDevice(),
            *m_resourceContext->renderTargetCache.get("ShadowMap")->image,
            VK_IMAGE_VIEW_TYPE_2D_ARRAY,
            kDefaultCascadeCount,
            kDefaultCascadeCount));

    m_lightSystem = std::make_unique<LightSystem>(
        m_renderer,
        DirectionalLight(-glm::vec3(1, 1, 0), glm::vec3(3.0f), glm::vec3(-5), glm::vec3(5)),
        kShadowMapSize,
        kDefaultCascadeCount);

    // Object transforms
    m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, 100);
    m_renderer->getDevice().setObjectName(m_transformBuffer->getUniformBuffer()->getHandle(), "transformBuffer");

    createCommonTextures();

    for (uint32_t i = 0; i < kDefaultCascadeCount; ++i) {
        std::string key = "cascadedShadowMap" + std::to_string(i);
        auto csmPipeline =
            m_resourceContext->createPipeline(key, "ShadowMap.json", m_rg->getRenderPass(kCsmPasses[i]), 0);
        auto csmMaterial = m_resourceContext->createMaterial(key, csmPipeline);
        csmMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
        csmMaterial->writeDescriptor(0, 1, *m_lightSystem->getCascadedDirectionalLightBuffer(i));
    }

    loadGltf("CesiumMan");

    m_skybox = m_lightSystem->getEnvironmentLight()->createSkybox(
        *m_renderer, m_rg->getRenderPass(kForwardLightingPass), m_resourceContext->imageCache.getSampler("linearClamp"));

    m_renderer->getDevice().flushDescriptorUpdates();
}

void GltfViewerScene::resize(int width, int height) {
    m_cameraController->onViewportResized(width, height);

    m_renderer->enqueueResourceUpdate([this](const VkCommandBuffer cmdBuffer) {
        m_rg->resize(m_renderer->getDevice(), m_renderer->getSwapChainExtent(), cmdBuffer);
        const auto& imageCache = m_resourceContext->imageCache;
        for (auto&& [name, node] : m_renderNodes) {
            auto& material = node->pass(kForwardLightingPass).material;
            for (uint32_t i = 0; i < kDefaultCascadeCount; ++i) {
                for (uint32_t k = 0; k < kRendererVirtualFrameCount; ++k) {
                    const auto& shadowMapView{m_rg->getRenderPass(kCsmPasses[i]).getAttachmentView(0, k)};
                    material->writeDescriptor(1, 6, k, i, shadowMapView, &imageCache.getSampler("nearestNeighbor"));
                }
            }
        }

        const auto& data = m_rg->getBlackboard().get<ForwardLightingData>();
        m_sceneImageViews.resize(kRendererVirtualFrameCount);
        for (auto& sv : m_sceneImageViews) {
            sv = m_rg->createViewFromResource(m_renderer->getDevice(), data.hdrImage);
        }

        m_renderer->setSceneImageViews(m_sceneImageViews);
    });
    m_renderer->flushResourceUpdates(true);
}

void GltfViewerScene::update(float dt) {
    static float totalT = 0.0f;

    // Camera
    m_cameraController->update(dt);
    const auto camParams = m_cameraController->getCameraParameters();
    m_resourceContext->getUniformBuffer("camera")->updateStagingBuffer2(camParams);

    // Object transforms
    m_transformBuffer->update(camParams.V, camParams.P);
    m_lightSystem->update(m_cameraController->getCamera(), dt);
    m_skybox->updateTransforms(camParams.V, camParams.P);

    if (m_skinningData.skeleton.joints.empty()) {
        return;
    }

    m_animation.updateJoints(m_skinningData.skeleton.joints, totalT);
    m_skinningData.skeleton.updateJointTransforms(m_skinningData.inverseBindTransforms);

    m_resourceContext->getRingBuffer("jointMatrices")
        ->updateStagingBuffer(
            {
                .data = m_skinningData.skeleton.jointTransforms.data(),
                .size = m_skinningData.skeleton.jointTransforms.size() * sizeof(glm::mat4),
            },
            m_renderer->getCurrentVirtualFrameIndex());

    totalT += dt;
    if (totalT >= 2.0f) {
        totalT = 0.0f;
    }
}

void GltfViewerScene::render() {
    m_renderer->enqueueDrawCommand([this](VkCommandBuffer cmdBuffer) {
        m_rg->execute(cmdBuffer, m_renderer->getCurrentVirtualFrameIndex());
    });
}

void GltfViewerScene::renderGui() {
    static std::vector<std::string> paths =
        enumerateDirectories(m_renderer->getAssetPaths().resourceDir / "glTFSamples/2.0");
    static int32_t selectedIdx{0};
    ImGui::Begin("Hi");
    ImGui::Text("GLTF Examples"); // NOLINT
    if (ImGui::BeginListBox("##", ImVec2(0, 500))) {
        for (int32_t i = 0; i < std::ssize(paths); ++i) {
            const bool isSelected{selectedIdx == i};
            if (ImGui::Selectable(paths[i].c_str(), isSelected)) {
                selectedIdx = i;
                m_renderer->finish();
                loadGltf(paths.at(selectedIdx));
            }

            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndListBox();
    }
    ImGui::End();
}

RenderNode* GltfViewerScene::createRenderNode(std::string id, bool hasTransform) {
    if (!hasTransform) {
        return m_renderNodes.emplace(id, std::make_unique<RenderNode>()).first->second.get();
    }
    const auto transformIndex{m_transformBuffer->getNextIndex()};
    return m_renderNodes.emplace(id, std::make_unique<RenderNode>(*m_transformBuffer, transformIndex))
        .first->second.get();
}

void GltfViewerScene::createCommonTextures() {
    constexpr float Anisotropy{16.0f};
    constexpr float MaxLod{9.0f};
    auto& imageCache = m_resourceContext->imageCache;
    imageCache.addSampler("nearestNeighbor", createNearestClampSampler(m_renderer->getDevice()));
    imageCache.addSampler("linearRepeat", createLinearRepeatSampler(m_renderer->getDevice(), Anisotropy));
    imageCache.addSampler("linearMipmap", createLinearClampSampler(m_renderer->getDevice(), Anisotropy, MaxLod));
    imageCache.addSampler("linearClamp", createLinearClampSampler(m_renderer->getDevice(), Anisotropy));
    addPbrImageGroupToImageCache(createDefaultPbrImageGroup(), imageCache);

    m_resourceContext->createPipeline("pbrTex", "PbrTex.json", m_rg->getRenderPass(kForwardLightingPass), 0);

    const std::string environmentMap = "TableMountain";
    m_lightSystem->setEnvironmentMap(
        loadImageBasedLightingData(m_renderer->getResourcesPath() / "Textures/EnvironmentMaps" / environmentMap).unwrap(),
        environmentMap);
    imageCache.addImageWithView("brdfLut", integrateBrdfLut(m_renderer));
    imageCache.addImageWithView("sheenLut", createSheenLookup(*m_renderer, m_renderer->getResourcesPath()));
}

void GltfViewerScene::loadGltf(const std::string& gltfAsset) {
    const std::string gltfRelativePath{fmt::format("glTFSamples/2.0/{}/glTF/{}.gltf", gltfAsset, gltfAsset)};
    auto [images, renderObjects] = loadGltfAsset(m_renderer->getResourcesPath() / gltfRelativePath).unwrap();

    auto& renderObject = renderObjects.at(0);
    // renderObject.material.name = gltfAsset;

    // m_cameraController->setTarget(renderObject.mesh.getBoundingBox().getCenter());
    // // m_cameraController->setOrientation(glm::pi<float>() * 0.25f, -glm::pi<float>() * 0.25f);
    // m_cameraController->setDistance(glm::length(renderObject.mesh.getBoundingBox().getExtents()) * 2.0f);

    const std::string entityName{"gltfNode"};
    auto gltfNode = createRenderNode(entityName, true);
    // gltfNode->transformPack->M = renderObject.transform;

    // addPbrTexturesToImageCache(
    //     renderObject.material.textures, renderObject.material.name, m_resourceContext->imageCache);

    // m_resourceContext->addGeometry(entityName, createGeometry(*m_renderer, renderObject.mesh, kPbrVertexFormat));
    // gltfNode->geometry = m_resourceContext->getGeometry(entityName);
    // gltfNode->pass(kForwardLightingPass).material = createPbrMaterial(
    //     entityName, renderObject.material.name, *m_resourceContext, renderObject.material.params,
    //     *m_transformBuffer);
    // setPbrMaterialSceneParams(*gltfNode->pass(kForwardLightingPass).material, *m_resourceContext, *m_lightSystem);

    for (uint32_t c = 0; c < kDefaultCascadeCount; ++c) {
        auto& subpass = gltfNode->pass(kCsmPasses[c]);
        subpass.setGeometry(m_resourceContext->getGeometry(entityName), 0, 1);
        subpass.material = m_resourceContext->getMaterial("cascadedShadowMap" + std::to_string(c));
        CRISP_CHECK(subpass.material->getPipeline()->getVertexLayout().isSubsetOf(subpass.geometry->getVertexLayout()));
    }

    m_renderer->getDevice().flushDescriptorUpdates();

    const bool hasSkinning{renderObject.mesh.hasCustomAttribute("weights0")};
    if (hasSkinning) {
        m_skinningData = renderObject.skinningData;
        if (!renderObject.animations.empty()) {
            m_animation = renderObject.animations.at(0);
        }

        const auto& restPositions = renderObject.mesh.getPositions();
        const auto vertexCount = static_cast<uint32_t>(restPositions.size());
        auto restVertexBuffer =
            m_resourceContext->addBuffer("restPositions", createStorageBuffer(m_renderer->getDevice(), restPositions));
        const auto& weightsData = renderObject.mesh.getCustomAttribute("weights0");
        auto weightsBuffer =
            m_resourceContext->addBuffer("weights", createStorageBuffer(m_renderer->getDevice(), weightsData.buffer));
        const auto& jointIndices = renderObject.mesh.getCustomAttribute("indices0");
        auto indicesBuffer =
            m_resourceContext->addBuffer("indices", createStorageBuffer(m_renderer->getDevice(), jointIndices.buffer));
        auto jointMatrices = m_resourceContext->addBuffer(
            "jointMatrices", createStorageBuffer(m_renderer->getDevice(), m_skinningData.skeleton.jointTransforms));
        auto& skinningPass = m_renderGraph->addComputePass("SkinningPass");
        skinningPass.workGroupSize = {256, 1, 1};
        skinningPass.numWorkGroups = {(vertexCount + 256 - 1) / 256, 1, 1};
        skinningPass.pipeline = createSkinningPipeline(m_renderer, skinningPass.workGroupSize);
        skinningPass.material = std::make_unique<Material>(skinningPass.pipeline.get());
        skinningPass.material->writeDescriptor(0, 0, *restVertexBuffer);
        skinningPass.material->writeDescriptor(0, 1, *weightsBuffer);
        skinningPass.material->writeDescriptor(0, 2, *indicesBuffer);
        skinningPass.material->writeDescriptor(0, 3, *jointMatrices);
        skinningPass.material->writeDescriptor(
            0, 4, VkDescriptorBufferInfo{gltfNode->geometry->getVertexBuffer()->getHandle(), 0, VK_WHOLE_SIZE});

        skinningPass.preDispatchCallback =
            [gltfNode, vertexCount](RenderGraph::Node& node, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/) {
                cmdBuffer.insertBufferMemoryBarrier(
                    gltfNode->geometry->getVertexBuffer()->createDescriptorInfo(),
                    VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                    VK_ACCESS_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

                struct SkinningParams {
                    uint32_t vertexCount;
                    uint32_t jointsPerVertex;
                };
                SkinningParams params{vertexCount, SkinningData::JointsPerVertex};
                node.pipeline->setPushConstants(cmdBuffer.getHandle(), VK_SHADER_STAGE_COMPUTE_BIT, params);
            };

        m_renderGraph->addDependency(
            "SkinningPass",
            kForwardLightingPass,
            [gltfNode](const VulkanRenderPass& /*renderPass*/, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIdx*/) {
                cmdBuffer.insertBufferMemoryBarrier(
                    gltfNode->geometry->getVertexBuffer()->createDescriptorInfo(),
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_ACCESS_SHADER_WRITE_BIT,
                    VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                    VK_ACCESS_SHADER_READ_BIT);
            });
    }
}

void GltfViewerScene::setupInput() {
    m_connectionHandlers.emplace_back(m_window->keyPressed.subscribe([this](Key key, int) {
        switch (key) {
        case Key::F5:
            m_resourceContext->recreatePipelines();
            break;
        default: {
        }
        }
    }));
}

} // namespace crisp