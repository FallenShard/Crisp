#include <Crisp/Vulkan/Benchmark/VulkanBenchmark.hpp>

#include <Crisp/Renderer/ComputePipeline.hpp>
#include <Crisp/Renderer/Material.hpp>
#include <Crisp/ShaderUtils/ShaderCompiler.hpp>
#include <Crisp/Vulkan/Rhi/VulkanBuffer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanQueue.hpp>
#include <Crisp/Vulkan/VulkanSynchronization.hpp>

#include <array>
#include <utility>

namespace crisp {
namespace {
constexpr VkDeviceSize kBytesPerElement = sizeof(float) * 4;
constexpr uint32_t kWorkGroupSize = 256;

constexpr uint32_t kWorkGroupCount = 4096;

struct BandwidthParams {
    uint32_t elementCount;
};

class BandwidthFixture : public VulkanBenchmarkFixture {
protected:
    void runCopyBenchmark(benchmark::State& state, const VulkanQueue& queue) {
        const auto bufferMiB = static_cast<VkDeviceSize>(state.range(0));
        const VkDeviceSize bufferBytes = bufferMiB * 1024ULL * 1024ULL;

        auto& device = getDevice();
        constexpr auto kCopyUsage = VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT;
        const VulkanBuffer srcBuffer(device, bufferBytes, kCopyUsage, BufferMemoryType::GpuOnly);
        const VulkanBuffer dstBuffer(device, bufferBytes, kCopyUsage, BufferMemoryType::GpuOnly);

        runTimedLoop(state, queue, [&](const VulkanCommandEncoder& encoder) {
            encoder.copyBuffer(srcBuffer, dstBuffer);
        });

        setRateCounter(state, "bytes/s", 2.0 * static_cast<double>(bufferBytes));
        state.counters["MiB"] = benchmark::Counter(static_cast<double>(bufferMiB));
    }
};
} // namespace

BENCHMARK_DEFINE_F(BandwidthFixture, StreamCopy)(benchmark::State& state) {
    const auto bufferMiB = static_cast<VkDeviceSize>(state.range(0));
    const VkDeviceSize bufferBytes = bufferMiB * 1024ULL * 1024ULL;
    const auto elementCount = static_cast<uint32_t>(bufferBytes / kBytesPerElement);

    auto& device = getDevice();

    auto pipeline = createComputePipeline(
        device, getBenchmarkShaderSpirv("bandwidth.comp.glsl"), VkExtent3D{kWorkGroupSize, 1, 1});

    constexpr auto kStorageUsage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT;
    const VulkanBuffer srcBuffer(device, bufferBytes, kStorageUsage, BufferMemoryType::GpuOnly);
    const VulkanBuffer dstBuffer(device, bufferBytes, kStorageUsage, BufferMemoryType::GpuOnly);

    Material material(pipeline.get());
    material.writeDescriptor(0, 0, srcBuffer.createDescriptorInfo());
    material.writeDescriptor(0, 1, dstBuffer.createDescriptorInfo());
    device.flushDescriptorUpdates();

    const BandwidthParams params{.elementCount = elementCount};
    const auto record = [&](const VulkanCommandEncoder& encoder) {
        encoder.bindPipeline(*pipeline);
        encoder.bindDescriptorSets(material.getDescriptorSetBinding());
        encoder.setPushConstants(*pipeline->getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, params);
        encoder.dispatchCompute(VkExtent3D{kWorkGroupCount, 1, 1});
    };

    runTimedLoop(state, record);

    const double bytesMoved = 2.0 * static_cast<double>(bufferBytes);
    setRateCounter(state, "bytes/s", bytesMoved);
    state.counters["MiB"] = benchmark::Counter(static_cast<double>(bufferMiB));
}

BENCHMARK_REGISTER_F(BandwidthFixture, StreamCopy)
    ->RangeMultiplier(4)
    ->Range(1, 1024)
    ->UseManualTime()
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_DEFINE_F(BandwidthFixture, StreamRead)(benchmark::State& state) {
    const auto bufferMiB = static_cast<VkDeviceSize>(state.range(0));
    const VkDeviceSize bufferBytes = bufferMiB * 1024ULL * 1024ULL;
    const auto elementCount = static_cast<uint32_t>(bufferBytes / kBytesPerElement);

    auto& device = getDevice();

    auto pipeline = createComputePipeline(
        device, getBenchmarkShaderSpirv("bandwidth-read.comp.glsl"), VkExtent3D{kWorkGroupSize, 1, 1});

    const VulkanBuffer srcBuffer(
        device, bufferBytes, VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT, BufferMemoryType::GpuOnly);
    const VulkanBuffer sinkBuffer(
        device,
        static_cast<VkDeviceSize>(kWorkGroupCount) * kWorkGroupSize * kBytesPerElement,
        VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT,
        BufferMemoryType::GpuOnly);

    Material material(pipeline.get());
    material.writeDescriptor(0, 0, srcBuffer.createDescriptorInfo());
    material.writeDescriptor(0, 1, sinkBuffer.createDescriptorInfo());
    device.flushDescriptorUpdates();

    const auto groupCount = state.range(1) > 0 ? static_cast<uint32_t>(state.range(1)) : kWorkGroupCount;
    const BandwidthParams params{.elementCount = elementCount};
    runTimedLoop(state, [&](const VulkanCommandEncoder& encoder) {
        encoder.bindPipeline(*pipeline);
        encoder.bindDescriptorSets(material.getDescriptorSetBinding());
        encoder.setPushConstants(*pipeline->getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, params);
        encoder.dispatchCompute(VkExtent3D{groupCount, 1, 1});
    });

    setRateCounter(state, "bytes/s", static_cast<double>(bufferBytes));
    state.counters["MiB"] = benchmark::Counter(static_cast<double>(bufferMiB));
    state.counters["groups"] = benchmark::Counter(static_cast<double>(groupCount));
}

BENCHMARK_REGISTER_F(BandwidthFixture, StreamRead)
    ->ArgsProduct({{1024}, {256, 512, 1024, 2048, 4096, 8192, 16384, 32768}})
    ->UseManualTime()
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_DEFINE_F(BandwidthFixture, StreamWrite)(benchmark::State& state) {
    const auto bufferMiB = static_cast<VkDeviceSize>(state.range(0));
    const VkDeviceSize bufferBytes = bufferMiB * 1024ULL * 1024ULL;
    const auto elementCount = static_cast<uint32_t>(bufferBytes / kBytesPerElement);

    auto& device = getDevice();

    auto pipeline = createComputePipeline(
        device, getBenchmarkShaderSpirv("bandwidth-write.comp.glsl"), VkExtent3D{kWorkGroupSize, 1, 1});

    const VulkanBuffer dstBuffer(
        device, bufferBytes, VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT, BufferMemoryType::GpuOnly);

    Material material(pipeline.get());
    material.writeDescriptor(0, 0, dstBuffer.createDescriptorInfo());
    device.flushDescriptorUpdates();

    const BandwidthParams params{.elementCount = elementCount};
    runTimedLoop(state, [&](const VulkanCommandEncoder& encoder) {
        encoder.bindPipeline(*pipeline);
        encoder.bindDescriptorSets(material.getDescriptorSetBinding());
        encoder.setPushConstants(*pipeline->getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, params);
        encoder.dispatchCompute(VkExtent3D{kWorkGroupCount, 1, 1});
    });

    setRateCounter(state, "bytes/s", static_cast<double>(bufferBytes));
    state.counters["MiB"] = benchmark::Counter(static_cast<double>(bufferMiB));
}

BENCHMARK_REGISTER_F(BandwidthFixture, StreamWrite)
    ->Arg(256)
    ->Arg(1024)
    ->UseManualTime()
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_DEFINE_F(BandwidthFixture, TransferQueueCopy)(benchmark::State& state) {
    const auto& queue = getDevice().getTransferQueue();
    runCopyBenchmark(state, queue);
    state.counters["timestamped"] = benchmark::Counter(supportsTimestamps(queue) ? 1 : 0);
}

BENCHMARK_REGISTER_F(BandwidthFixture, TransferQueueCopy)
    ->Arg(256)
    ->Arg(1024)
    ->UseManualTime()
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_DEFINE_F(BandwidthFixture, GeneralQueueCopy)(benchmark::State& state) {
    runCopyBenchmark(state, getDevice().getGeneralQueue());
}

BENCHMARK_REGISTER_F(BandwidthFixture, GeneralQueueCopy)
    ->Arg(256)
    ->Arg(1024)
    ->UseManualTime()
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_DEFINE_F(BandwidthFixture, ConcurrentComputeAndDma)(benchmark::State& state) {
    const auto bufferMiB = static_cast<VkDeviceSize>(state.range(0));
    const VkDeviceSize bufferBytes = bufferMiB * 1024ULL * 1024ULL;
    const auto elementCount = static_cast<uint32_t>(bufferBytes / kBytesPerElement);

    auto& device = getDevice();

    auto pipeline = createComputePipeline(
        device, getBenchmarkShaderSpirv("bandwidth.comp.glsl"), VkExtent3D{kWorkGroupSize, 1, 1});

    constexpr auto kStorageUsage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT;
    const VulkanBuffer computeSrc(device, bufferBytes, kStorageUsage, BufferMemoryType::GpuOnly);
    const VulkanBuffer computeDst(device, bufferBytes, kStorageUsage, BufferMemoryType::GpuOnly);

    constexpr auto kCopyUsage = VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT;
    const VulkanBuffer dmaSrc(device, bufferBytes, kCopyUsage, BufferMemoryType::GpuOnly);
    const VulkanBuffer dmaDst(device, bufferBytes, kCopyUsage, BufferMemoryType::GpuOnly);

    Material material(pipeline.get());
    material.writeDescriptor(0, 0, computeSrc.createDescriptorInfo());
    material.writeDescriptor(0, 1, computeDst.createDescriptorInfo());
    device.flushDescriptorUpdates();

    const BandwidthParams params{.elementCount = elementCount};
    const std::array<VulkanBenchmarkWorkItem, 2> work{
        VulkanBenchmarkWorkItem{
            &device.getGeneralQueue(),
            [&](const VulkanCommandEncoder& encoder) {
                encoder.bindPipeline(*pipeline);
                encoder.bindDescriptorSets(material.getDescriptorSetBinding());
                encoder.setPushConstants(*pipeline->getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, params);
                encoder.dispatchCompute(VkExtent3D{kWorkGroupCount, 1, 1});
            }},
        VulkanBenchmarkWorkItem{
            &device.getTransferQueue(),
            [&](const VulkanCommandEncoder& encoder) { encoder.copyBuffer(dmaSrc, dmaDst); }},
    };

    runConcurrentLoop(state, work);

    setRateCounter(state, "bytes/s", 4.0 * static_cast<double>(bufferBytes));
    state.counters["MiB"] = benchmark::Counter(static_cast<double>(bufferMiB));
}

BENCHMARK_REGISTER_F(BandwidthFixture, ConcurrentComputeAndDma)
    ->Arg(512)
    ->Arg(1024)
    ->UseManualTime()
    ->Unit(benchmark::kMicrosecond);
} // namespace crisp

BENCHMARK_MAIN();
