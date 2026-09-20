#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <benchmark/benchmark.h>

#include <Crisp/Vulkan/Rhi/VulkanCommandPool.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanInstance.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPhysicalDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanQueue.hpp>
#include <Crisp/Vulkan/Rhi/VulkanTimestampQueryPool.hpp>
#include <Crisp/Vulkan/VulkanCommandEncoder.hpp>

namespace crisp {

class VulkanBenchmarkContext {
public:
    static VulkanBenchmarkContext& instance();

    VulkanDevice& getDevice() const {
        return *m_device;
    }

    const VulkanPhysicalDevice& getPhysicalDevice() const {
        return *m_physicalDevice;
    }

    VulkanBenchmarkContext(const VulkanBenchmarkContext&) = delete;
    VulkanBenchmarkContext& operator=(const VulkanBenchmarkContext&) = delete;
    VulkanBenchmarkContext(VulkanBenchmarkContext&&) = delete;
    VulkanBenchmarkContext& operator=(VulkanBenchmarkContext&&) = delete;

private:
    VulkanBenchmarkContext();
    ~VulkanBenchmarkContext();

    std::unique_ptr<VulkanInstance> m_instance;
    std::unique_ptr<VulkanPhysicalDevice> m_physicalDevice;
    std::unique_ptr<VulkanDevice> m_device;
};

using VulkanBenchmarkRecorder = std::function<void(const VulkanCommandEncoder&)>;
using VulkanBenchmarkWorkItem = std::pair<const VulkanQueue*, VulkanBenchmarkRecorder>;

class VulkanBenchmarkFixture : public benchmark::Fixture {
public:
    void SetUp(benchmark::State& state) override;
    void TearDown(benchmark::State& state) override;

protected:
    static VulkanDevice& getDevice() {
        return VulkanBenchmarkContext::instance().getDevice();
    }

    static const VulkanPhysicalDevice& getPhysicalDevice() {
        return VulkanBenchmarkContext::instance().getPhysicalDevice();
    }

    static bool supportsTimestamps(const VulkanQueue& queue);

    double measureDeviceSeconds(const VulkanBenchmarkRecorder& record);
    double measureDeviceSeconds(const VulkanQueue& queue, const VulkanBenchmarkRecorder& record);
    double measureConcurrentSeconds(std::span<const VulkanBenchmarkWorkItem> work);

    void runTimedLoop(benchmark::State& state, const VulkanBenchmarkRecorder& record);
    void runTimedLoop(benchmark::State& state, const VulkanQueue& queue, const VulkanBenchmarkRecorder& record);
    void runConcurrentLoop(benchmark::State& state, std::span<const VulkanBenchmarkWorkItem> work);

private:
    struct QueueResources {
        std::unique_ptr<VulkanCommandPool> commandPool;
        std::unique_ptr<VulkanTimestampQueryPool> queryPool;
    };

    QueueResources& getResourcesFor(const VulkanQueue& queue);

    std::unordered_map<VkQueue, QueueResources> m_queueResources;
};

void setRateCounter(benchmark::State& state, std::string_view name, double unitsPerIteration);
void setEfficiencyCounter(benchmark::State& state, std::string_view name, double achieved, double peak);

std::filesystem::path getBenchmarkShaderPath(std::string_view fileName);
const std::filesystem::path& getBenchmarkShaderSpirv(std::string_view fileName);

} // namespace crisp
