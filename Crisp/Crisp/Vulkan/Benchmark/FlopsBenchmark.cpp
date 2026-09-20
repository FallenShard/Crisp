#include <Crisp/Vulkan/Benchmark/VulkanBenchmark.hpp>

#include <Crisp/Renderer/ComputePipeline.hpp>
#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Vulkan/Rhi/VulkanBuffer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanQueue.hpp>

namespace crisp {
namespace {
constexpr uint32_t kWorkGroupSize = 256;

constexpr uint32_t kFmasPerIteration = 32;

constexpr uint32_t kWorkGroupCount = 4096;
constexpr uint32_t kInvocationCount = kWorkGroupCount * kWorkGroupSize;

struct FlopsParams {
    uint32_t iterations;
};

class FlopsFixture : public VulkanBenchmarkFixture {};
} // namespace

BENCHMARK_DEFINE_F(FlopsFixture, Fma32)(benchmark::State& state) {
    const auto iterations = static_cast<uint32_t>(state.range(0));

    auto& device = getDevice();

    auto pipeline =
        createComputePipeline(device, getBenchmarkShaderSpirv("flops.comp.glsl"), VkExtent3D{kWorkGroupSize, 1, 1});

    const VulkanBuffer sinkBuffer(
        device,
        static_cast<VkDeviceSize>(kInvocationCount) * sizeof(float),
        VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT,
        BufferMemoryType::GpuOnly);

    Material material(pipeline.get());
    material.writeDescriptor(0, 0, sinkBuffer.createDescriptorInfo());
    device.flushDescriptorUpdates();

    const FlopsParams params{.iterations = iterations};
    const auto record = [&](const VulkanCommandEncoder& encoder) {
        encoder.bindPipeline(*pipeline);
        encoder.bindDescriptorSets(material.getDescriptorSetBinding());
        encoder.setPushConstants(*pipeline->getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, params);
        encoder.dispatchCompute(VkExtent3D{kWorkGroupCount, 1, 1});
    };

    runTimedLoop(state, record);

    const double flops = 2.0 * static_cast<double>(kInvocationCount) * iterations * kFmasPerIteration;
    setRateCounter(state, "FLOP/s", flops);
}

BENCHMARK_REGISTER_F(FlopsFixture, Fma32)
    ->RangeMultiplier(4)
    ->Range(16, 1024)
    ->UseManualTime()
    ->Unit(benchmark::kMicrosecond);
} // namespace crisp

BENCHMARK_MAIN();
