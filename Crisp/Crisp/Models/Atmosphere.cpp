#include <Crisp/Models/Atmosphere.hpp>

#include <Crisp/Renderer/PipelineBuilder.hpp>
#include <Crisp/Renderer/PipelineLayoutBuilder.hpp>
#include <Crisp/Renderer/RenderGraph.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/ResourceContext.hpp>

#include <Crisp/Vulkan/VulkanDevice.hpp>

namespace crisp
{
std::unique_ptr<VulkanPipeline> createMultiScatPipeline(Renderer& renderer, const glm::uvec3& workGroupSize)
{
    PipelineLayoutBuilder layoutBuilder;
    layoutBuilder
        .defineDescriptorSet(0, false,
            {
                {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_COMPUTE_BIT},
                {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_COMPUTE_BIT},
    })
        .defineDescriptorSet(1, true,
            {
                { 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT },
                { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT },
            });

    VulkanDevice& device = renderer.getDevice();
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

    VkComputePipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    pipelineInfo.stage =
        createShaderStageInfo(VK_SHADER_STAGE_COMPUTE_BIT, renderer.getShaderModule("multiple-scattering.comp"));
    pipelineInfo.stage.pSpecializationInfo = &specInfo;
    pipelineInfo.layout = layout->getHandle();
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;
    VkPipeline pipeline;
    vkCreateComputePipelines(device.getHandle(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

    auto uniqueHandle =
        std::make_unique<VulkanPipeline>(device, pipeline, std::move(layout), PipelineDynamicStateFlags());
    uniqueHandle->setBindPoint(VK_PIPELINE_BIND_POINT_COMPUTE);
    return uniqueHandle;
}

robin_hood::unordered_flat_map<std::string, std::unique_ptr<RenderNode>> addAtmosphereRenderPasses(
    RenderGraph& renderGraph, Renderer& renderer, ResourceContext& resourceContext, const std::string& dependentPass)
{
    robin_hood::unordered_flat_map<std::string, std::unique_ptr<RenderNode>> renderNodes;
    resourceContext.createUniformBuffer("atmosphere", sizeof(SkyAtmosphereConstantBufferStructure),
        BufferUpdatePolicy::PerFrame);
    resourceContext.createUniformBuffer("atmosphereCommon", sizeof(CommonConstantBufferStructure),
        BufferUpdatePolicy::PerFrame);

    // Transmittance lookup
    renderGraph.addRenderPass("TransLUTPass", createTransmittanceLutPass(renderer));
    auto transLutPipeline =
        resourceContext.createPipeline("transLut", "SkyTransLut.lua", renderGraph.getRenderPass("TransLUTPass"), 0);
    auto transLutMaterial = resourceContext.createMaterial("transLut", transLutPipeline);
    transLutMaterial->writeDescriptor(0, 0, *resourceContext.getUniformBuffer("atmosphereCommon"));
    transLutMaterial->writeDescriptor(0, 1, *resourceContext.getUniformBuffer("atmosphere"));

    auto transLutNode = renderNodes.emplace("transLutNode", std::make_unique<RenderNode>()).first->second.get();
    transLutNode->geometry = renderer.getFullScreenGeometry();
    transLutNode->pass("TransLUTPass").material = transLutMaterial;
    transLutNode->pass("TransLUTPass").pipeline = transLutPipeline;

    // Multiscattering
    VkImageCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    createInfo.flags = 0;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    createInfo.extent = VkExtent3D{ 32u, 32u, 1u };
    createInfo.mipLevels = 1;
    createInfo.arrayLayers = 2;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    resourceContext.addImage("multiScatTex",
        std::make_unique<VulkanImage>(renderer.getDevice(), createInfo, VK_IMAGE_ASPECT_COLOR_BIT));
    resourceContext.addImageView("multiScatTexView0",
        resourceContext.getImage("multiScatTex")->createView(VK_IMAGE_VIEW_TYPE_2D, 0, 1));
    resourceContext.addImageView("multiScatTexView1",
        resourceContext.getImage("multiScatTex")->createView(VK_IMAGE_VIEW_TYPE_2D, 1, 1));

    renderer.enqueueResourceUpdate(
        [tex = resourceContext.getImage("multiScatTex")](VkCommandBuffer cmdBuffer)
        {
            tex->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        });

    auto& multiScatPass = renderGraph.addComputePass("MultiScatPass");
    renderGraph.addRenderTargetLayoutTransition("TransLUTPass", "MultiScatPass", 0);
    multiScatPass.workGroupSize = glm::ivec3(1, 1, 64);
    multiScatPass.numWorkGroups = glm::ivec3(32, 32, 1);
    multiScatPass.pipeline = createMultiScatPipeline(renderer, multiScatPass.workGroupSize);
    multiScatPass.material = std::make_unique<Material>(multiScatPass.pipeline.get());
    multiScatPass.material->writeDescriptor(0, 0, *resourceContext.getUniformBuffer("atmosphereCommon"));
    multiScatPass.material->writeDescriptor(0, 1, *resourceContext.getUniformBuffer("atmosphere"));

    std::vector<VulkanImageView*> views{ resourceContext.getImageView("multiScatTexView0"),
        resourceContext.getImageView("multiScatTexView1") };
    multiScatPass.material->writeDescriptor(1, 0, views, nullptr, VK_IMAGE_LAYOUT_GENERAL);
    multiScatPass.material->writeDescriptor(1, 1, renderGraph.getRenderPass("TransLUTPass"), 0,
        resourceContext.getSampler("linearClamp"));
    multiScatPass.material->setDynamicBufferView(0, *resourceContext.getUniformBuffer("atmosphereCommon"), 0);
    multiScatPass.material->setDynamicBufferView(1, *resourceContext.getUniformBuffer("atmosphere"), 0);
    multiScatPass.preDispatchCallback = [](VulkanCommandBuffer& /*cmdBuffer*/, uint32_t /*frameIndex*/)
    {
    };

    renderGraph.addRenderPass("SkyViewLUTPass", createSkyViewLutPass(renderer));
    renderGraph.addRenderTargetLayoutTransition("TransLUTPass", "SkyViewLUTPass", 0);
    renderGraph.addDependency("MultiScatPass", "SkyViewLUTPass",
        [tex = resourceContext.getImage("multiScatTex")](const VulkanRenderPass&, VulkanCommandBuffer& cmdBuffer,
            uint32_t frameIndex)
        {
            VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.image = tex->getHandle();
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = frameIndex;
            barrier.subresourceRange.layerCount = 1;

            vkCmdPipelineBarrier(cmdBuffer.getHandle(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 0, &barrier);
        });

    // Sky View LUT
    auto skyViewLutPipeline =
        resourceContext.createPipeline("skyViewLut", "SkyViewLut.lua", renderGraph.getRenderPass("SkyViewLUTPass"), 0);
    auto skyViewLutMaterial = resourceContext.createMaterial("skyViewLut", skyViewLutPipeline);
    skyViewLutMaterial->writeDescriptor(0, 0, *resourceContext.getUniformBuffer("atmosphereCommon"));
    skyViewLutMaterial->writeDescriptor(0, 1, *resourceContext.getUniformBuffer("atmosphere"));
    skyViewLutMaterial->writeDescriptor(1, 0, renderGraph.getRenderPass("TransLUTPass"), 0,
        resourceContext.getSampler("linearClamp"));
    skyViewLutMaterial->writeDescriptor(1, 1, views, resourceContext.getSampler("linearClamp"),
        VK_IMAGE_LAYOUT_GENERAL);
    auto skyViewLutNode = renderNodes.emplace("skyViewLutNode", std::make_unique<RenderNode>()).first->second.get();
    skyViewLutNode->geometry = renderer.getFullScreenGeometry();
    skyViewLutNode->pass("SkyViewLUTPass").material = skyViewLutMaterial;
    skyViewLutNode->pass("SkyViewLUTPass").pipeline = skyViewLutPipeline;

    // Camera volumes
    auto camVolPass = std::make_unique<CameraVolumesPass>(renderer);
    auto& camPass = *camVolPass;
    renderGraph.addRenderPass("CameraVolumesPass", std::move(camVolPass));
    renderGraph.addRenderTargetLayoutTransition("SkyViewLUTPass", "CameraVolumesPass", 0);
    auto cameraVolumesPipeline = resourceContext.createPipeline("skyCameraVolumes", "SkyCameraVolumes.lua",
        renderGraph.getRenderPass("CameraVolumesPass"), 0);
    auto cameraVolumesMaterial = resourceContext.createMaterial("skyCameraVolumes", cameraVolumesPipeline);
    cameraVolumesMaterial->writeDescriptor(0, 0, *resourceContext.getUniformBuffer("atmosphereCommon"));
    cameraVolumesMaterial->writeDescriptor(0, 1, *resourceContext.getUniformBuffer("atmosphere"));
    cameraVolumesMaterial->writeDescriptor(1, 0, renderGraph.getRenderPass("TransLUTPass"), 0,
        resourceContext.getSampler("linearClamp"));
    cameraVolumesMaterial->writeDescriptor(1, 1, views, resourceContext.getSampler("linearClamp"),
        VK_IMAGE_LAYOUT_GENERAL);

    // Voxelized multiple scattering
    const std::vector<glm::vec2> vertices = {
        {-1.0f, -1.0f},
        {+3.0f, -1.0f},
        {-1.0f, +3.0f}
    };
    const std::vector<glm::uvec3> faces = {
        {0, 2, 1}
    };
    resourceContext.addGeometry("fullScreenInstanced", std::make_unique<Geometry>(&renderer, vertices, faces));
    resourceContext.getGeometry("fullScreenInstanced")->setInstanceCount(32);
    auto skyCameraVolumesNode =
        renderNodes.emplace("skyCameraVolumesNode", std::make_unique<RenderNode>()).first->second.get();
    skyCameraVolumesNode->geometry = resourceContext.getGeometry("fullScreenInstanced");
    skyCameraVolumesNode->pass("CameraVolumesPass").material = cameraVolumesMaterial;
    skyCameraVolumesNode->pass("CameraVolumesPass").pipeline = cameraVolumesPipeline;

    renderer.getDevice().flushDescriptorUpdates();

    // Ray marching - final step
    renderGraph.addRenderPass("RayMarchingPass", createRayMarchingPass(renderer));
    renderGraph.addRenderTargetLayoutTransition("CameraVolumesPass", "RayMarchingPass", 0);
    renderGraph.addRenderTargetLayoutTransition("DepthPrePass", "RayMarchingPass", 0);
    auto rayMarchingPipeline = resourceContext.createPipeline("rayMarching", "SkyRayMarching.lua",
        renderGraph.getRenderPass("RayMarchingPass"), 0);
    auto rayMarchingMaterial = resourceContext.createMaterial("rayMarching", rayMarchingPipeline);
    rayMarchingMaterial->writeDescriptor(0, 0, *resourceContext.getUniformBuffer("atmosphereCommon"));
    rayMarchingMaterial->writeDescriptor(0, 1, *resourceContext.getUniformBuffer("atmosphere"));
    rayMarchingMaterial->writeDescriptor(1, 0, renderGraph.getRenderPass("TransLUTPass"), 0,
        resourceContext.getSampler("linearClamp"));
    rayMarchingMaterial->writeDescriptor(1, 1, views, resourceContext.getSampler("linearClamp"),
        VK_IMAGE_LAYOUT_GENERAL);
    rayMarchingMaterial->writeDescriptor(1, 2, renderGraph.getRenderPass("SkyViewLUTPass"), 0,
        resourceContext.getSampler("linearClamp"));
    rayMarchingMaterial->writeDescriptor(1, 3, camPass.getArrayViews(), resourceContext.getSampler("linearClamp"),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    rayMarchingMaterial->writeDescriptor(1, 4, renderGraph.getRenderPass("DepthPrePass"), 0,
        resourceContext.getSampler("nearestNeighbor"));

    renderer.getDevice().flushDescriptorUpdates();

    auto skyRayMarchingNode =
        renderNodes.emplace("skyRayMarchingNode", std::make_unique<RenderNode>()).first->second.get();
    skyRayMarchingNode->geometry = renderer.getFullScreenGeometry();
    skyRayMarchingNode->pass("RayMarchingPass").material = rayMarchingMaterial;
    skyRayMarchingNode->pass("RayMarchingPass").pipeline = rayMarchingPipeline;

    renderGraph.addRenderTargetLayoutTransition("RayMarchingPass", dependentPass, 0);

    return renderNodes;
}
} // namespace crisp