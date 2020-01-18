#pragma once

#include <vulkan/vulkan.h>

#include <filesystem>
#include <string>
#include <vector>

namespace crisp::sl
{
    class Reflection
    {
    public:
        Reflection();

        void parseDescriptorSets(const std::filesystem::path& sourcePath);

        uint32_t getDescriptorSetCount() const;

        std::vector<VkDescriptorSetLayoutBinding> getDescriptorSetLayouts(uint32_t index) const;
        bool isSetBuffered(uint32_t index) const;

        std::vector<VkPushConstantRange> getPushConstants() const;

    private:
        void addSetLayoutBinding(uint32_t setid, const VkDescriptorSetLayoutBinding& binding, bool isBuffered);

        std::vector<std::vector<VkDescriptorSetLayoutBinding>> m_setLayoutBindings;
        std::vector<bool> m_isSetBuffered;

        std::vector<VkPushConstantRange> m_pushConstants;
    };
}
