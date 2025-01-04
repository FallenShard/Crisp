#include <Crisp/Vulkan/Rhi/Test/VulkanTest.hpp>

#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/PipelineLayoutBuilder.hpp>

namespace crisp {
namespace {

using ::testing::Field;
using ::testing::IsEmpty;

using DescriptorUpdaterTest = VulkanTest;

TEST_F(DescriptorUpdaterTest, BasicUsage) {
    PipelineLayoutBuilder builder{};
    builder
        .defineDescriptorSet(
            0,
            false,
            {
                {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
                {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
                {2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
            })
        .defineDescriptorSet(
            1,
            false,
            {
                {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT},
                {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT},
            })
        .defineDescriptorSet(
            2,
            false,
            {
                {0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
                {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
            });
    const auto bindings = builder.getDescriptorSetLayoutBindings();
    EXPECT_EQ(bindings.size(), 3);

    auto layoutHandles = builder.createDescriptorSetLayoutHandles(device_->getHandle());
    EXPECT_EQ(layoutHandles.size(), 3);

    for (auto layout : layoutHandles) {
        vkDestroyDescriptorSetLayout(device_->getHandle(), layout, nullptr);
    }
}

auto BindingIs(const uint32_t binding, const uint32_t count, const VkDescriptorType type) {
    return AllOf(
        Field(&VkDescriptorSetLayoutBinding::binding, binding),
        Field(&VkDescriptorSetLayoutBinding::descriptorCount, count),
        Field(&VkDescriptorSetLayoutBinding::descriptorType, type));
}

TEST_F(DescriptorUpdaterTest, BindlessDescriptor) {
    PipelineLayoutMetadata metadata{
        reflectPipelineLayoutFromSpirvPath("D:/Projects/Crisp/Resources/Shaders/pbr.frag.spv").unwrap()};
    EXPECT_THAT(
        metadata.descriptorSetLayoutBindings,
        ElementsAre(
            ElementsAre(
                BindingIs(0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER),
                BindingIs(1, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER),
                BindingIs(2, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER),
                BindingIs(3, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER),
                BindingIs(4, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER),
                BindingIs(5, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER),
                BindingIs(6, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)),
            ElementsAre(
                BindingIs(0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER),
                BindingIs(1, 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER))));
    EXPECT_THAT(metadata.pushConstants, IsEmpty());

    PipelineLayoutBuilder builder(std::move(metadata));
    builder.setDescriptorBindless(1, 1, 50);

    const auto layout = builder.create(*device_);

    VulkanDescriptorSetAllocator setAllocator(
        *device_, builder.getDescriptorSetLayoutBindings(), {1, 1}, VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

    // const auto set =
    //     setAllocator.allocateBindless(layout->getDescriptorSetLayout(1), layout->getDescriptorSetLayoutBindings(1));

    // VkImageCreateInfo createInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    // createInfo.imageType = VK_IMAGE_TYPE_2D;
    // createInfo.arrayLayers = 1;
    // createInfo.extent = {100, 100, 1};
    // createInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    // createInfo.mipLevels = 1;
    // createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // createInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    // createInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    // VulkanImage image(*device_, createInfo);
    // const auto view(createView(*device_, image, VK_IMAGE_VIEW_TYPE_2D));

    // const auto sampler(createLinearClampSampler(*device_));

    // std::array<VkWriteDescriptorSet, 50> writes{};
    // std::array<VkDescriptorImageInfo, 50> descInfos{};
    // for (uint32_t i = 0; auto& write : writes) {
    //     write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    //     write.dstSet = set;
    //     write.dstBinding = 7;
    //     write.dstArrayElement = i;
    //     write.descriptorCount = 1;
    //     write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    //     write.pImageInfo = &descInfos[i];

    //     descInfos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    //     descInfos[i].imageView = view->getHandle();
    //     descInfos[i].sampler = sampler->getHandle();

    //     ++i;
    // }

    // vkUpdateDescriptorSets(device_->getHandle(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

} // namespace
} // namespace crisp