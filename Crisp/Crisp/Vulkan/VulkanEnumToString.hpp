#pragma once

#include <Crisp/Vulkan/VulkanHeader.hpp>

#include <string>

namespace crisp
{

std::string pipelineStageBitToString(VkPipelineStageFlagBits flags);

std::string pipelineStageToString(VkPipelineStageFlags flags);

} // namespace crisp