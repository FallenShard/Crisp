#include <Crisp/Core/ChromeEventTracerIo.hpp>

#include <fstream>

#include <Crisp/Core/ChromeEventTracer.hpp>
#include <Crisp/Vulkan/VulkanTracer.hpp>

namespace crisp {
namespace {

template <typename TracingContext>
void serializeContext(const TracingContext& ctx, std::ofstream& output, const bool appendComma) {
    std::vector<uint32_t> stack;
    const auto& tracedEvents = ctx.getTracedEvents();
    for (uint32_t i = 0; i < tracedEvents.size(); ++i) {
        auto& event = tracedEvents[i];
        if (event.type == ScopeEventType::Begin) {
            stack.push_back(i);
        } else {
            const auto& beginEvent = tracedEvents[stack.back()];
            stack.pop_back();

            const auto beginUs = static_cast<double>(beginEvent.timestamp) / 1000.0;
            const auto durationUs = static_cast<double>(event.timestamp - beginEvent.timestamp) / 1000.0;
            const std::string_view comma = appendComma || i < tracedEvents.size() - 1 ? ",\n" : "\n";
            output << fmt::format(
                R"({{"name":{:?}, "ph":"X", "ts":{}, "dur":{}, "pid":{}}}{})",
                beginEvent.name.literal,
                beginUs,
                durationUs,
                1,
                comma);
        }
    }
}

} // namespace

void serializeTracedEvents(const std::filesystem::path& outputFile) {
    std::ofstream output(outputFile);
    output << "{\"traceEvents\":[\n";

    const size_t vulkanContextCount = detail::getTraceContexts().size();

    serializeContext(detail::getCpuContext(), output, vulkanContextCount > 0);

    for (uint32_t i = 0; i < vulkanContextCount; ++i) {
        const auto& ctx = detail::getTraceContexts()[i];
        serializeContext(*ctx, output, i != vulkanContextCount - 1);
    }

    output << "],\n";
    output << R"("meta_user":"FallenShard","meta_cpu_count":"16"})";
}

} // namespace crisp