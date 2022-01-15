#pragma once

#include <string>

#include <vulkan/vulkan.h>

namespace crisp
{

std::string pipelineStageBitToString(VkPipelineStageFlagBits flags);

std::string pipelineStageToString(VkPipelineStageFlags flags);

} // namespace crisp