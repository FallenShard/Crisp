#include <Crisp/Vulkan/Benchmark/VulkanBenchmark.hpp>

#include <array>
#include <chrono>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <vector>

#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Core/Logger.hpp>
#include <Crisp/Core/UniqueTemporaryFile.hpp>
#include <Crisp/ShaderUtils/ShaderCompiler.hpp>
#include <Crisp/Vulkan/Rhi/VulkanQueueConfiguration.hpp>

namespace crisp {
namespace {
constexpr uint32_t kTimestampQueryCount = 2;
constexpr uint32_t kWarmUpIterations = 3;
} // namespace

VulkanBenchmarkContext::VulkanBenchmarkContext() {
    spdlog::set_level(spdlog::level::warn);

    m_instance = std::make_unique<VulkanInstance>(nullptr, std::vector<std::string>{}, false);

    VulkanDeviceConfiguration deviceConfig{};
    auto featureRequests = createDefaultFeatureRequests();
    addMeshShadingFeatures(featureRequests);
    m_physicalDevice = std::make_unique<VulkanPhysicalDevice>(
        selectPhysicalDevice(
            *m_instance,
            featureRequests,
            *deviceConfig.featureChain,
            deviceConfig.extensions,
            deviceConfig.enabledFeatures)
            .unwrap());
    deviceConfig.queueConfig =
        createQueueConfiguration({QueueType::General, QueueType::Transfer}, *m_instance, *m_physicalDevice);
    m_device = std::make_unique<VulkanDevice>(
        std::move(deviceConfig), *m_physicalDevice, *m_instance, /*virtualFrameCount=*/1);
}

VulkanBenchmarkContext::~VulkanBenchmarkContext() {
    m_device.reset();
    m_physicalDevice.reset();
    m_instance.reset();
}

VulkanBenchmarkContext& VulkanBenchmarkContext::instance() {
    static VulkanBenchmarkContext context;
    return context;
}

void VulkanBenchmarkFixture::SetUp(benchmark::State& /*state*/) {}

void VulkanBenchmarkFixture::TearDown(benchmark::State& /*state*/) {
    m_queueResources.clear();
    getDevice().getResourceDeallocator().freeAllResources();
}

bool VulkanBenchmarkFixture::supportsTimestamps(const VulkanQueue& queue) {
    return queue.getTimestampValidBits() > 0;
}

VulkanBenchmarkFixture::QueueResources& VulkanBenchmarkFixture::getResourcesFor(const VulkanQueue& queue) {
    auto& device = getDevice();
    const auto [it, inserted] = m_queueResources.try_emplace(queue.getHandle());
    if (inserted) {
        it->second.commandPool = std::make_unique<VulkanCommandPool>(
            queue.createCommandPool(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT),
            device.getResourceDeallocator());
        if (supportsTimestamps(queue)) {
            it->second.queryPool = std::make_unique<VulkanTimestampQueryPool>(
                device, queue, kTimestampQueryCount, "vulkanBenchmarkTimestamps");
        }
    }
    return it->second;
}

double VulkanBenchmarkFixture::measureDeviceSeconds(const VulkanBenchmarkRecorder& record) {
    return measureDeviceSeconds(getDevice().getGeneralQueue(), record);
}

double VulkanBenchmarkFixture::measureDeviceSeconds(
    const VulkanQueue& queue, const VulkanBenchmarkRecorder& record) {
    auto& device = getDevice();
    auto& resources = getResourcesFor(queue);
    const bool timestamped = resources.queryPool != nullptr;

    if (timestamped) {
        resources.queryPool->reset();
    }
    resources.commandPool->reset(device);

    const VkCommandBuffer cmdBuffer =
        resources.commandPool->allocateCommandBuffer(device, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmdBuffer, &beginInfo);

    const VulkanCommandEncoder encoder{cmdBuffer};
    if (timestamped) {
        encoder.writeTimestamp(*resources.queryPool, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0);
    }
    record(encoder);
    if (timestamped) {
        encoder.writeTimestamp(*resources.queryPool, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 1);
    }

    vkEndCommandBuffer(cmdBuffer);

    const VkFence fence = device.createFence(0);
    const auto cpuStart = std::chrono::steady_clock::now();
    queue.submit(cmdBuffer, fence);
    device.wait(fence);
    const auto cpuEnd = std::chrono::steady_clock::now();
    vkDestroyFence(device.getHandle(), fence, nullptr);

    if (!timestamped) {
        return std::chrono::duration<double>(cpuEnd - cpuStart).count();
    }

    std::array<uint64_t, kTimestampQueryCount> timestamps{};
    resources.queryPool->getResultsAndWait(timestamps);
    return resources.queryPool->getElapsedMilliseconds(timestamps[0], timestamps[1]) / 1000.0;
}

double VulkanBenchmarkFixture::measureConcurrentSeconds(const std::span<const VulkanBenchmarkWorkItem> work) {
    auto& device = getDevice();

    std::vector<VkCommandBuffer> cmdBuffers;
    std::vector<VkFence> fences;
    cmdBuffers.reserve(work.size());
    fences.reserve(work.size());

    for (const auto& [queue, record] : work) {
        auto& resources = getResourcesFor(*queue);
        resources.commandPool->reset(device);
        const VkCommandBuffer cmdBuffer =
            resources.commandPool->allocateCommandBuffer(device, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmdBuffer, &beginInfo);
        record(VulkanCommandEncoder{cmdBuffer});
        vkEndCommandBuffer(cmdBuffer);

        cmdBuffers.push_back(cmdBuffer);
        fences.push_back(device.createFence(0));
    }

    const auto cpuStart = std::chrono::steady_clock::now();
    for (size_t i = 0; i < work.size(); ++i) {
        work[i].first->submit(cmdBuffers[i], fences[i]);
    }
    for (const VkFence fence : fences) {
        device.wait(fence);
    }
    const auto cpuEnd = std::chrono::steady_clock::now();

    for (const VkFence fence : fences) {
        vkDestroyFence(device.getHandle(), fence, nullptr);
    }
    return std::chrono::duration<double>(cpuEnd - cpuStart).count();
}

void VulkanBenchmarkFixture::runTimedLoop(benchmark::State& state, const VulkanBenchmarkRecorder& record) {
    runTimedLoop(state, getDevice().getGeneralQueue(), record);
}

void VulkanBenchmarkFixture::runTimedLoop(
    benchmark::State& state, const VulkanQueue& queue, const VulkanBenchmarkRecorder& record) {
    for (uint32_t i = 0; i < kWarmUpIterations; ++i) {
        std::ignore = measureDeviceSeconds(queue, record);
    }
    for (auto _ : state) {
        state.SetIterationTime(measureDeviceSeconds(queue, record));
    }
}

void VulkanBenchmarkFixture::runConcurrentLoop(
    benchmark::State& state, const std::span<const VulkanBenchmarkWorkItem> work) {
    for (uint32_t i = 0; i < kWarmUpIterations; ++i) {
        std::ignore = measureConcurrentSeconds(work);
    }
    for (auto _ : state) {
        state.SetIterationTime(measureConcurrentSeconds(work));
    }
}

void setRateCounter(benchmark::State& state, const std::string_view name, const double unitsPerIteration) {
    state.counters[std::string(name)] =
        benchmark::Counter(unitsPerIteration, benchmark::Counter::kIsIterationInvariantRate);
}

void setEfficiencyCounter(
    benchmark::State& state, const std::string_view name, const double achieved, const double peak) {
    if (peak <= 0.0) {
        return;
    }
    state.counters[std::string(name)] = benchmark::Counter(100.0 * achieved / peak);
}

std::filesystem::path getBenchmarkShaderPath(const std::string_view fileName) {
    return std::filesystem::path(CRISP_BENCHMARK_ASSET_DIR) / fileName;
}

const std::filesystem::path& getBenchmarkShaderSpirv(const std::string_view fileName) {
    static std::mutex mutex;
    static FlatStringHashMap<std::filesystem::path> cache;
    static std::vector<std::unique_ptr<UniqueTemporaryFile>> ownedFiles;

    const std::scoped_lock lock(mutex);
    if (const auto it = cache.find(fileName); it != cache.end()) {
        return it->second;
    }

    auto spirvFile = std::make_unique<UniqueTemporaryFile>("spv", "vulkan-benchmark-");
    const auto outputPath = spirvFile->getPath();
    if (auto result = compileGlslShader(getBenchmarkShaderPath(fileName), outputPath); !result.isValid()) {
        throw std::runtime_error(std::move(result).getError());
    }

    ownedFiles.push_back(std::move(spirvFile));
    return cache.emplace(std::string(fileName), outputPath).first->second;
}

} // namespace crisp
