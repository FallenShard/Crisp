#include <Crisp/Vulkan/VulkanEnumToString.hpp>

#define CASE_ENUM_TO_STRING(x, offset)                                                                                 \
    {                                                                                                                  \
    case x:                                                                                                            \
        return std::string(#x).substr(offset);                                                                         \
    }

#define PIPELINE_STAGE_TO_STRING(x) CASE_ENUM_TO_STRING(x, std::strlen("VK_PIPELINE_STAGE_"));

namespace crisp
{
std::string pipelineStageBitToString(VkPipelineStageFlagBits flags)
{
    switch (flags)
    {
        PIPELINE_STAGE_TO_STRING(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
        PIPELINE_STAGE_TO_STRING(VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);
        PIPELINE_STAGE_TO_STRING(VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);
        PIPELINE_STAGE_TO_STRING(VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
        PIPELINE_STAGE_TO_STRING(VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT);
        PIPELINE_STAGE_TO_STRING(VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT);
        PIPELINE_STAGE_TO_STRING(VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT);
        PIPELINE_STAGE_TO_STRING(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        PIPELINE_STAGE_TO_STRING(VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
        PIPELINE_STAGE_TO_STRING(VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
        PIPELINE_STAGE_TO_STRING(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        PIPELINE_STAGE_TO_STRING(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        PIPELINE_STAGE_TO_STRING(VK_PIPELINE_STAGE_TRANSFER_BIT);
        PIPELINE_STAGE_TO_STRING(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        PIPELINE_STAGE_TO_STRING(VK_PIPELINE_STAGE_HOST_BIT);
        PIPELINE_STAGE_TO_STRING(VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
        PIPELINE_STAGE_TO_STRING(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    }

    return "Unknown";
}

std::string pipelineStageToString(VkPipelineStageFlags flags)
{
    std::string result;
    uint32_t i = 0;
    while (flags)
    {
        if (flags & 0x01)
        {
            if (!result.empty())
            {
                result += " | ";
            }
            result += pipelineStageBitToString(static_cast<VkPipelineStageFlagBits>(1 << i));
        }
        flags >>= 1;
        ++i;
    }

    return result;
}

} // namespace crisp