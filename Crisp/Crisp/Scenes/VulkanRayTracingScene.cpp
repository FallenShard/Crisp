#include "Crisp/Renderer/RayTracingPipelineBuilder.hpp"
#include <Crisp/Scenes/VulkanRayTracingScene.hpp>

#include <Crisp/Mesh/Io/MeshLoader.hpp>
#include <Crisp/Renderer/PipelineLayoutBuilder.hpp>
#include <Crisp/ShaderUtils/ShaderType.hpp>

#include <Crisp/GUI/ImGuiUtils.hpp>

namespace crisp {
namespace {
constexpr const char* MainPass = "mainPass";

const VertexLayoutDescription posFormat = {{VertexAttribute::Position, VertexAttribute::Normal}};

std::unique_ptr<Geometry> createRayTracingGeometry(
    Renderer& renderer, const std::filesystem::path& relativePath, const glm::mat4& transform = glm::mat4(1.0f)) {
    auto mesh{loadTriangleMesh(renderer.getResourcesPath() / relativePath, flatten(posFormat)).unwrap()};
    mesh.transform(transform);
    return std::make_unique<Geometry>(
        renderer,
        std::move(mesh),
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
    m_cameraController->setPosition(0.0f, 0.919769f, 5.41159f);
    m_cameraController->setFovY(27.7856f);
    m_resourceContext->createUniformBuffer("camera", sizeof(ExtendedCameraParameters), BufferUpdatePolicy::PerFrame);

    m_integratorParams = {.maxBounces = 5};
    m_resourceContext->createUniformBuffer("integrator", sizeof(IntegratorParameters), BufferUpdatePolicy::PerFrame);

    const std::vector<std::string> meshNames = {"walls", "leftwall", "rightwall", "light", "sphere", "sphere"};

    std::vector<VulkanAccelerationStructure*> blases;
    for (auto&& [idx, meshName] : std::views::enumerate(meshNames)) {
        const std::filesystem::path relativePath = std::filesystem::path("Meshes") / (meshName + ".obj");

        glm::mat4 transform = glm::mat4(1.0f);
        if (idx == 4) {
            transform = glm::translate(glm::vec3(0.445800, 0.332100, 0.376700)) * glm::scale(glm::vec3(0.3263f));
        } else if (idx == 5) {
            transform = glm::translate(glm::vec3(-0.421400, 0.332100, -0.280000)) * glm::scale(glm::vec3(0.3263f));
        }

        auto& geometry = m_resourceContext->addGeometry(
            fmt::format("{}_{}", meshName, idx), createRayTracingGeometry(*m_renderer, relativePath, transform));
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

    for (uint32_t i = 0; i < RendererConfig::VirtualFrameCount; ++i) {
        m_rtImageViews.emplace_back(createView(*m_rtImage, VK_IMAGE_VIEW_TYPE_2D, 0, 1));
    }

    m_pipeline = createPipeline();

    m_material = std::make_unique<Material>(m_pipeline.get());

    updateDescriptorSets();
    for (auto&& [idx, name] : std::views::enumerate(meshNames)) {
        updateGeometryBufferDescriptors(
            *m_resourceContext->getGeometry(fmt::format("{}_{}", name, idx)), static_cast<uint32_t>(idx));
    }

    m_renderer->getDevice().flushDescriptorUpdates();

    m_renderer->setSceneImageViews(m_rtImageViews);
} // namespace crisp

void VulkanRayTracingScene::resize(int /*width*/, int /*height*/) {
    /* m_cameraController->onViewportResized(width, height);

     m_renderGraph->resize(width, height);
     m_renderer->setSceneImageView(m_renderGraph->getNode(MainPass).renderPass.get(),
     0);*/
}

void VulkanRayTracingScene::update(float dt) {
    if (m_cameraController->update(dt)) {
        m_integratorParams.frameIdx = 0;
    }

    const ExtendedCameraParameters cameraParams = m_cameraController->getExtendedCameraParameters();
    m_resourceContext->getUniformBuffer("camera")->updateStagingBuffer(cameraParams);
    m_resourceContext->getUniformBuffer("integrator")->updateStagingBuffer(m_integratorParams);
}

void VulkanRayTracingScene::render() {
    m_renderer->enqueueDrawCommand([this](VkCommandBuffer cmdBuffer) {
        m_rtImage->transitionLayout(
            cmdBuffer,
            VK_IMAGE_LAYOUT_GENERAL,
            0,
            1,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

        m_pipeline->bind(cmdBuffer);
        m_material->bind(m_renderer->getCurrentVirtualFrameIndex(), cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);

        const auto extent = m_renderer->getSwapChainExtent();
        vkCmdTraceRaysKHR(cmdBuffer, &m_sbt.rgen, &m_sbt.miss, &m_sbt.hit, &m_sbt.call, extent.width, extent.height, 1);

        m_rtImage->transitionLayout(
            cmdBuffer,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            0,
            1,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        m_integratorParams.frameIdx++;
    });
}

void VulkanRayTracingScene::renderGui() {
    ImGui::Begin("Integrator");
    if (ImGui::InputInt("Max Bounces", &m_integratorParams.maxBounces)) {
        m_integratorParams.frameIdx = 0;
    }
    if (ImGui::InputInt("Samples per Frame", &m_integratorParams.sampleCount)) {
        m_integratorParams.frameIdx = 0;
    }
    ImGui::End();
}

RenderNode* VulkanRayTracingScene::createRenderNode(std::string id, int transformIndex) {
    const TransformHandle handle{{{static_cast<uint16_t>(transformIndex), 0}}};
    auto node = std::make_unique<RenderNode>(*m_transformBuffer, handle);
    return m_renderNodes.emplace(id, std::move(node)).first->second.get();
}

std::unique_ptr<VulkanPipeline> VulkanRayTracingScene::createPipeline() {
    std::vector<std::string> shaderNames{
        "path-trace.rgen",
        "path-trace.rmiss",
        "path-trace.rchit",
        "path-trace-lambertian.rcall",
        "path-trace-dielectric.rcall",
    };
    std::vector<std::filesystem::path> shaderSpvPaths;
    shaderSpvPaths.reserve(shaderNames.size());
    for (const auto& name : shaderNames) {
        shaderSpvPaths.emplace_back(m_renderer->getAssetPaths().getSpvShaderPath(name));
    }
    PipelineLayoutBuilder builder{reflectUniformMetadataFromSpirvPaths(shaderSpvPaths).unwrap()};
    builder.setDescriptorSetBuffering(0, true);
    builder.setDescriptorDynamic(0, 2, true);
    builder.setDescriptorDynamic(0, 3, true);
    auto pipelineLayout = builder.create(m_renderer->getDevice());

    RayTracingPipelineBuilder pipelineBuilder(*m_renderer);
    for (const auto& shaderName : shaderNames) {
        pipelineBuilder.addShaderStage(shaderName);
    }
    pipelineBuilder.addShaderGroup(0, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR);
    pipelineBuilder.addShaderGroup(1, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR);
    pipelineBuilder.addShaderGroup(2, VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR);
    pipelineBuilder.addShaderGroup(3, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR);
    pipelineBuilder.addShaderGroup(4, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR);

    const VkPipeline pipeline{pipelineBuilder.createHandle(pipelineLayout->getHandle())};

    const uint32_t baseAlignment =
        m_renderer->getPhysicalDevice().getRayTracingPipelineProperties().shaderGroupBaseAlignment;

    m_sbt.buffer = pipelineBuilder.createShaderBindingTable(pipeline);
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
    m_sbt.call.size = baseAlignment * 2;
    m_sbt.call.stride = baseAlignment;

    return std::make_unique<VulkanPipeline>(
        m_renderer->getDevice(),
        pipeline,
        std::move(pipelineLayout),
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        VertexLayout{});
}

void VulkanRayTracingScene::updateDescriptorSets() {
    m_material->writeDescriptor(0, 0, m_topLevelAccelStructure->getDescriptorInfo());
    m_material->writeDescriptor(0, 1, m_rtImageViews, nullptr, VK_IMAGE_LAYOUT_GENERAL);
    m_material->writeDescriptor(0, 2, *m_resourceContext->getUniformBuffer("camera"));
    m_material->writeDescriptor(0, 3, *m_resourceContext->getUniformBuffer("integrator"));
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
        default: {
        }
        }
    };
}

} // namespace crisp