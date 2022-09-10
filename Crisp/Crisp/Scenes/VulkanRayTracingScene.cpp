#include "VulkanRayTracingScene.hpp"

#include <Crisp/Core/Application.hpp>
#include <Crisp/Core/Window.hpp>
#include <Crisp/LuaConfig.hpp>

#include <Crisp/Camera/FreeCameraController.hpp>

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
#include <Crisp/Mesh/TriangleMeshUtils.hpp>
#include <Crisp/Models/Skybox.hpp>

#include <Crisp/Lights/DirectionalLight.hpp>
#include <Crisp/Lights/EnvironmentLighting.hpp>
#include <Crisp/Lights/LightSystem.hpp>

#include <Crisp/GUI/Button.hpp>
#include <Crisp/GUI/CheckBox.hpp>
#include <Crisp/GUI/ComboBox.hpp>
#include <Crisp/GUI/Form.hpp>
#include <Crisp/GUI/Label.hpp>
#include <Crisp/GUI/Slider.hpp>

#include <Crisp/Math/Constants.hpp>
#include <Crisp/Profiler.hpp>

#include <Crisp/Renderer/PipelineLayoutBuilder.hpp>

#include <Crisp/Vulkan/VulkanAccelerationStructure.hpp>
#include <Crisp/Vulkan/VulkanGetDeviceProc.hpp>

#include <Crisp/IO/MeshLoader.hpp>

#include <random>

namespace crisp
{
namespace
{
static constexpr const char* MainPass = "mainPass";

PFN_vkCmdTraceRaysKHR vkCmdTraceRays;
PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelines;
PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandles;

inline void initializeRayTracingApi(VkDevice device)
{
    static bool initialized = false;
    if (initialized)
        return;

    GET_DEVICE_PROC_KHR(vkCmdTraceRays);
    GET_DEVICE_PROC_KHR(vkCreateRayTracingPipelines);
    GET_DEVICE_PROC_KHR(vkGetRayTracingShaderGroupHandles);
    initialized = true;
}
} // namespace

VulkanRayTracingScene::VulkanRayTracingScene(Renderer* renderer, Application* app)
    : m_renderer(renderer)
    , m_app(app)
    , m_resourceContext(std::make_unique<ResourceContext>(renderer))
{
    setupInput();

    initializeRayTracingApi(renderer->getDevice().getHandle());

    // Camera
    m_cameraController = std::make_unique<FreeCameraController>(app->getWindow());
    m_resourceContext->createUniformBuffer("camera", sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);

    //// m_renderGraph = std::make_unique<RenderGraph>(m_renderer);

    ////// Main render pass
    //// m_renderGraph->addRenderPass(MainPass, std::make_unique<ForwardLightingPass>(m_renderer,
    /// VK_SAMPLE_COUNT_1_BIT));

    ////// Wrap-up render graph definition
    //// m_renderGraph->addRenderTargetLayoutTransition(MainPass, "SCREEN", 0);
    //// m_renderGraph->sortRenderPasses().unwrap();
    //// m_renderer->setSceneImageView(m_renderGraph->getNode(MainPass).renderPass.get(), 0);

    ///*m_lightSystem = std::make_unique<LightSystem>(m_renderer, ShadowMapSize);
    // m_lightSystem->setDirectionalLight(DirectionalLight(-glm::vec3(1, 1, 0), glm::vec3(3.0f), glm::vec3(-5),
    // glm::vec3(5)));*/

    //// Object transforms
    // m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, 100);

    //// createPlane();

    const std::vector<VertexAttributeDescriptor> posFormat = {VertexAttribute::Position, VertexAttribute::Normal};
    //// m_resourceContext->addGeometry("floorPos", std::make_unique<Geometry>(m_renderer,
    //// createPlaneMesh(posFormat, 10.0f)));
    m_resourceContext->addGeometry(
        "walls",
        std::make_unique<Geometry>(
            m_renderer,
            loadTriangleMesh(m_renderer->getResourcesPath() / "Meshes/walls.obj", posFormat).unwrap(),
            posFormat,
            false,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR));
    m_resourceContext->addGeometry(
        "leftwall",
        std::make_unique<Geometry>(
            m_renderer,
            loadTriangleMesh(m_renderer->getResourcesPath() / "Meshes/leftwall.obj", posFormat).unwrap(),
            posFormat,
            false,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR));
    m_resourceContext->addGeometry(
        "rightwall",
        std::make_unique<Geometry>(
            m_renderer,
            loadTriangleMesh(m_renderer->getResourcesPath() / "Meshes/rightwall.obj", posFormat).unwrap(),
            posFormat,
            false,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR));

    // const TriangleMesh lightMesh(loadTriangleMesh(m_renderer->getResourcesPath() / "Meshes/light.obj", posFormat));
    //// lightMesh.transform(glm::translate(glm::vec3(0.0f, -0.2f, 0.0f)));
    //// TriangleMesh lightMesh(m_renderer->getResourcesPath() / "Meshes/sphere.obj", posFormat);
    //// lightMesh.transform(glm::translate(glm::vec3(0.0f, +0.8f, 0.0f)) * glm::scale(glm::vec3(0.1f)));
    m_resourceContext->addGeometry(
        "light",
        std::make_unique<Geometry>(
            m_renderer,
            loadTriangleMesh(m_renderer->getResourcesPath() / "Meshes/light.obj", posFormat).unwrap(),
            posFormat,
            false,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR));

    //// TriangleMesh leftSphere(loadTriangleMesh(m_renderer->getResourcesPath() / "Meshes/sphere.obj", posFormat));
    //// leftSphere.transform(glm::translate(glm::vec3(-0.421400f, 0.332100f, -0.280000f)) *
    //// glm::scale(glm::vec3(0.3263f))); m_resourceContext->addGeometry("leftSphere",
    //// std::make_unique<Geometry>(m_renderer, leftSphere, posFormat, true, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
    //// TriangleMesh rightSphere(loadTriangleMesh(m_renderer->getResourcesPath() / "Meshes/Ocean3.obj", posFormat));
    //////rightSphere.transform(glm::translate(glm::vec3(0.445800, 0.332100, 0.376700)) *
    /// glm::scale(glm::vec3(0.3263))); / rightSphere.transform(glm::translate(glm::vec3(0.000f, 0.232100f, 0.000f)) *
    /// glm::scale(glm::vec3(0.5f)));// / *glm::scale(glm::vec3(0.3263))); m_resourceContext->addGeometry("rightSphere",
    //// std::make_unique<Geometry>(m_renderer, rightSphere, posFormat, true, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
    const auto createGeometry = [renderer](const Geometry& geometry)
    {
        VkAccelerationStructureGeometryKHR geo = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
        geo.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        geo.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geo.geometry = {};
        geo.geometry.triangles = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
        geo.geometry.triangles.vertexData.deviceAddress =
            renderer->getDevice().getBufferAddress(geometry.getVertexBuffer()->getHandle());
        geo.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        geo.geometry.triangles.vertexStride = 2 * sizeof(glm::vec3);
        geo.geometry.triangles.maxVertex = geometry.getVertexCount() - 1;
        geo.geometry.triangles.indexData.deviceAddress =
            renderer->getDevice().getBufferAddress(geometry.getIndexBuffer()->getHandle());
        geo.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
        return geo;
    };

    m_bottomLevelAccelStructures.push_back(std::make_unique<VulkanAccelerationStructure>(
        m_renderer->getDevice(),
        createGeometry(*m_resourceContext->getGeometry("walls")),
        m_resourceContext->getGeometry("walls")->getIndexCount() / 3,
        glm::mat4(1.0f)));
    m_bottomLevelAccelStructures.push_back(std::make_unique<VulkanAccelerationStructure>(
        m_renderer->getDevice(),
        createGeometry(*m_resourceContext->getGeometry("leftwall")),
        m_resourceContext->getGeometry("leftwall")->getIndexCount() / 3,
        glm ::mat4(1.0f)));
    m_bottomLevelAccelStructures.push_back(std::make_unique<VulkanAccelerationStructure>(
        m_renderer->getDevice(),
        createGeometry(*m_resourceContext->getGeometry("rightwall")),
        m_resourceContext->getGeometry("rightwall")->getIndexCount() / 3,
        glm ::mat4(1.0f)));
    m_bottomLevelAccelStructures.push_back(std::make_unique<VulkanAccelerationStructure>(
        m_renderer->getDevice(),
        createGeometry(*m_resourceContext->getGeometry("light")),
        m_resourceContext->getGeometry("light")->getIndexCount() / 3,
        glm ::mat4(1.0f)));

    std::vector<VulkanAccelerationStructure*> blases;
    for (auto& blas : m_bottomLevelAccelStructures)
    {
        blases.push_back(blas.get());
    }

    m_topLevelAccelStructure = std::make_unique<VulkanAccelerationStructure>(m_renderer->getDevice(), blases);

    std::default_random_engine eng;
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    std::vector<float> randData;
    for (int i = 0; i < 65536 / sizeof(float); ++i)
    {
        randData.push_back(dist(eng));
    }
    auto randBuffer = m_resourceContext->createUniformBuffer("random", randData, BufferUpdatePolicy::Constant);

    m_renderer->enqueueResourceUpdate(
        [this](VkCommandBuffer cmdBuffer)
        {
            std::vector<VulkanAccelerationStructure*> blases;
            for (auto& blas : m_bottomLevelAccelStructures)
            {
                blas->build(m_renderer->getDevice(), cmdBuffer, {});
                blases.push_back(blas.get());
            }

            m_topLevelAccelStructure->build(m_renderer->getDevice(), cmdBuffer, blases);
        });
    m_renderer->flushResourceUpdates(true);

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

    for (uint32_t i = 0; i < RendererConfig::VirtualFrameCount; ++i)
        m_rtImageViews.emplace_back(m_rtImage->createView(VK_IMAGE_VIEW_TYPE_2D, 0, 1));

    std::array<VkImageView, 2> rtImageViewHandles = {m_rtImageViews[0]->getHandle(), m_rtImageViews[1]->getHandle()};

    createPipeline(createPipelineLayout());

    m_renderer->getDevice().flushDescriptorUpdates();

    m_renderer->setSceneImageViews(m_rtImageViews);

    VkDescriptorBufferInfo idxBufferInfo = {};
    idxBufferInfo.buffer = randBuffer->get();
    idxBufferInfo.offset = 0;
    idxBufferInfo.range = VK_WHOLE_SIZE;
    VkWriteDescriptorSet writeIndexBuffer = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    writeIndexBuffer.dstSet = m_descSets[0][1];
    writeIndexBuffer.dstBinding = 2;
    writeIndexBuffer.descriptorCount = 1;
    writeIndexBuffer.dstArrayElement = 0;
    writeIndexBuffer.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeIndexBuffer.pBufferInfo = &idxBufferInfo;

    VkWriteDescriptorSet writeSets[] = {writeIndexBuffer};
    vkUpdateDescriptorSets(m_renderer->getDevice().getHandle(), 1, writeSets, 0, nullptr);

    updateGeometryBufferDescriptors(*m_resourceContext->getGeometry("walls"), 0);
    updateGeometryBufferDescriptors(*m_resourceContext->getGeometry("leftwall"), 1);
    updateGeometryBufferDescriptors(*m_resourceContext->getGeometry("rightwall"), 2);
    updateGeometryBufferDescriptors(*m_resourceContext->getGeometry("light"), 3);

    // createGui();
}

VulkanRayTracingScene::~VulkanRayTracingScene()
{
    m_renderer->setSceneImageView(nullptr, 0);
    m_app->getForm()->remove("normalMappingPanel");
}

void VulkanRayTracingScene::resize(int /*width*/, int /*height*/)
{
    /* m_cameraController->onViewportResized(width, height);

     m_renderGraph->resize(width, height);
     m_renderer->setSceneImageView(m_renderGraph->getNode(MainPass).renderPass.get(), 0);*/
}

void VulkanRayTracingScene::update(float dt)
{
    if (m_cameraController->update(dt))
    {
        m_frameIdx = 0;
    }
    const CameraParameters cameraParams = m_cameraController->getCameraParameters();
    //

    // m_transformBuffer->update(cameraParams.V, cameraParams.P);

    //// TODO
    //// m_lightSystem->update(m_cameraController->getCamera(), dt);

    m_resourceContext->getUniformBuffer("camera")->updateStagingBuffer(cameraParams);
}

void VulkanRayTracingScene::render()
{
    // m_renderGraph->clearCommandLists();
    // m_renderGraph->buildCommandLists(m_renderNodes);
    // m_renderGraph->executeCommandLists();

    m_renderer->enqueueDrawCommand(
        [this](VkCommandBuffer cmdBuffer)
        {
            // m_renderer->finish();

            const auto virtualFrameIndex = m_renderer->getCurrentVirtualFrameIndex();

            m_rtImage->transitionLayout(
                cmdBuffer,
                VK_IMAGE_LAYOUT_GENERAL,
                0,
                1,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

            const auto& cameraBuffer = *m_resourceContext->getUniformBuffer("camera");
            uint32_t offset = virtualFrameIndex * static_cast<uint32_t>(cameraBuffer.getDescriptorInfo().range);

            m_pipeline->bind(cmdBuffer);
            vkCmdBindDescriptorSets(
                cmdBuffer,
                VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                m_pipeline->getPipelineLayout()->getHandle(),
                0,
                2,
                m_descSets[virtualFrameIndex].data(),
                1,
                &offset);

            vkCmdPushConstants(
                cmdBuffer,
                m_pipeline->getPipelineLayout()->getHandle(),
                VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                0,
                sizeof(uint32_t),
                &m_frameIdx);

            // const VkDeviceSize progSize = m_rayTracingMaterial->getShaderGroupHandleSize();
            const VkDeviceSize baseOffset =
                m_renderer->getPhysicalDevice().getRayTracingPipelineProperties().shaderGroupBaseAlignment;
            const VkDeviceSize rayGenOffset = 0;
            const VkDeviceSize missOffset = baseOffset;
            const VkDeviceSize hitGroupOffset = 2 * baseOffset;

            const auto extent = m_renderer->getSwapChainExtent();
            const auto sbtBufferAddress = m_renderer->getDevice().getBufferAddress(m_sbtBuffer->getHandle());
            VkStridedDeviceAddressRegionKHR rayGen;
            rayGen.deviceAddress = sbtBufferAddress + rayGenOffset;
            rayGen.size = baseOffset;
            rayGen.stride = baseOffset;
            VkStridedDeviceAddressRegionKHR miss;
            miss.deviceAddress = sbtBufferAddress + missOffset;
            miss.size = baseOffset;
            miss.stride = baseOffset;
            VkStridedDeviceAddressRegionKHR hit;
            hit.deviceAddress = sbtBufferAddress + hitGroupOffset;
            hit.size = baseOffset;
            hit.stride = baseOffset;
            VkStridedDeviceAddressRegionKHR call{};

            vkCmdTraceRays(cmdBuffer, &rayGen, &miss, &hit, &call, extent.width, extent.height, 1);

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

RenderNode* VulkanRayTracingScene::createRenderNode(std::string id, int transformIndex)
{
    auto node = std::make_unique<RenderNode>(*m_transformBuffer, transformIndex);
    m_renderNodes.emplace(id, std::move(node));
    return m_renderNodes.at(id).get();
}

void VulkanRayTracingScene::createPlane()
{
    /*std::vector<VertexAttributeDescriptor> pbrVertexFormat = { VertexAttribute::Position, VertexAttribute::Normal,
    VertexAttribute::TexCoord, VertexAttribute::Tangent }; m_resourceContext->addGeometry("floor",
    std::make_unique<Geometry>(m_renderer, createPlaneMesh(pbrVertexFormat, 10.0f)));

    m_resourceContext->addSampler("linearRepeat", std::make_unique<VulkanSampler>(m_renderer->getDevice(),
    VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 16.0f, 12.0f));
    m_resourceContext->addImageWithView("normalMap", convertToVulkanImage(m_renderer, "path.jpg",
    VK_FORMAT_R8G8B8A8_UNORM)); m_resourceContext->addImageWithView("diffuseMap", convertToVulkanImage(m_renderer,
    "uv_rainbow.jpg", VK_FORMAT_R8G8B8A8_SRGB));

    VulkanPipeline* pipeline = m_resourceContext->createPipeline("normalMap", "NormalMap.lua",
    m_renderGraph->getRenderPass(MainPass), 0); Material* material = m_resourceContext->createMaterial("normalMap",
    pipeline); material->writeDescriptor(0, 0, *m_transformBuffer->getUniformBuffer()); material->writeDescriptor(0, 1,
    *m_resourceContext->getUniformBuffer("camera")); material->writeDescriptor(1, 0,
    *m_resourceContext->getImageView("normalMap"), m_resourceContext->getSampler("linearRepeat"));
    material->writeDescriptor(1, 1, *m_resourceContext->getImageView("diffuseMap"),
    m_resourceContext->getSampler("linearRepeat"));

    auto floor = createRenderNode("floor", 0);
    floor->transformPack->M = glm::rotate(glm::radians(0.0f), glm::vec3(1.0f, 0.0f, 0.0f)) *
    glm::scale(glm::vec3(1.0, 1.0f, 1.0f)); floor->geometry = m_resourceContext->getGeometry("floor");
    floor->pass(MainPass).material = material;



    m_renderer->getDevice().flushDescriptorUpdates();*/
}

std::unique_ptr<VulkanPipelineLayout> VulkanRayTracingScene::createPipelineLayout()
{
    auto device = m_renderer->getDevice().getHandle();

    PipelineLayoutBuilder builder{};
    builder
        .defineDescriptorSet(
            0,
            true,
            {
                {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
                {1,              VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
                {2,     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
    })
        .addPushConstant(VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_RAYGEN_BIT_NV, 0, sizeof(uint32_t))
        .defineDescriptorSet(
            1,
            false,
            {
                {0,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                 4,
                 VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV},
                {1,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                 4,
                 VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV},
                {2,
                 VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                 1,
                 VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV},
            });

    auto pipelineLayout = builder.create(m_renderer->getDevice());

    m_descSets.resize(RendererConfig::VirtualFrameCount, std::vector<VkDescriptorSet>(2));
    m_descSets[0][0] = pipelineLayout->allocateSet(0);
    m_descSets[0][1] = pipelineLayout->allocateSet(1);
    m_descSets[1][0] = pipelineLayout->allocateSet(0);
    m_descSets[1][1] = m_descSets[0][1];

    for (int i = 0; i < RendererConfig::VirtualFrameCount; ++i)
    {
        const auto tlasInfo = m_topLevelAccelStructure->getDescriptorInfo();
        VkWriteDescriptorSet writeAsBinding = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writeAsBinding.dstSet = m_descSets[i][0];
        writeAsBinding.pNext = &tlasInfo;
        writeAsBinding.dstBinding = 0;
        writeAsBinding.descriptorCount = 1;
        writeAsBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

        VkDescriptorImageInfo imageInfo;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo.imageView = m_rtImageViews[i]->getHandle();
        imageInfo.sampler = nullptr;
        VkWriteDescriptorSet writeImageBinding = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writeImageBinding.dstSet = m_descSets[i][0];
        writeImageBinding.dstBinding = 1;
        writeImageBinding.descriptorCount = 1;
        writeImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writeImageBinding.pImageInfo = &imageInfo;

        VkDescriptorBufferInfo bufferInfo = m_resourceContext->getUniformBuffer("camera")->getDescriptorInfo();
        VkWriteDescriptorSet writeCameraBinding = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writeCameraBinding.dstSet = m_descSets[i][0];
        writeCameraBinding.dstBinding = 2;
        writeCameraBinding.descriptorCount = 1;
        writeCameraBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        writeCameraBinding.pBufferInfo = &bufferInfo;

        VkWriteDescriptorSet writeSets[] = {writeAsBinding, writeImageBinding, writeCameraBinding};
        vkUpdateDescriptorSets(device, 3, writeSets, 0, nullptr);
    }

    return pipelineLayout;
}

void VulkanRayTracingScene::createPipeline(std::unique_ptr<VulkanPipelineLayout> pipelineLayout)
{
    std::vector<VkPipelineShaderStageCreateInfo> stages(3, {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO});
    stages[0].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    stages[0].module = m_renderer->loadShaderModule("path-trace.rgen");
    stages[0].pName = "main";
    stages[1].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    stages[1].module = m_renderer->loadShaderModule("path-trace.rmiss");
    stages[1].pName = "main";
    stages[2].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    stages[2].module = m_renderer->loadShaderModule("path-trace.rchit");
    stages[2].pName = "main";

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups(
        3, {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR});
    for (uint32_t i = 0; i < groups.size(); ++i)
    {
        groups[i].anyHitShader = VK_SHADER_UNUSED_KHR;
        groups[i].closestHitShader = VK_SHADER_UNUSED_KHR;
        groups[i].generalShader = VK_SHADER_UNUSED_KHR;
        groups[i].intersectionShader = VK_SHADER_UNUSED_KHR;
    }
    groups[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[0].generalShader = 0;
    groups[1].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[1].generalShader = 1;
    groups[2].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    groups[2].closestHitShader = 2;

    const VkDevice deviceHandle = m_renderer->getDevice().getHandle();
    VkRayTracingPipelineCreateInfoKHR raytracingCreateInfo = {VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
    raytracingCreateInfo.stageCount = static_cast<uint32_t>(stages.size());
    raytracingCreateInfo.pStages = stages.data();
    raytracingCreateInfo.groupCount = static_cast<uint32_t>(groups.size());
    raytracingCreateInfo.pGroups = groups.data();
    raytracingCreateInfo.layout = pipelineLayout->getHandle();
    raytracingCreateInfo.maxPipelineRayRecursionDepth = 1;
    VkPipeline pipeline;
    vkCreateRayTracingPipelines(deviceHandle, {}, nullptr, 1, &raytracingCreateInfo, nullptr, &pipeline);

    const uint32_t groupCount = static_cast<uint32_t>(groups.size());
    const uint32_t handleSize = m_renderer->getPhysicalDevice().getRayTracingPipelineProperties().shaderGroupHandleSize;
    const uint32_t baseAlignment =
        m_renderer->getPhysicalDevice().getRayTracingPipelineProperties().shaderGroupBaseAlignment;
    std::vector<uint8_t> shaderHandleStorage(groupCount * baseAlignment);

    vkGetRayTracingShaderGroupHandles(
        deviceHandle, pipeline, 0, groupCount, shaderHandleStorage.size(), shaderHandleStorage.data());

    for (int32_t i = groupCount - 1; i >= 0; --i)
    {
        memcpy(&shaderHandleStorage[i * baseAlignment], &shaderHandleStorage[i * handleSize], handleSize);
    }

    m_sbtBuffer = std::make_unique<VulkanBuffer>(
        m_renderer->getDevice(),
        shaderHandleStorage.size(),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    m_renderer->enqueueResourceUpdate(
        [&, this, deviceHandle, handleStorage = std::move(shaderHandleStorage)](VkCommandBuffer cmdBuffer)
        {
            vkCmdUpdateBuffer(cmdBuffer, m_sbtBuffer->getHandle(), 0, handleStorage.size(), handleStorage.data());
        });
    m_pipeline = std::make_unique<VulkanPipeline>(
        m_renderer->getDevice(), pipeline, std::move(pipelineLayout), PipelineDynamicStateFlags{});
    m_pipeline->setBindPoint(VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
}

void VulkanRayTracingScene::updateGeometryBufferDescriptors(const Geometry& geometry, uint32_t idx)
{
    auto* vtxBuffer = geometry.getVertexBuffer();
    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = vtxBuffer->getHandle();
    bufferInfo.offset = 0;
    bufferInfo.range = VK_WHOLE_SIZE;
    VkWriteDescriptorSet writeVertexBuffer = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    writeVertexBuffer.dstSet = m_descSets[0][1];
    writeVertexBuffer.dstBinding = 0;
    writeVertexBuffer.descriptorCount = 1;
    writeVertexBuffer.dstArrayElement = idx;
    writeVertexBuffer.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeVertexBuffer.pBufferInfo = &bufferInfo;

    auto* idxBuffer = geometry.getIndexBuffer();
    VkDescriptorBufferInfo idxBufferInfo = {};
    idxBufferInfo.buffer = idxBuffer->getHandle();
    idxBufferInfo.offset = 0;
    idxBufferInfo.range = VK_WHOLE_SIZE;
    VkWriteDescriptorSet writeIndexBuffer = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    writeIndexBuffer.dstSet = m_descSets[0][1];
    writeIndexBuffer.dstBinding = 1;
    writeIndexBuffer.descriptorCount = 1;
    writeIndexBuffer.dstArrayElement = idx;
    writeIndexBuffer.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeIndexBuffer.pBufferInfo = &idxBufferInfo;

    VkWriteDescriptorSet writeSets[] = {writeVertexBuffer, writeIndexBuffer};
    vkUpdateDescriptorSets(m_renderer->getDevice().getHandle(), 2, writeSets, 0, nullptr);
}

//
// void RayTracingMaterial::setRandomBuffer(const UniformBuffer& /*randBuffer*/)
//{
//     /*VkDescriptorBufferInfo idxBufferInfo = {};
//     idxBufferInfo.buffer = randBuffer.get();
//     idxBufferInfo.offset = 0;
//     idxBufferInfo.range = VK_WHOLE_SIZE;
//     VkWriteDescriptorSet writeIndexBuffer = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
//     writeIndexBuffer.dstSet = m_descSets[1];
//     writeIndexBuffer.dstBinding = 2;
//     writeIndexBuffer.descriptorCount = 1;
//     writeIndexBuffer.dstArrayElement = 0;
//     writeIndexBuffer.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
//     writeIndexBuffer.pBufferInfo = &idxBufferInfo;
//
//     VkWriteDescriptorSet writeSets[] = { writeIndexBuffer };
//     vkUpdateDescriptorSets(m_renderer->getDevice().getHandle(), 1, writeSets, 0, nullptr);*/
// }

void VulkanRayTracingScene::setupInput()
{
    m_app->getWindow()->keyPressed += [this](Key key, int)
    {
        switch (key)
        {
        case Key::F5:
            m_resourceContext->recreatePipelines();
            break;
        }
    };
}

void VulkanRayTracingScene::createGui() {}
} // namespace crisp