#include <Crisp/Scenes/VulkanRayTracingScene.hpp>

#include <Crisp/Mesh/Io/MeshLoader.hpp>
#include <Crisp/Renderer/PipelineLayoutBuilder.hpp>

namespace crisp {
namespace {
constexpr const char* MainPass = "mainPass";

const VertexLayoutDescription posFormat = {{VertexAttribute::Position, VertexAttribute::Normal}};

std::unique_ptr<Geometry> createRayTracingGeometry(Renderer& renderer, const std::filesystem::path& relativePath) {
    return std::make_unique<Geometry>(
        renderer,
        loadTriangleMesh(renderer.getResourcesPath() / relativePath, flatten(posFormat)).unwrap(),
        posFormat,
        /*padToVec4=*/false,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
}

VkAccelerationStructureGeometryKHR createAccelerationStructureGeometry(const Geometry& geometry) {
    VkAccelerationStructureGeometryKHR geo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    geo.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geo.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geo.geometry = {};
    geo.geometry.triangles = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
    geo.geometry.triangles.vertexData.deviceAddress = geometry.getVertexBuffer()->getDeviceAddress();
    geo.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT; // Positions format.
    geo.geometry.triangles.vertexStride = 2 * sizeof(glm::vec3);      // Spacing between positions.
    geo.geometry.triangles.maxVertex = geometry.getVertexCount() - 1;
    geo.geometry.triangles.indexData.deviceAddress = geometry.getIndexBuffer()->getDeviceAddress();
    geo.geometry.triangles.indexType = geometry.getIndexType();
    return geo;
}

} // namespace

VulkanRayTracingScene::VulkanRayTracingScene(Renderer* renderer, Window* window)
    : Scene(renderer, window) {
    setupInput();

    // Camera
    m_cameraController = std::make_unique<FreeCameraController>(*m_window);
    m_resourceContext->createUniformBuffer("camera", sizeof(ExtendedCameraParameters), BufferUpdatePolicy::PerFrame);

    std::vector<std::string> meshNames = {"walls", "leftwall", "rightwall", "light"};

    std::vector<VulkanAccelerationStructure*> blases;
    for (const auto& meshName : meshNames) {
        const std::filesystem::path relativePath = std::filesystem::path("Meshes") / (meshName + ".obj");

        auto& geometry = m_resourceContext->addGeometry(meshName, createRayTracingGeometry(*m_renderer, relativePath));
        m_bottomLevelAccelStructures.push_back(std::make_unique<VulkanAccelerationStructure>(
            m_renderer->getDevice(),
            createAccelerationStructureGeometry(geometry),
            geometry.getIndexCount() / 3,
            glm::mat4(1.0f)));

        blases.push_back(m_bottomLevelAccelStructures.back().get());
    }
    m_topLevelAccelStructure = std::make_unique<VulkanAccelerationStructure>(m_renderer->getDevice(), blases);

    m_renderer->enqueueResourceUpdate([this](VkCommandBuffer cmdBuffer) {
        std::vector<VulkanAccelerationStructure*> blases;
        for (auto& blas : m_bottomLevelAccelStructures) {
            blas->build(m_renderer->getDevice(), cmdBuffer, {});
            blases.push_back(blas.get());
        }

        m_topLevelAccelStructure->build(m_renderer->getDevice(), cmdBuffer, blases);
    });

    VkImageCreateInfo createInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    createInfo.flags = 0;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    createInfo.extent = m_renderer->getSwapChainExtent3D();
    createInfo.mipLevels = 1;
    createInfo.arrayLayers = 1;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_rtImage = std::make_unique<VulkanImage>(m_renderer->getDevice(), createInfo);

    std::array<VkImageView, RendererConfig::VirtualFrameCount> rtImageViewHandles;
    for (uint32_t i = 0; i < RendererConfig::VirtualFrameCount; ++i) {
        m_rtImageViews.emplace_back(createView(*m_rtImage, VK_IMAGE_VIEW_TYPE_2D, 0, 1));
        rtImageViewHandles[i] = m_rtImageViews.back()->getHandle();
    }

    m_pipeline = createPipeline(createPipelineLayout());

    m_material = std::make_unique<Material>(m_pipeline.get());

    updateDescriptorSets();
    updateGeometryBufferDescriptors(*m_resourceContext->getGeometry("walls"), 0);
    updateGeometryBufferDescriptors(*m_resourceContext->getGeometry("leftwall"), 1);
    updateGeometryBufferDescriptors(*m_resourceContext->getGeometry("rightwall"), 2);
    updateGeometryBufferDescriptors(*m_resourceContext->getGeometry("light"), 3);

    m_renderer->getDevice().flushDescriptorUpdates();

    m_renderer->setSceneImageViews(m_rtImageViews);
}

void VulkanRayTracingScene::resize(int /*width*/, int /*height*/) {
    /* m_cameraController->onViewportResized(width, height);

     m_renderGraph->resize(width, height);
     m_renderer->setSceneImageView(m_renderGraph->getNode(MainPass).renderPass.get(), 0);*/
}

void VulkanRayTracingScene::update(float dt) {
    if (m_cameraController->update(dt)) {
        m_frameIdx = 0;
    }

    const ExtendedCameraParameters cameraParams = m_cameraController->getExtendedCameraParameters();
    m_resourceContext->getUniformBuffer("camera")->updateStagingBuffer(cameraParams);
}

void VulkanRayTracingScene::render() {
    m_renderer->enqueueDrawCommand([this](VkCommandBuffer cmdBuffer) {
        const auto virtualFrameIndex = m_renderer->getCurrentVirtualFrameIndex();

        m_rtImage->transitionLayout(
            cmdBuffer,
            VK_IMAGE_LAYOUT_GENERAL,
            0,
            1,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

        m_pipeline->bind(cmdBuffer);
        m_material->bind(virtualFrameIndex, cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);

        m_pipeline->setPushConstant(
            cmdBuffer, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0, m_frameIdx);

        const auto extent = m_renderer->getSwapChainExtent();
        vkCmdTraceRaysKHR(cmdBuffer, &m_sbt.rgen, &m_sbt.miss, &m_sbt.hit, &m_sbt.call, extent.width, extent.height, 1);

        m_rtImage->transitionLayout(
            cmdBuffer,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            0,
            1,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        m_frameIdx++;
    });
}

RenderNode* VulkanRayTracingScene::createRenderNode(std::string id, int transformIndex) {
    const TransformHandle handle{{{static_cast<uint16_t>(transformIndex), 0}}};
    auto node = std::make_unique<RenderNode>(*m_transformBuffer, handle);
    m_renderNodes.emplace(id, std::move(node));
    return m_renderNodes.at(id).get();
}

std::unique_ptr<VulkanPipelineLayout> VulkanRayTracingScene::createPipelineLayout() {
    PipelineLayoutBuilder builder{};
    builder
        .defineDescriptorSet(
            0,
            true,
            {
                {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
                {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
                {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
            })
        .defineDescriptorSet(
            1,
            false,
            {
                {0,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                 4,
                 VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
                {1,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                 4,
                 VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
            })
        .addPushConstant(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, sizeof(uint32_t));

    return builder.create(m_renderer->getDevice());
}

std::unique_ptr<VulkanPipeline> VulkanRayTracingScene::createPipeline(
    std::unique_ptr<VulkanPipelineLayout> pipelineLayout) {
    std::vector<VkPipelineShaderStageCreateInfo> stages(4, {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO});
    stages[0].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    stages[0].module = m_renderer->getOrLoadShaderModule("path-trace.rgen");
    stages[0].pName = "main";
    stages[1].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    stages[1].module = m_renderer->getOrLoadShaderModule("path-trace.rmiss");
    stages[1].pName = "main";
    stages[2].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    stages[2].module = m_renderer->getOrLoadShaderModule("path-trace.rchit");
    stages[2].pName = "main";
    stages[3].stage = VK_SHADER_STAGE_CALLABLE_BIT_KHR;
    stages[3].module = m_renderer->getOrLoadShaderModule("path-trace-lambertian.rcall");
    stages[3].pName = "main";

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups(
        4, {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR});
    for (auto& group : groups) {
        group.anyHitShader = VK_SHADER_UNUSED_KHR;
        group.closestHitShader = VK_SHADER_UNUSED_KHR;
        group.generalShader = VK_SHADER_UNUSED_KHR;
        group.intersectionShader = VK_SHADER_UNUSED_KHR;
    }
    groups[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[0].generalShader = 0;
    groups[1].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[1].generalShader = 1;
    groups[2].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    groups[2].closestHitShader = 2;
    groups[3].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[3].generalShader = 3;

    const VkDevice deviceHandle = m_renderer->getDevice().getHandle();
    VkRayTracingPipelineCreateInfoKHR raytracingCreateInfo = {VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
    raytracingCreateInfo.stageCount = static_cast<uint32_t>(stages.size());
    raytracingCreateInfo.pStages = stages.data();
    raytracingCreateInfo.groupCount = static_cast<uint32_t>(groups.size());
    raytracingCreateInfo.pGroups = groups.data();
    raytracingCreateInfo.layout = pipelineLayout->getHandle();
    raytracingCreateInfo.maxPipelineRayRecursionDepth = 1;
    VkPipeline pipeline;
    vkCreateRayTracingPipelinesKHR(deviceHandle, {}, nullptr, 1, &raytracingCreateInfo, nullptr, &pipeline);

    const uint32_t groupCount = static_cast<uint32_t>(groups.size());
    const uint32_t handleSize = m_renderer->getPhysicalDevice().getRayTracingPipelineProperties().shaderGroupHandleSize;
    const uint32_t baseAlignment =
        m_renderer->getPhysicalDevice().getRayTracingPipelineProperties().shaderGroupBaseAlignment;
    std::vector<uint8_t> shaderHandleStorage(groupCount * baseAlignment);

    vkGetRayTracingShaderGroupHandlesKHR(
        deviceHandle, pipeline, 0, groupCount, shaderHandleStorage.size(), shaderHandleStorage.data());

    for (int32_t i = groupCount - 1; i >= 0; --i) {
        memcpy(&shaderHandleStorage[i * baseAlignment], &shaderHandleStorage[i * handleSize], handleSize);
    }

    m_sbt.buffer = std::make_unique<VulkanBuffer>(
        m_renderer->getDevice(),
        shaderHandleStorage.size(),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    m_sbt.rgen.deviceAddress = m_sbt.buffer->getDeviceAddress();
    m_sbt.rgen.size = baseAlignment;
    m_sbt.rgen.stride = baseAlignment;
    m_sbt.miss.deviceAddress = m_sbt.buffer->getDeviceAddress() + baseAlignment;
    m_sbt.miss.size = baseAlignment;
    m_sbt.miss.stride = baseAlignment;
    m_sbt.hit.deviceAddress = m_sbt.buffer->getDeviceAddress() + 2 * baseAlignment;
    m_sbt.hit.size = baseAlignment;
    m_sbt.hit.stride = baseAlignment;
    m_sbt.call.deviceAddress = m_sbt.buffer->getDeviceAddress() + 3 * baseAlignment;
    m_sbt.call.size = baseAlignment;
    m_sbt.call.stride = baseAlignment;

    m_renderer->enqueueResourceUpdate(
        [this, deviceHandle, handleStorage = std::move(shaderHandleStorage)](VkCommandBuffer cmdBuffer) {
            vkCmdUpdateBuffer(cmdBuffer, m_sbt.buffer->getHandle(), 0, handleStorage.size(), handleStorage.data());
        });
    return std::make_unique<VulkanPipeline>(
        m_renderer->getDevice(),
        pipeline,
        std::move(pipelineLayout),
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        VertexLayout{});
}

void VulkanRayTracingScene::updateDescriptorSets() {
    const auto tlasInfo = m_topLevelAccelStructure->getDescriptorInfo();
    m_material->writeDescriptor(0, 0, tlasInfo);
    m_material->writeDescriptor(0, 1, m_rtImageViews, nullptr, VK_IMAGE_LAYOUT_GENERAL);
    m_material->writeDescriptor(0, 2, *m_resourceContext->getUniformBuffer("camera"));
    m_renderer->getDevice().flushDescriptorUpdates();
}

void VulkanRayTracingScene::updateGeometryBufferDescriptors(const Geometry& geometry, uint32_t idx) {
    m_material->writeDescriptor(1, 0, geometry.getVertexBuffer()->createDescriptorInfo(), idx);
    m_material->writeDescriptor(1, 1, geometry.getIndexBuffer()->createDescriptorInfo(), idx);
    m_renderer->getDevice().flushDescriptorUpdates();
}

void VulkanRayTracingScene::setupInput() {
    m_window->keyPressed += [this](Key key, int) {
        switch (key) {
        case Key::F5:
            m_resourceContext->recreatePipelines();
            break;
        }
    };
}

} // namespace crisp