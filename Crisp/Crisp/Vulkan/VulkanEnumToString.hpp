#pragma once

#include <string>

#include <Crisp/Vulkan/VulkanHeader.hpp>

namespace crisp {

std::string pipelineStageBitToString(VkPipelineStageFlagBits flags);

std::string pipelineStageToString(VkPipelineStageFlags flags);

} // namespace crisp