#include "DescriptorSetAllocator.hpp"
#include "Vulkan/VulkanDevice.hpp"

#include <spdlog/spdlog.h>

#include <numeric>

namespace crisp
{
    namespace
    {
        std::vector<VkDescriptorPoolSize> calculateMinimumPoolSizes(const std::vector<std::vector<VkDescriptorSetLayoutBinding>>& setBindings, std::vector<uint32_t> numCopies)
        {
            if (numCopies.empty())
                numCopies.resize(setBindings.size(), 1);

            std::vector<VkDescriptorPoolSize> poolSizes;
            for (uint32_t i = 0; i < setBindings.size(); i++)
            {
                const std::vector<VkDescriptorSetLayoutBinding>& setBinding = setBindings.at(i);
                for (const auto& descriptorBinding : setBinding)
                {
                    uint32_t k = 0;
                    while (k < poolSizes.size())
                    {
                        if (poolSizes[k].type == descriptorBinding.descriptorType)
                            break;
                        k++;
                    }


                    if (k == poolSizes.size())
                        poolSizes.push_back({ descriptorBinding.descriptorType, 0 });

                    poolSizes[k].descriptorCount += descriptorBinding.descriptorCount * numCopies.at(i);
                }
            }
            return poolSizes;
        }

        static constexpr uint32_t AllocationCount = 4;
    }


    DescriptorSetAllocator::DescriptorSetAllocator(VulkanDevice* device, const std::vector<std::vector<VkDescriptorSetLayoutBinding>>& setBindings, const std::vector<uint32_t>& numCopiesPerSet, VkDescriptorPoolCreateFlags flags)
        : m_device(device)
    {
        auto pool = std::make_unique<DescriptorPool>();
        pool->currentAllocations = 0;
        pool->maxAllocations     = std::accumulate(numCopiesPerSet.begin(), numCopiesPerSet.end(), 0u);
        pool->freeSizes          = calculateMinimumPoolSizes(setBindings, numCopiesPerSet);
        if (pool->maxAllocations > 0)
        {
            VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
            poolInfo.flags         = flags;
            poolInfo.poolSizeCount = static_cast<uint32_t>(pool->freeSizes.size());
            poolInfo.pPoolSizes    = pool->freeSizes.data();
            poolInfo.maxSets       = pool->maxAllocations;
            vkCreateDescriptorPool(m_device->getHandle(), &poolInfo, nullptr, &pool->handle);

            m_descriptorPools.push_back(std::move(pool));
        }
    }

    DescriptorSetAllocator::~DescriptorSetAllocator()
    {
        for (auto& pool : m_descriptorPools)
            vkDestroyDescriptorPool(m_device->getHandle(), pool->handle, nullptr);
    }

    VkDescriptorSet DescriptorSetAllocator::allocate(VkDescriptorSetLayout setLayout, const std::vector<VkDescriptorSetLayoutBinding>& setBindings)
    {
        for (auto& pool : m_descriptorPools)
        {
            if (pool->currentAllocations >= pool->maxAllocations || !pool->canAllocateSet(setBindings))
                continue;

            return pool->allocate(m_device->getHandle(), setLayout, setBindings);
        }

        auto pool = std::make_unique<DescriptorPool>();
        pool->currentAllocations = 0;
        pool->maxAllocations     = AllocationCount;
        pool->freeSizes          = calculateMinimumPoolSizes({ setBindings }, { AllocationCount });

        VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        poolInfo.flags         = 0;
        poolInfo.poolSizeCount = static_cast<uint32_t>(pool->freeSizes.size());
        poolInfo.pPoolSizes    = pool->freeSizes.data();
        poolInfo.maxSets       = pool->maxAllocations;
        vkCreateDescriptorPool(m_device->getHandle(), &poolInfo, nullptr, &pool->handle);

        m_descriptorPools.push_back(std::move(pool));

        return m_descriptorPools.back()->allocate(m_device->getHandle(), setLayout, setBindings);
    }

    bool DescriptorSetAllocator::DescriptorPool::canAllocateSet(const std::vector<VkDescriptorSetLayoutBinding>& setBindings) const
    {
        auto newSizes = freeSizes;
        for (const auto& binding : setBindings)
        {
            bool canAllocateBinding = false;
            for (auto& size : newSizes)
            {
                if (size.type == binding.descriptorType)
                {
                    if (size.descriptorCount >= binding.descriptorCount)
                    {
                        size.descriptorCount -= binding.descriptorCount;
                        canAllocateBinding = true;
                        break;
                    }
                    else
                        return false;
                }
            }

            if (!canAllocateBinding)
                return false;
        }

        return true;
    }

    void DescriptorSetAllocator::DescriptorPool::deductPoolSizes(const std::vector<VkDescriptorSetLayoutBinding>& setBindings)
    {
        for (const auto& binding : setBindings)
            for (auto& size : freeSizes)
                if (size.type == binding.descriptorType)
                    size.descriptorCount -= binding.descriptorCount;
    }

    VkDescriptorSet DescriptorSetAllocator::DescriptorPool::allocate(VkDevice device, VkDescriptorSetLayout setLayout, const std::vector<VkDescriptorSetLayoutBinding>& setBindings)
    {
        currentAllocations++;
        deductPoolSizes(setBindings);

        VkDescriptorSetAllocateInfo descSetInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        descSetInfo.descriptorPool     = handle;
        descSetInfo.descriptorSetCount = 1;
        descSetInfo.pSetLayouts        = &setLayout;

        VkDescriptorSet descSet;
        vkAllocateDescriptorSets(device, &descSetInfo, &descSet);
        if (descSet == 0)
            spdlog::error("Descriptor set is nullptr!");

        return descSet;
    }
}