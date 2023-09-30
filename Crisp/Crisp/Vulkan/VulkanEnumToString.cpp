#include <Crisp/Vulkan/VulkanEnumToString.hpp>

#define CASE_ENUM_TO_STRING(x, offset)                                                                                 \
    {                                                                                                                  \
    case x:                                                                                                            \
        return std::string(#x).substr(offset);                                                                         \
    }

#define PIPELINE_STAGE_TO_STRING(x) CASE_ENUM_TO_STRING(VK_PIPELINE_STAGE_##x, std::strlen("VK_PIPELINE_STAGE_"));

namespace crisp {
std::string pipelineStageBitToString(VkPipelineStageFlagBits flags) {
    switch (flags) {
        PIPELINE_STAGE_TO_STRING(TOP_OF_PIPE_BIT);
        PIPELINE_STAGE_TO_STRING(DRAW_INDIRECT_BIT);
        PIPELINE_STAGE_TO_STRING(VERTEX_INPUT_BIT);
        PIPELINE_STAGE_TO_STRING(VERTEX_SHADER_BIT);
        PIPELINE_STAGE_TO_STRING(TESSELLATION_CONTROL_SHADER_BIT);
        PIPELINE_STAGE_TO_STRING(TESSELLATION_EVALUATION_SHADER_BIT);
        PIPELINE_STAGE_TO_STRING(GEOMETRY_SHADER_BIT);
        PIPELINE_STAGE_TO_STRING(FRAGMENT_SHADER_BIT);
        PIPELINE_STAGE_TO_STRING(EARLY_FRAGMENT_TESTS_BIT);
        PIPELINE_STAGE_TO_STRING(LATE_FRAGMENT_TESTS_BIT);
        PIPELINE_STAGE_TO_STRING(COLOR_ATTACHMENT_OUTPUT_BIT);
        PIPELINE_STAGE_TO_STRING(COMPUTE_SHADER_BIT);
        PIPELINE_STAGE_TO_STRING(TRANSFER_BIT);
        PIPELINE_STAGE_TO_STRING(BOTTOM_OF_PIPE_BIT);
        PIPELINE_STAGE_TO_STRING(HOST_BIT);
        PIPELINE_STAGE_TO_STRING(ALL_GRAPHICS_BIT);
        PIPELINE_STAGE_TO_STRING(ALL_COMMANDS_BIT);
        PIPELINE_STAGE_TO_STRING(NONE);
        PIPELINE_STAGE_TO_STRING(TRANSFORM_FEEDBACK_BIT_EXT);
        PIPELINE_STAGE_TO_STRING(CONDITIONAL_RENDERING_BIT_EXT);
        PIPELINE_STAGE_TO_STRING(ACCELERATION_STRUCTURE_BUILD_BIT_KHR);
        PIPELINE_STAGE_TO_STRING(RAY_TRACING_SHADER_BIT_KHR);
        PIPELINE_STAGE_TO_STRING(FRAGMENT_DENSITY_PROCESS_BIT_EXT);
        PIPELINE_STAGE_TO_STRING(FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR);
        PIPELINE_STAGE_TO_STRING(COMMAND_PREPROCESS_BIT_NV);
        PIPELINE_STAGE_TO_STRING(TASK_SHADER_BIT_EXT);
        PIPELINE_STAGE_TO_STRING(MESH_SHADER_BIT_EXT);
        PIPELINE_STAGE_TO_STRING(FLAG_BITS_MAX_ENUM);
        break;
    }

    return "Unknown";
}

std::string pipelineStageToString(VkPipelineStageFlags flags) {
    std::string result;
    uint32_t i = 0;
    while (flags) {
        if (flags & 0x01) {
            if (!result.empty()) {
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