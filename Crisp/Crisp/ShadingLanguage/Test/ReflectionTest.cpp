#include <Crisp/ShadingLanguage/Reflection.hpp>

#include <gmock/gmock.h>
#include <spirv_reflect.h>

namespace crisp {
namespace {
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::SizeIs;

std::filesystem::path getSpirvShaderPath(const std::string& shaderName) {
    const std::filesystem::path kSpvAssetPath = "D:/Projects/Crisp/Resources/Shaders";
    return kSpvAssetPath / (shaderName + ".spv");
}

TEST(ReflectionTest, VertexShader) {
    const auto reflection = reflectUniformMetadataFromSpirvPath(getSpirvShaderPath("ocean-spectrum.comp")).unwrap();
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

TEST(ReflectionTest, SpirvReflect) {
    SpvReflectShaderModule module;
    const auto spirv = readSpirvFile(getSpirvShaderPath("ocean-spectrum.comp")).unwrap();
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

} // namespace
} // namespace crisp