#include <Crisp/ShaderUtils/Reflection.hpp>

#include <gmock/gmock.h>
#include <spirv_reflect.h>

#include <Crisp/ShaderUtils/Test/TestShaderMap.hpp>

namespace crisp {
namespace {
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::SizeIs;

const auto kShaderSourceDirectory = std::filesystem::path{"TestData"} / "CrispSpvReflectionTest";
const TestShaderMap kTestShaders{
    kShaderSourceDirectory / "reflection.comp",
    kShaderSourceDirectory / "reflection.vert",
    kShaderSourceDirectory / "heap-untyped.comp",
};

TEST(ReflectionTest, ComputeShader) {
    const auto reflection = reflectPipelineLayoutFromSpirv(kTestShaders.getSpirvPath("reflection.comp")).unwrap();
    ASSERT_THAT(reflection.descriptorSetLayoutBindings, SizeIs(1));
    EXPECT_THAT(
        reflection.descriptorSetLayoutBindings[0],
        ElementsAre(
            AllOf(
                Field(&VkDescriptorSetLayoutBinding::binding, 0),
                Field(&VkDescriptorSetLayoutBinding::descriptorCount, 1),
                Field(&VkDescriptorSetLayoutBinding::descriptorType, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)),
            AllOf(
                Field(&VkDescriptorSetLayoutBinding::binding, 1),
                Field(&VkDescriptorSetLayoutBinding::descriptorCount, 1),
                Field(&VkDescriptorSetLayoutBinding::descriptorType, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)),
            AllOf(
                Field(&VkDescriptorSetLayoutBinding::binding, 2),
                Field(&VkDescriptorSetLayoutBinding::descriptorCount, 1),
                Field(&VkDescriptorSetLayoutBinding::descriptorType, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)),
            AllOf(
                Field(&VkDescriptorSetLayoutBinding::binding, 3),
                Field(&VkDescriptorSetLayoutBinding::descriptorCount, 1),
                Field(&VkDescriptorSetLayoutBinding::descriptorType, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)),
            AllOf(
                Field(&VkDescriptorSetLayoutBinding::binding, 4),
                Field(&VkDescriptorSetLayoutBinding::descriptorCount, 1),
                Field(&VkDescriptorSetLayoutBinding::descriptorType, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)),
            AllOf(
                Field(&VkDescriptorSetLayoutBinding::binding, 5),
                Field(&VkDescriptorSetLayoutBinding::descriptorCount, 1),
                Field(&VkDescriptorSetLayoutBinding::descriptorType, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE))));

    EXPECT_THAT(
        reflection.pushConstants,
        ElementsAre(AllOf(
            Field(&VkPushConstantRange::offset, 0),
            Field(&VkPushConstantRange::size, 48),
            Field(&VkPushConstantRange::stageFlags, VK_SHADER_STAGE_COMPUTE_BIT))));
}

TEST(ReflectionTest, MergeCoalescesMatchingPushConstantRanges) {
    PipelineLayoutMetadata lhs{};
    lhs.pushConstants.push_back({VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, 56});

    PipelineLayoutMetadata rhs{};
    rhs.pushConstants.push_back({VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0, 56});
    lhs.merge(rhs);

    EXPECT_THAT(
        lhs.pushConstants,
        ElementsAre(AllOf(
            Field(&VkPushConstantRange::offset, 0),
            Field(&VkPushConstantRange::size, 56),
            Field(
                &VkPushConstantRange::stageFlags,
                VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR))));
}

TEST(ReflectionTest, SpirvReflect) {
    SpvReflectShaderModule module;
    const auto spirv = readSpirvFile(kTestShaders.getSpirvPath("reflection.comp")).unwrap();
    SpvReflectResult result = spvReflectCreateShaderModule(spirv.size(), spirv.data(), &module);
    EXPECT_THAT(result, SPV_REFLECT_RESULT_SUCCESS);

    uint32_t varCount = 0;
    result = spvReflectEnumerateInputVariables(&module, &varCount, nullptr);
    EXPECT_THAT(result, SPV_REFLECT_RESULT_SUCCESS);

    std::vector<SpvReflectInterfaceVariable*> inputVars(varCount);
    result = spvReflectEnumerateInputVariables(&module, &varCount, inputVars.data());
    EXPECT_THAT(result, SPV_REFLECT_RESULT_SUCCESS);

    uint32_t descSetCount = 0;
    result = spvReflectEnumerateDescriptorSets(&module, &descSetCount, nullptr);
    EXPECT_THAT(result, SPV_REFLECT_RESULT_SUCCESS);

    std::vector<SpvReflectDescriptorSet*> descSets(descSetCount);
    result = spvReflectEnumerateDescriptorSets(&module, &descSetCount, descSets.data());
    EXPECT_THAT(result, SPV_REFLECT_RESULT_SUCCESS);

    spvReflectDestroyShaderModule(&module);
}

TEST(ReflectionTest, VertexShaderExcludesBuiltIns) {
    const auto reflection =
        reflectVertexMetadataFromSpirvShader(readSpirvFile(kTestShaders.getSpirvPath("reflection.vert")).unwrap())
            .unwrap();
    using AttribDesc = decltype(reflection)::VertexAttributeDescription;
    EXPECT_THAT(
        reflection.attributes,
        ElementsAre(
            AllOf(
                Field(&AttribDesc::name, "position"),
                Field(&AttribDesc::location, 0),
                Field(&AttribDesc::format, VK_FORMAT_R32G32B32_SFLOAT)),
            AllOf(
                Field(&AttribDesc::name, "normal"),
                Field(&AttribDesc::location, 1),
                Field(&AttribDesc::format, VK_FORMAT_R32G32B32_SFLOAT)),
            AllOf(
                Field(&AttribDesc::name, "texCoord"),
                Field(&AttribDesc::location, 2),
                Field(&AttribDesc::format, VK_FORMAT_R32G32_SFLOAT)),
            AllOf(
                Field(&AttribDesc::name, "tangent"),
                Field(&AttribDesc::location, 3),
                Field(&AttribDesc::format, VK_FORMAT_R32G32B32A32_SFLOAT))));
}

TEST(DescriptorHeapReflectionTest, UntypedHeapReportsAccessesNotBindings) {
    const auto spirv = readSpirvFile(kTestShaders.getSpirvPath("heap-untyped.comp")).unwrap();

    SpvReflectShaderModule module;
    ASSERT_THAT(spvReflectCreateShaderModule(spirv.size(), spirv.data(), &module), SPV_REFLECT_RESULT_SUCCESS);

    EXPECT_THAT(module.descriptor_binding_count, 0U);
    EXPECT_THAT(module.descriptor_set_count, 0U);
    EXPECT_THAT(module.push_constant_block_count, 1U);

    ASSERT_THAT(module.entry_point_count, 1U);
    const auto& entryPoint = module.entry_points[0]; // NOLINT
    EXPECT_THAT(entryPoint.sampler_heap_access_count, 0U);

    std::vector<SpvReflectDescriptorType> accessedTypes;
    for (uint32_t i = 0; i < entryPoint.resource_heap_access_count; ++i) {
        accessedTypes.push_back(entryPoint.resource_heap_accesses[i].descriptor_type); // NOLINT
    }
    EXPECT_THAT(
        accessedTypes,
        ::testing::UnorderedElementsAre(
            SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER));

    spvReflectDestroyShaderModule(&module);
}

} // namespace
} // namespace crisp
