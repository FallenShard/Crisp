#include <Crisp/Renderer/RenderGraph/RenderGraphGui.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <imgui.h>

namespace crisp {
namespace {

enum class SelectionType : uint8_t { None, Pass, Resource };

struct InspectorState {
    const rg::RenderGraph* graph{nullptr};
    SelectionType selectionType{SelectionType::None};
    uint32_t selectedIndex{0};
    ImGuiTextFilter passFilter;
    ImGuiTextFilter resourceFilter;
    ImGuiTextFilter timelineFilter;
};

// A resource with no producer and no reads is never touched by any pass; `isUsed` distinguishes that from
// a lifetime that legitimately starts at pass 0.
struct ResourceLifetime {
    uint32_t firstPass{0};
    uint32_t lastPass{0};
    bool isUsed{false};
};

// Per-frame derived data. Lifetimes are O(reads) to compute and are needed by the table, the sort comparator
// and the timeline, so they are built once instead of per comparison.
class GraphView {
public:
    explicit GraphView(const rg::RenderGraph& renderGraph)
        : m_graph(&renderGraph)
        , m_passTimings(renderGraph.getGpuPassTimingsMs())
        , m_graphTiming(renderGraph.getGpuFrameTimingMs()) {
        m_lifetimes.reserve(renderGraph.getResourceCount());
        for (const auto& resource : renderGraph.getResources()) {
            m_lifetimes.push_back(computeLifetime(resource));
        }
    }

    const rg::RenderGraph& graph() const {
        return *m_graph;
    }

    size_t passCount() const {
        return m_graph->getPassCount();
    }

    size_t resourceCount() const {
        return m_graph->getResourceCount();
    }

    const RenderGraphPass& pass(const size_t index) const {
        return m_graph->getPasses()[index];
    }

    const RenderGraphResource& resource(const size_t index) const {
        return m_graph->getResources()[index];
    }

    const ResourceLifetime& lifetime(const size_t index) const {
        return m_lifetimes[index];
    }

    bool isValidPass(const uint32_t passId) const {
        return passId < passCount();
    }

    bool isValidResource(const uint32_t resourceId) const {
        return resourceId < resourceCount();
    }

    // Timings lag the graph: they are empty until the first virtual frame is reused, and can be shorter than
    // the pass list if passes were added after compilation.
    std::optional<double> passTiming(const size_t passIndex) const {
        return passIndex < m_passTimings.size() ? m_passTimings[passIndex] : std::nullopt;
    }

    // Wall-clock GPU span from the first pass's begin to the last pass's end, so it also covers the gaps
    // between passes (barriers, layout transitions).
    std::optional<double> graphTiming() const {
        return m_graphTiming;
    }

    // Sum of the individual pass durations, which excludes those gaps. Nullopt until the first sample lands.
    std::optional<double> passTimingSum() const {
        std::optional<double> sum;
        for (size_t i = 0; i < timedPassCount(); ++i) {
            if (const auto timing = passTiming(i)) {
                sum = sum.value_or(0.0) + *timing;
            }
        }
        return sum;
    }

    size_t timedPassCount() const {
        return std::min(m_passTimings.size(), passCount());
    }

private:
    ResourceLifetime computeLifetime(const RenderGraphResource& resource) const {
        ResourceLifetime lifetime{};
        const auto extend = [&](const uint32_t passId) {
            if (!isValidPass(passId)) {
                return;
            }
            if (!lifetime.isUsed) {
                lifetime = {passId, passId, true};
                return;
            }
            lifetime.firstPass = std::min(lifetime.firstPass, passId);
            lifetime.lastPass = std::max(lifetime.lastPass, passId);
        };
        extend(resource.producer.id);
        for (const auto readPass : resource.readPasses) {
            extend(readPass.id);
        }
        return lifetime;
    }

    const rg::RenderGraph* m_graph;
    std::span<const std::optional<double>> m_passTimings;
    std::optional<double> m_graphTiming;
    std::vector<ResourceLifetime> m_lifetimes;
};

constexpr ImGuiTableFlags kTableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
                                        ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable |
                                        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp |
                                        ImGuiTableFlags_HighlightHoveredColumn;

constexpr size_t kPassTypeCount{3};
static_assert(static_cast<size_t>(PassType::RayTracing) + 1 == kPassTypeCount);

// Column identities are decoupled from column order, because the table is reorderable.
constexpr ImGuiID kPassColIndex{0};
constexpr ImGuiID kPassColName{1};
constexpr ImGuiID kPassColType{2};
constexpr ImGuiID kPassColGpu{3};
constexpr ImGuiID kPassColInputs{4};
constexpr ImGuiID kPassColOutputs{5};
constexpr ImGuiID kPassColShare{6};

constexpr ImGuiID kResColIndex{0};
constexpr ImGuiID kResColName{1};
constexpr ImGuiID kResColVersion{2};
constexpr ImGuiID kResColType{3};
constexpr ImGuiID kResColProducer{4};
constexpr ImGuiID kResColLifetime{5};
constexpr ImGuiID kResColPhysical{6};
constexpr ImGuiID kResColDescription{7};

template <typename T>
int compareValues(const T& lhs, const T& rhs) {
    const auto order = lhs <=> rhs;
    if (order < 0) {
        return -1;
    }
    return order > 0 ? 1 : 0;
}

const char* toString(const PassType type) {
    switch (type) {
    case PassType::Compute:
        return "Compute";
    case PassType::Rasterizer:
        return "Raster";
    case PassType::RayTracing:
        return "Ray tracing";
    }
    return "Unknown";
}

const char* toString(const ResourceType type) {
    switch (type) {
    case ResourceType::Buffer:
        return "Buffer";
    case ResourceType::Image:
        return "Image";
    case ResourceType::Unknown:
        return "Unknown";
    }
    return "Unknown";
}

const char* toString(const ResourceUsageType type) {
    switch (type) {
    case ResourceUsageType::Storage:
        return "Storage";
    case ResourceUsageType::Attachment:
        return "Attachment";
    case ResourceUsageType::Texture:
        return "Texture";
    }
    return "Unknown";
}

const char* toString(const SizePolicy policy) {
    switch (policy) {
    case SizePolicy::Absolute:
        return "Absolute";
    case SizePolicy::SwapChainRelative:
        return "Swapchain relative";
    case SizePolicy::InputRelative:
        return "Input relative";
    }
    return "Unknown";
}

const char* formatName(const VkFormat format) {
    switch (format) {
    case VK_FORMAT_UNDEFINED:
        return "Undefined";
    case VK_FORMAT_R8_UNORM:
        return "R8 UNORM";
    case VK_FORMAT_R8_UINT:
        return "R8 UINT";
    case VK_FORMAT_R8G8B8A8_UNORM:
        return "RGBA8 UNORM";
    case VK_FORMAT_R8G8B8A8_SRGB:
        return "RGBA8 sRGB";
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        return "RGBA16 float";
    case VK_FORMAT_R32G32_SFLOAT:
        return "RG32 float";
    case VK_FORMAT_R32G32B32_SFLOAT:
        return "RGB32 float";
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        return "RGBA32 float";
    case VK_FORMAT_D24_UNORM_S8_UINT:
        return "D24 UNORM S8 UINT";
    case VK_FORMAT_D32_SFLOAT:
        return "D32 float";
    default:
        return "Other";
    }
}

ImVec4 passColor(const PassType type) {
    switch (type) {
    case PassType::Compute:
        return {0.35f, 0.72f, 1.00f, 1.00f};
    case PassType::Rasterizer:
        return {0.42f, 0.82f, 0.48f, 1.00f};
    case PassType::RayTracing:
        return {0.82f, 0.52f, 1.00f, 1.00f};
    }
    return {0.75f, 0.75f, 0.75f, 1.00f};
}

char physicalPrefix(const ResourceType type) {
    return type == ResourceType::Image ? 'I' : 'B';
}

// Resources sharing a physical allocation get the same hue, so aliasing groups read at a glance in both the
// resource table and the timeline. 47 and 101 are coprime, which spreads adjacent indices apart on the wheel.
ImU32 physicalResourceColor(const RenderGraphResource& resource, const float alpha = 1.0f) {
    if (resource.isExternal || resource.physicalResourceIndex == RenderGraphResource::kInvalidIndex) {
        return ImGui::GetColorU32(ImGuiCol_TextDisabled, alpha);
    }
    const auto typeOffset = static_cast<uint32_t>(resource.type) * 17u;
    const float hue = static_cast<float>((resource.physicalResourceIndex * 47u + typeOffset) % 101u) / 101.0f;
    return ImGui::GetColorU32(ImColor::HSV(hue, 0.55f, 0.82f, alpha).Value);
}

std::string resourceLabel(const RenderGraphResource& resource) {
    return resource.name + " v" + std::to_string(resource.version);
}

void appendFlag(std::string& value, const uint64_t flags, const uint64_t flag, const char* name) {
    if ((flags & flag) == 0) {
        return;
    }
    if (!value.empty()) {
        value += " | ";
    }
    value += name;
}

std::string imageUsage(const VkImageUsageFlags flags) {
    std::string value;
    appendFlag(value, flags, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, "Transfer src");
    appendFlag(value, flags, VK_IMAGE_USAGE_TRANSFER_DST_BIT, "Transfer dst");
    appendFlag(value, flags, VK_IMAGE_USAGE_SAMPLED_BIT, "Sampled");
    appendFlag(value, flags, VK_IMAGE_USAGE_STORAGE_BIT, "Storage");
    appendFlag(value, flags, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, "Color attachment");
    appendFlag(value, flags, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, "Depth attachment");
    appendFlag(value, flags, VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, "Input attachment");
    return value.empty() ? "None" : value;
}

std::string bufferUsage(const VkBufferUsageFlags flags) {
    std::string value;
    appendFlag(value, flags, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, "Transfer src");
    appendFlag(value, flags, VK_BUFFER_USAGE_TRANSFER_DST_BIT, "Transfer dst");
    appendFlag(value, flags, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, "Uniform texel");
    appendFlag(value, flags, VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT, "Storage texel");
    appendFlag(value, flags, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, "Uniform");
    appendFlag(value, flags, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "Storage");
    appendFlag(value, flags, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, "Index");
    appendFlag(value, flags, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, "Vertex");
    appendFlag(value, flags, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, "Indirect");
    return value.empty() ? "None" : value;
}

std::string pipelineStages(const VkPipelineStageFlags2 flags) {
    std::string value;
    appendFlag(value, flags, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, "Top");
    appendFlag(value, flags, VK_PIPELINE_STAGE_2_TRANSFER_BIT, "Transfer");
    appendFlag(value, flags, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, "Compute");
    appendFlag(value, flags, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, "Vertex shader");
    appendFlag(value, flags, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, "Fragment shader");
    appendFlag(value, flags, VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT, "Vertex input");
    appendFlag(value, flags, VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT, "Index input");
    appendFlag(value, flags, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, "Color output");
    appendFlag(value, flags, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, "Early tests");
    appendFlag(value, flags, VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, "Late tests");
    appendFlag(value, flags, VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR, "Ray tracing");
    appendFlag(value, flags, VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, "Acceleration structure");
    appendFlag(value, flags, VK_PIPELINE_STAGE_2_HOST_BIT, "Host");
    appendFlag(value, flags, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, "Bottom");
    appendFlag(value, flags, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, "All commands");
    return value.empty() ? "None" : value;
}

std::string accessFlags(const VkAccessFlags2 flags) {
    std::string value;
    appendFlag(value, flags, VK_ACCESS_2_TRANSFER_READ_BIT, "Transfer read");
    appendFlag(value, flags, VK_ACCESS_2_TRANSFER_WRITE_BIT, "Transfer write");
    appendFlag(value, flags, VK_ACCESS_2_SHADER_READ_BIT, "Shader read");
    appendFlag(value, flags, VK_ACCESS_2_SHADER_WRITE_BIT, "Shader write");
    appendFlag(value, flags, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, "Sampled read");
    appendFlag(value, flags, VK_ACCESS_2_SHADER_STORAGE_READ_BIT, "Storage read");
    appendFlag(value, flags, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, "Storage write");
    appendFlag(value, flags, VK_ACCESS_2_UNIFORM_READ_BIT, "Uniform read");
    appendFlag(value, flags, VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT, "Input attachment read");
    appendFlag(value, flags, VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT, "Vertex read");
    appendFlag(value, flags, VK_ACCESS_2_INDEX_READ_BIT, "Index read");
    appendFlag(value, flags, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, "Color write");
    appendFlag(value, flags, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, "Depth write");
    appendFlag(value, flags, VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, "AS write");
    appendFlag(value, flags, VK_ACCESS_2_HOST_READ_BIT, "Host read");
    appendFlag(value, flags, VK_ACCESS_2_HOST_WRITE_BIT, "Host write");
    appendFlag(value, flags, VK_ACCESS_2_MEMORY_READ_BIT, "Memory read");
    appendFlag(value, flags, VK_ACCESS_2_MEMORY_WRITE_BIT, "Memory write");
    return value.empty() ? "None" : value;
}

std::string milliseconds(const double value) {
    char text[32];
    std::snprintf(text, std::size(text), "%.3f ms", value);
    return text;
}

std::string byteSize(const VkDeviceSize size) {
    char text[32];
    if (size >= 1024 * 1024) {
        std::snprintf(text, std::size(text), "%.2f MiB", static_cast<double>(size) / (1024.0 * 1024.0));
    } else if (size >= 1024) {
        std::snprintf(text, std::size(text), "%.2f KiB", static_cast<double>(size) / 1024.0);
    } else {
        std::snprintf(text, std::size(text), "%llu B", static_cast<unsigned long long>(size));
    }
    return text;
}

bool isPassSelected(const InspectorState& state, const size_t passIndex) {
    return state.selectionType == SelectionType::Pass && state.selectedIndex == passIndex;
}

bool isResourceSelected(const InspectorState& state, const size_t resourceIndex) {
    return state.selectionType == SelectionType::Resource && state.selectedIndex == resourceIndex;
}

void clearSelection(InspectorState& state) {
    state.selectionType = SelectionType::None;
    state.selectedIndex = 0;
}

// Re-activating the current item clears it. Cross-links never point at their own owner, so a link can never
// deselect the thing the user just navigated from.
void selectPass(InspectorState& state, const uint32_t passIndex) {
    if (isPassSelected(state, passIndex)) {
        clearSelection(state);
        return;
    }
    state.selectionType = SelectionType::Pass;
    state.selectedIndex = passIndex;
}

void selectResource(InspectorState& state, const uint32_t resourceIndex) {
    if (isResourceSelected(state, resourceIndex)) {
        clearSelection(state);
        return;
    }
    state.selectionType = SelectionType::Resource;
    state.selectedIndex = resourceIndex;
}

// Returns whether a link was drawn, not whether it was clicked - callers use it to fall back to a placeholder.
bool drawResourceLink(const GraphView& view, InspectorState& state, const RenderGraphResourceHandle handle) {
    if (!view.isValidResource(handle.id)) {
        return false;
    }
    const auto label = resourceLabel(view.resource(handle.id));
    ImGui::PushID(static_cast<int>(handle.id));
    const bool clicked = ImGui::TextLink(label.c_str());
    ImGui::PopID();
    if (clicked) {
        selectResource(state, handle.id);
    }
    return true;
}

bool drawPassLink(const GraphView& view, InspectorState& state, const RenderGraphPassHandle handle) {
    if (!view.isValidPass(handle.id)) {
        return false;
    }
    const auto& pass = view.pass(handle.id);
    ImGui::PushID(static_cast<int>(handle.id));
    const bool clicked = ImGui::TextLink(pass.name.c_str());
    ImGui::PopID();
    ImGui::SetItemTooltip("Pass %u, %s", handle.id, toString(pass.type)); // NOLINT
    if (clicked) {
        selectPass(state, handle.id);
    }
    return true;
}

void drawMetric(const char* label, const std::string& value) {
    ImGui::TableNextColumn();
    ImGui::TextDisabled("%s", label); // NOLINT
    ImGui::TextUnformatted(value.c_str());
}

void drawOverview(const GraphView& view) {
    size_t imageCount = 0;
    size_t bufferCount = 0;
    size_t externalCount = 0;
    for (const auto& resource : view.graph().getResources()) {
        imageCount += resource.type == ResourceType::Image ? 1 : 0;
        bufferCount += resource.type == ResourceType::Buffer ? 1 : 0;
        externalCount += resource.isExternal ? 1 : 0;
    }
    const auto physicalCount = view.graph().getPhysicalImageCount() + view.graph().getPhysicalBufferCount();
    const auto internalCount = view.resourceCount() - externalCount;
    const auto savedAllocations = internalCount > physicalCount ? internalCount - physicalCount : 0;

    std::optional<size_t> slowestPass;
    for (size_t i = 0; i < view.timedPassCount(); ++i) {
        const auto timing = view.passTiming(i);
        if (timing && (!slowestPass || *timing > *view.passTiming(*slowestPass))) {
            slowestPass = i;
        }
    }

    constexpr auto kMetricTableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame;
    if (ImGui::BeginTable("##RenderGraphStructure", 3, kMetricTableFlags)) {
        drawMetric("PASSES", std::to_string(view.passCount()));
        drawMetric("LOGICAL RESOURCES", std::to_string(view.resourceCount()));
        drawMetric("IMAGES / BUFFERS", std::to_string(imageCount) + " / " + std::to_string(bufferCount));
        drawMetric("EXTERNAL", std::to_string(externalCount));
        drawMetric("PHYSICAL ALLOCATIONS", std::to_string(physicalCount));
        drawMetric("ALIASED ALLOCATIONS SAVED", std::to_string(savedAllocations));
        ImGui::EndTable();
    }

    ImGui::Spacing();
    const auto graphTime = view.graphTiming();
    const auto passSum = view.passTimingSum();
    const char* pendingText = view.graph().isGpuProfilingSupported() ? "Pending" : "Unsupported";
    if (ImGui::BeginTable("##RenderGraphTiming", 4, kMetricTableFlags)) {
        drawMetric("GPU TOTAL", graphTime ? milliseconds(*graphTime) : pendingText);
        drawMetric("SUM OF PASSES", passSum ? milliseconds(*passSum) : pendingText);
        // The span covers barriers and layout transitions between passes; the per-pass timers do not.
        if (graphTime && passSum) {
            drawMetric("BETWEEN PASSES", milliseconds(std::max(0.0, *graphTime - *passSum)));
        } else {
            drawMetric("BETWEEN PASSES", pendingText);
        }
        if (slowestPass) {
            drawMetric(
                "SLOWEST PASS",
                view.pass(*slowestPass).name + " (" + milliseconds(*view.passTiming(*slowestPass)) + ")");
        } else {
            drawMetric("SLOWEST PASS", pendingText);
        }
        ImGui::EndTable();
    }
    ImGui::Spacing();
    std::array<size_t, kPassTypeCount> typeCounts{};
    for (const auto& pass : view.graph().getPasses()) {
        ++typeCounts.at(static_cast<size_t>(pass.type));
    }
    for (const auto type : {PassType::Rasterizer, PassType::Compute, PassType::RayTracing}) {
        ImGui::TextColored(passColor(type), "%s: %zu", toString(type), typeCounts[static_cast<size_t>(type)]); // NOLINT
        if (type != PassType::RayTracing) {
            ImGui::SameLine(0.0f, 24.0f);
        }
    }
    ImGui::Spacing();
    ImGui::TextDisabled("(?)");
    ImGui::SetItemTooltip(
        "GPU TOTAL is the span from the first pass's begin to the last pass's end, so it also covers the gaps "
        "between passes. SUM OF PASSES adds up the individual pass timers only, and BETWEEN PASSES is the "
        "difference: barriers, layout transitions and idle time.");
    ImGui::SameLine();
    ImGui::TextWrapped(
        "GPU timings are read asynchronously when a virtual frame is reused. Static graph metadata remains available "
        "while the first sample is pending.");
}

void sortPassIndices(std::vector<size_t>& indices, const GraphView& view, const ImGuiTableSortSpecs* specs) {
    if (!specs || specs->SpecsCount == 0) {
        return;
    }
    const auto& sort = specs->Specs[0];
    const auto compare = [&](const size_t lhsIndex, const size_t rhsIndex) {
        const auto& lhs = view.pass(lhsIndex);
        const auto& rhs = view.pass(rhsIndex);
        int result = 0;
        switch (sort.ColumnUserID) {
        case kPassColName:
            result = compareValues(lhs.name, rhs.name);
            break;
        case kPassColType:
            result = compareValues(lhs.type, rhs.type);
            break;
        case kPassColGpu:
            result = compareValues(
                view.passTiming(lhsIndex).value_or(-1.0), view.passTiming(rhsIndex).value_or(-1.0));
            break;
        case kPassColInputs:
            result = compareValues(lhs.inputs.size(), rhs.inputs.size());
            break;
        case kPassColOutputs:
            result = compareValues(lhs.outputs.size(), rhs.outputs.size());
            break;
        case kPassColIndex:
        default:
            break;
        }
        if (result == 0) {
            result = compareValues(lhsIndex, rhsIndex);
        }
        return sort.SortDirection == ImGuiSortDirection_Ascending ? result < 0 : result > 0;
    };
    std::sort(indices.begin(), indices.end(), compare);
}

void drawPassTable(const GraphView& view, InspectorState& state) {
    state.passFilter.Draw("Filter passes", 240.0f);
    ImGui::SameLine();
    ImGui::TextDisabled("Click a row for details, click it again to clear");
    const auto graphTime = view.graphTiming();
    std::vector<size_t> indices(view.passCount());
    std::iota(indices.begin(), indices.end(), 0);

    if (!ImGui::BeginTable("##RenderGraphPasses", 7, kTableFlags, ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 14.0f))) {
        return;
    }
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn(
        "#", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, 32.0f, kPassColIndex);
    ImGui::TableSetupColumn("Pass", ImGuiTableColumnFlags_WidthStretch, 2.0f, kPassColName);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 88.0f, kPassColType);
    ImGui::TableSetupColumn("GPU", ImGuiTableColumnFlags_WidthFixed, 76.0f, kPassColGpu);
    ImGui::TableSetupColumn(
        "Graph %", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed, 110.0f, kPassColShare);
    ImGui::TableSetupColumn("Inputs", ImGuiTableColumnFlags_WidthFixed, 54.0f, kPassColInputs);
    ImGui::TableSetupColumn("Outputs", ImGuiTableColumnFlags_WidthFixed, 60.0f, kPassColOutputs);
    ImGui::TableHeadersRow();
    sortPassIndices(indices, view, ImGui::TableGetSortSpecs());

    for (const size_t passIndex : indices) {
        const auto& pass = view.pass(passIndex);
        if (!state.passFilter.PassFilter(pass.name.c_str())) {
            continue;
        }
        ImGui::PushID(static_cast<int>(passIndex));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%zu", passIndex); // NOLINT
        ImGui::TableNextColumn();
        if (ImGui::Selectable(pass.name.c_str(), isPassSelected(state, passIndex), ImGuiSelectableFlags_SpanAllColumns)) {
            selectPass(state, static_cast<uint32_t>(passIndex));
        }
        ImGui::TableNextColumn();
        ImGui::TextColored(passColor(pass.type), "%s", toString(pass.type)); // NOLINT
        ImGui::TableNextColumn();
        const auto timing = view.passTiming(passIndex);
        if (timing) {
            ImGui::Text("%.3f ms", *timing); // NOLINT
        } else {
            ImGui::TextDisabled("--");
        }
        ImGui::TableNextColumn();
        if (timing && graphTime && *graphTime > 0.0) {
            const float share = static_cast<float>(*timing / *graphTime);
            char label[24];
            std::snprintf(label, std::size(label), "%.1f%%", share * 100.0f);
            // A default-height bar is GetFrameHeight() tall and would inflate the row past the text-height cells
            // beside it, leaving them top-aligned against a centered bar label.
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, passColor(pass.type));
            ImGui::ProgressBar(
                std::clamp(share, 0.0f, 1.0f), ImVec2(-1.0f, ImGui::GetTextLineHeight()), label);
            ImGui::PopStyleColor();
        } else {
            ImGui::TextDisabled("--");
        }
        ImGui::TableNextColumn();
        ImGui::Text("%zu", pass.inputs.size()); // NOLINT
        ImGui::TableNextColumn();
        ImGui::Text("%zu", pass.outputs.size()); // NOLINT
        ImGui::PopID();
    }
    ImGui::EndTable();

    if (const auto passSum = view.passTimingSum()) {
        ImGui::Text("Sum of passes: %s", milliseconds(*passSum).c_str()); // NOLINT
        if (graphTime) {
            ImGui::SameLine();
            ImGui::TextDisabled( // NOLINT
                "| GPU total %s | %s between passes",
                milliseconds(*graphTime).c_str(),
                milliseconds(std::max(0.0, *graphTime - *passSum)).c_str());
        }
    } else {
        ImGui::TextDisabled("Sum of passes: pending");
    }
}

void sortResourceIndices(std::vector<size_t>& indices, const GraphView& view, const ImGuiTableSortSpecs* specs) {
    if (!specs || specs->SpecsCount == 0) {
        return;
    }
    const auto& sort = specs->Specs[0];
    // Resources never touched by a pass sort past every real lifetime rather than aliasing onto pass 0.
    const auto lifetimeKey = [&](const size_t index) {
        const auto& lifetime = view.lifetime(index);
        return lifetime.isUsed ? static_cast<size_t>(lifetime.firstPass) : view.passCount();
    };
    const auto compare = [&](const size_t lhsIndex, const size_t rhsIndex) {
        const auto& lhs = view.resource(lhsIndex);
        const auto& rhs = view.resource(rhsIndex);
        int result = 0;
        switch (sort.ColumnUserID) {
        case kResColName:
            result = compareValues(lhs.name, rhs.name);
            break;
        case kResColVersion:
            result = compareValues(lhs.version, rhs.version);
            break;
        case kResColType:
            result = compareValues(lhs.type, rhs.type);
            break;
        case kResColProducer:
            result = compareValues(lhs.producer.id, rhs.producer.id);
            break;
        case kResColLifetime:
            result = compareValues(lifetimeKey(lhsIndex), lifetimeKey(rhsIndex));
            break;
        case kResColPhysical:
            result = compareValues(lhs.physicalResourceIndex, rhs.physicalResourceIndex);
            break;
        case kResColIndex:
        default:
            break;
        }
        if (result == 0) {
            result = compareValues(lhsIndex, rhsIndex);
        }
        return sort.SortDirection == ImGuiSortDirection_Ascending ? result < 0 : result > 0;
    };
    std::sort(indices.begin(), indices.end(), compare);
}

void drawResourceTable(const GraphView& view, InspectorState& state) {
    state.resourceFilter.Draw("Filter resources", 240.0f);
    ImGui::SameLine();
    ImGui::TextDisabled("Versions sharing a physical ID are aliased");
    std::vector<size_t> indices(view.resourceCount());
    std::iota(indices.begin(), indices.end(), 0);

    if (!ImGui::BeginTable("##RenderGraphResources", 8, kTableFlags, ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 14.0f))) {
        return;
    }
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn(
        "#", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, 32.0f, kResColIndex);
    ImGui::TableSetupColumn("Resource", ImGuiTableColumnFlags_WidthStretch, 2.0f, kResColName);
    ImGui::TableSetupColumn("v", ImGuiTableColumnFlags_WidthFixed, 28.0f, kResColVersion);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 58.0f, kResColType);
    ImGui::TableSetupColumn("Producer", ImGuiTableColumnFlags_WidthStretch, 1.2f, kResColProducer);
    ImGui::TableSetupColumn("Lifetime", ImGuiTableColumnFlags_WidthFixed, 72.0f, kResColLifetime);
    ImGui::TableSetupColumn("Physical", ImGuiTableColumnFlags_WidthFixed, 66.0f, kResColPhysical);
    ImGui::TableSetupColumn(
        "Description", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthStretch, 1.7f, kResColDescription);
    ImGui::TableHeadersRow();
    sortResourceIndices(indices, view, ImGui::TableGetSortSpecs());

    for (const size_t resourceIndex : indices) {
        const auto& resource = view.resource(resourceIndex);
        const auto label = resourceLabel(resource);
        if (!state.resourceFilter.PassFilter(label.c_str())) {
            continue;
        }
        const auto& lifetime = view.lifetime(resourceIndex);
        ImGui::PushID(static_cast<int>(resourceIndex));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%zu", resourceIndex); // NOLINT
        ImGui::TableNextColumn();
        if (ImGui::Selectable(
                resource.name.c_str(), isResourceSelected(state, resourceIndex), ImGuiSelectableFlags_SpanAllColumns)) {
            selectResource(state, static_cast<uint32_t>(resourceIndex));
        }
        ImGui::TableNextColumn();
        ImGui::Text("%u", resource.version); // NOLINT
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(toString(resource.type));
        ImGui::TableNextColumn();
        if (view.isValidPass(resource.producer.id)) {
            ImGui::TextUnformatted(view.pass(resource.producer.id).name.c_str());
        } else {
            ImGui::TextDisabled("External");
        }
        ImGui::TableNextColumn();
        if (lifetime.isUsed) {
            ImGui::Text("%u - %u", lifetime.firstPass, lifetime.lastPass); // NOLINT
        } else {
            ImGui::TextDisabled("--");
        }
        ImGui::TableNextColumn();
        if (resource.isExternal) {
            ImGui::TextDisabled("External");
        } else {
            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, physicalResourceColor(resource, 0.30f));
            ImGui::Text("%c%u", physicalPrefix(resource.type), resource.physicalResourceIndex); // NOLINT
        }
        ImGui::TableNextColumn();
        const RenderGraphResourceHandle handle{static_cast<uint32_t>(resourceIndex)};
        if (resource.type == ResourceType::Image) {
            const auto extent = view.graph().getImageExtent(handle);
            const auto& description = view.graph().getImageDescription(handle);
            ImGui::Text(
                "%ux%ux%u, %s", extent.width, extent.height, extent.depth, formatName(description.format)); // NOLINT
        } else if (resource.type == ResourceType::Buffer) {
            ImGui::TextUnformatted(byteSize(view.graph().getBufferDescription(handle).size).c_str());
        }
        ImGui::PopID();
    }
    ImGui::EndTable();
}

void drawTimelineHeader(
    const GraphView& view,
    InspectorState& state,
    ImDrawList& drawList,
    const float totalWidth,
    const float labelWidth,
    const float cellWidth,
    const float height) {
    const ImVec2 headerMin = ImGui::GetCursorScreenPos();
    drawList.AddRectFilled(
        headerMin, {headerMin.x + totalWidth, headerMin.y + height}, ImGui::GetColorU32(ImGuiCol_TableHeaderBg));
    drawList.AddText({headerMin.x + 5.0f, headerMin.y + 4.0f}, ImGui::GetColorU32(ImGuiCol_Text), "Execution order");

    // Each cell is a real item rather than a raw rect test, so hovering respects window overlap, clipping and the
    // configured tooltip delay.
    for (size_t passIndex = 0; passIndex < view.passCount(); ++passIndex) {
        const auto& pass = view.pass(passIndex);
        ImGui::SetCursorScreenPos({headerMin.x + labelWidth + cellWidth * static_cast<float>(passIndex), headerMin.y});
        ImGui::PushID(static_cast<int>(passIndex));
        ImGui::InvisibleButton("##TimelinePass", ImVec2(cellWidth, height));
        if (ImGui::IsItemClicked()) {
            selectPass(state, static_cast<uint32_t>(passIndex));
        }
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const bool selected = isPassSelected(state, passIndex);
        drawList.AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(passColor(pass.type)), 2.0f);
        drawList.AddRect(
            min, max, selected ? IM_COL32_WHITE : IM_COL32(20, 20, 20, 180), 2.0f, 0, selected ? 2.0f : 1.0f);
        drawList.AddText(
            ImGui::GetFont(),
            ImGui::GetFontSize() * 0.85f,
            {min.x + 4.0f, min.y + 4.0f},
            IM_COL32(15, 15, 15, 255),
            std::to_string(passIndex).c_str());

        if (ImGui::BeginItemTooltip()) {
            ImGui::TextUnformatted(pass.name.c_str());
            ImGui::TextDisabled("Pass %zu, %s", passIndex, toString(pass.type)); // NOLINT
            if (const auto timing = view.passTiming(passIndex)) {
                ImGui::Text("%.3f ms", *timing); // NOLINT
            }
            ImGui::EndTooltip();
        }
        ImGui::PopID();
    }

    // Claim the header strip in the layout, now that the cells were placed at explicit positions.
    ImGui::SetCursorScreenPos(headerMin);
    ImGui::Dummy(ImVec2(totalWidth, height));
}

void drawTimeline(const GraphView& view, InspectorState& state) {
    state.timelineFilter.Draw("Filter resources", 240.0f);
    ImGui::SameLine();
    ImGui::TextDisabled("Bars show first-to-last pass use; colors identify physical allocations");
    if (view.passCount() == 0) {
        ImGui::TextDisabled("No passes");
        return;
    }

    constexpr float kLabelWidth = 190.0f;
    constexpr float kMinPassWidth = 34.0f;
    const auto passCount = static_cast<float>(view.passCount());
    const float rowHeight = ImGui::GetTextLineHeightWithSpacing() + 4.0f;
    const float availableLaneWidth = std::max(100.0f, ImGui::GetContentRegionAvail().x - kLabelWidth);
    const float laneWidth = std::max(availableLaneWidth, kMinPassWidth * passCount);
    const float totalWidth = kLabelWidth + laneWidth;
    const float cellWidth = laneWidth / passCount;

    std::vector<size_t> visibleIndices;
    visibleIndices.reserve(view.resourceCount());
    for (size_t resourceIndex = 0; resourceIndex < view.resourceCount(); ++resourceIndex) {
        if (state.timelineFilter.PassFilter(resourceLabel(view.resource(resourceIndex)).c_str())) {
            visibleIndices.push_back(resourceIndex);
        }
    }

    ImGui::BeginChild(
        "##RenderGraphTimeline",
        ImVec2(0.0f, rowHeight * 14.0f),
        ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY,
        ImGuiWindowFlags_HorizontalScrollbar);
    auto* drawList = ImGui::GetWindowDrawList();
    drawTimelineHeader(view, state, *drawList, totalWidth, kLabelWidth, cellWidth, rowHeight * 1.6f);

    // The lane separators span every row, so they are emitted once beneath the rows instead of per row.
    const ImVec2 gridOrigin = ImGui::GetCursorScreenPos();
    const float gridBottom = gridOrigin.y + rowHeight * static_cast<float>(visibleIndices.size());
    for (size_t passIndex = 0; passIndex <= view.passCount(); ++passIndex) {
        const float x = gridOrigin.x + kLabelWidth + cellWidth * static_cast<float>(passIndex);
        drawList->AddLine({x, gridOrigin.y}, {x, gridBottom}, ImGui::GetColorU32(ImGuiCol_Border));
    }

    for (size_t rowIndex = 0; rowIndex < visibleIndices.size(); ++rowIndex) {
        const size_t resourceIndex = visibleIndices[rowIndex];
        const auto& resource = view.resource(resourceIndex);
        const auto label = resourceLabel(resource);
        const auto& lifetime = view.lifetime(resourceIndex);
        ImGui::PushID(static_cast<int>(resourceIndex));
        ImGui::InvisibleButton("##TimelineResource", ImVec2(totalWidth, rowHeight));
        const ImVec2 rowMin = ImGui::GetItemRectMin();
        const ImVec2 rowMax = ImGui::GetItemRectMax();
        const bool selected = isResourceSelected(state, resourceIndex);
        if (selected) {
            drawList->AddRectFilled(rowMin, rowMax, ImGui::GetColorU32(ImGuiCol_Header));
        } else if ((rowIndex & 1u) != 0) {
            drawList->AddRectFilled(rowMin, rowMax, ImGui::GetColorU32(ImGuiCol_TableRowBgAlt));
        }
        const ImVec4 clipRect{rowMin.x, rowMin.y, rowMin.x + kLabelWidth - 5.0f, rowMax.y};
        drawList->AddText(
            ImGui::GetFont(),
            ImGui::GetFontSize(),
            {rowMin.x + 5.0f, rowMin.y + 3.0f},
            ImGui::GetColorU32(ImGuiCol_Text),
            label.c_str(),
            nullptr,
            0.0f,
            &clipRect);
        if (lifetime.isUsed) {
            const ImVec2 barMin{
                rowMin.x + kLabelWidth + cellWidth * static_cast<float>(lifetime.firstPass) + 2.0f, rowMin.y + 3.0f};
            const ImVec2 barMax{
                rowMin.x + kLabelWidth + cellWidth * static_cast<float>(lifetime.lastPass + 1) - 2.0f, rowMax.y - 3.0f};
            drawList->AddRectFilled(barMin, barMax, physicalResourceColor(resource), 3.0f);
            if (selected) {
                drawList->AddRect(barMin, barMax, IM_COL32_WHITE, 3.0f, 0, 2.0f);
            }
        }
        if (ImGui::IsItemClicked()) {
            selectResource(state, static_cast<uint32_t>(resourceIndex));
        }
        if (ImGui::BeginItemTooltip()) {
            ImGui::TextUnformatted(label.c_str());
            if (resource.isExternal) {
                ImGui::Text("%s, external", toString(resource.type)); // NOLINT
            } else {
                ImGui::Text(
                    "%s, physical %c%u",
                    toString(resource.type),
                    physicalPrefix(resource.type),
                    resource.physicalResourceIndex); // NOLINT
            }
            if (lifetime.isUsed) {
                ImGui::Text("Passes %u - %u", lifetime.firstPass, lifetime.lastPass); // NOLINT
            } else {
                ImGui::TextDisabled("Never used by a pass");
            }
            ImGui::EndTooltip();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
}

void drawAccessCell(const ResourceAccessState& access) {
    ImGui::TextUnformatted(toString(access.usageType));
    ImGui::TableNextColumn();
    ImGui::TextWrapped( // NOLINT
        "%s / %s",
        pipelineStages(access.stage.stage).c_str(),
        accessFlags(access.stage.access).c_str());
}

void drawPassInspector(const GraphView& view, InspectorState& state, const uint32_t passIndex) {
    const auto& pass = view.pass(passIndex);
    ImGui::TextUnformatted(pass.name.c_str());
    ImGui::SameLine();
    ImGui::TextColored(passColor(pass.type), "[%s]", toString(pass.type)); // NOLINT
    if (const auto timing = view.passTiming(passIndex)) {
        ImGui::SameLine();
        ImGui::Text("%.3f ms", *timing); // NOLINT
    }

    if (ImGui::BeginTable("##PassInspector", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Direction", ImGuiTableColumnFlags_WidthFixed, 58.0f);
        ImGui::TableSetupColumn("Resource", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Usage", ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableSetupColumn("Stage / access", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        for (size_t inputIndex = 0; inputIndex < pass.inputs.size(); ++inputIndex) {
            ImGui::PushID(static_cast<int>(inputIndex));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled("IN");
            ImGui::TableNextColumn();
            if (!drawResourceLink(view, state, pass.inputs[inputIndex])) {
                ImGui::TextDisabled("Invalid handle");
            }
            ImGui::TableNextColumn();
            if (inputIndex < pass.inputAccesses.size()) {
                drawAccessCell(pass.inputAccesses[inputIndex]);
            } else {
                ImGui::TextDisabled("--");
                ImGui::TableNextColumn();
                ImGui::TextDisabled("--");
            }
            ImGui::PopID();
        }
        for (size_t outputIndex = 0; outputIndex < pass.outputs.size(); ++outputIndex) {
            const auto handle = pass.outputs[outputIndex];
            ImGui::PushID(static_cast<int>(pass.inputs.size() + outputIndex));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled("OUT");
            ImGui::TableNextColumn();
            const bool isValid = drawResourceLink(view, state, handle);
            ImGui::TableNextColumn();
            if (isValid) {
                drawAccessCell(view.resource(handle.id).producerAccess);
            } else {
                ImGui::TextDisabled("Invalid handle");
                ImGui::TableNextColumn();
                ImGui::TextDisabled("--");
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::TextDisabled(
        "Attachments: %zu color%s",
        pass.colorAttachments.size(),
        pass.depthStencilAttachment ? ", depth/stencil" : ""); // NOLINT
}

void drawResourceInspector(const GraphView& view, InspectorState& state, const uint32_t resourceIndex) {
    const auto& resource = view.resource(resourceIndex);
    const auto label = resourceLabel(resource);
    const auto& lifetime = view.lifetime(resourceIndex);
    ImGui::TextUnformatted(label.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("[%s]", toString(resource.type)); // NOLINT

    if (ImGui::BeginTable("##ResourceInspectorSummary", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 130.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        const auto property = [](const char* name) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", name); // NOLINT
            ImGui::TableNextColumn();
        };

        property("Producer");
        if (!drawPassLink(view, state, resource.producer)) {
            ImGui::TextDisabled("External / none");
        }
        property("Consumers");
        bool hasConsumer = false;
        for (const auto consumer : resource.readPasses) {
            if (!view.isValidPass(consumer.id)) {
                continue;
            }
            if (hasConsumer) {
                ImGui::SameLine();
            }
            drawPassLink(view, state, consumer);
            hasConsumer = true;
        }
        if (!hasConsumer) {
            ImGui::TextDisabled("None");
        }
        property("Lifetime");
        if (lifetime.isUsed) {
            ImGui::Text("Pass %u through %u", lifetime.firstPass, lifetime.lastPass); // NOLINT
        } else {
            ImGui::TextDisabled("Unused");
        }
        property("Allocation");
        if (resource.isExternal) {
            ImGui::TextUnformatted("External");
        } else {
            ImGui::Text("Physical %c%u", physicalPrefix(resource.type), resource.physicalResourceIndex); // NOLINT
        }

        const RenderGraphResourceHandle handle{resourceIndex};
        if (resource.type == ResourceType::Image) {
            const auto& description = view.graph().getImageDescription(handle);
            const auto extent = view.graph().getImageExtent(handle);
            property("Extent");
            ImGui::Text(
                "%u x %u x %u (%s)",
                extent.width,
                extent.height,
                extent.depth,
                toString(description.sizePolicy)); // NOLINT
            property("Format");
            ImGui::Text(
                "%s (VkFormat %d)", formatName(description.format), static_cast<int>(description.format)); // NOLINT
            property("Subresources");
            ImGui::Text(
                "%u layer(s), %u mip(s), %u sample(s)",
                description.layerCount,
                description.mipLevelCount,
                static_cast<uint32_t>(description.sampleCount)); // NOLINT
            property("Usage");
            ImGui::TextWrapped("%s", imageUsage(description.imageUsageFlags).c_str()); // NOLINT
            property("Clear value");
            ImGui::TextUnformatted(description.clearValue ? "Defined" : "None");
        } else if (resource.type == ResourceType::Buffer) {
            const auto& description = view.graph().getBufferDescription(handle);
            property("Size");
            ImGui::TextUnformatted(byteSize(description.size).c_str());
            property("Format hint");
            ImGui::Text(
                "%s (VkFormat %d)",
                formatName(description.formatHint),
                static_cast<int>(description.formatHint)); // NOLINT
            property("Usage");
            ImGui::TextWrapped("%s", bufferUsage(description.usageFlags).c_str()); // NOLINT
        }
        ImGui::EndTable();
    }

    if (resource.isExternal || resource.physicalResourceIndex == RenderGraphResource::kInvalidIndex) {
        return;
    }
    bool hasAlias = false;
    for (size_t index = 0; index < view.resourceCount(); ++index) {
        const auto& candidate = view.resource(index);
        if (index == resourceIndex || candidate.isExternal || candidate.type != resource.type ||
            candidate.physicalResourceIndex != resource.physicalResourceIndex) {
            continue;
        }
        if (!hasAlias) {
            ImGui::TextDisabled("Aliases");
            hasAlias = true;
        }
        ImGui::SameLine();
        drawResourceLink(view, state, {static_cast<uint32_t>(index)});
    }
}

void drawSelectionInspector(const GraphView& view, InspectorState& state) {
    if (state.selectionType == SelectionType::Pass && !view.isValidPass(state.selectedIndex)) {
        state.selectionType = SelectionType::None;
    }
    if (state.selectionType == SelectionType::Resource && !view.isValidResource(state.selectedIndex)) {
        state.selectionType = SelectionType::None;
    }
    if (state.selectionType == SelectionType::None) {
        return;
    }
    ImGui::Spacing();
    if (!ImGui::CollapsingHeader("Selection", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    if (ImGui::SmallButton("Clear")) {
        clearSelection(state);
        return;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("or press Escape, or click the highlighted row again");
    if (state.selectionType == SelectionType::Pass) {
        drawPassInspector(view, state, state.selectedIndex);
    } else {
        drawResourceInspector(view, state, state.selectedIndex);
    }
}

// Indices are only meaningful for the graph they were taken from, so a different graph clears the selection.
InspectorState& getInspectorState(const rg::RenderGraph& renderGraph) {
    static InspectorState state;
    if (state.graph != &renderGraph) {
        state.graph = &renderGraph;
        state.selectionType = SelectionType::None;
        state.selectedIndex = 0;
    }
    return state;
}

} // namespace

void drawRenderGraphGui(const rg::RenderGraph& renderGraph) {
    const GraphView view(renderGraph);
    InspectorState& state = getInspectorState(renderGraph);

    // Scoped to this window so the inspector does not swallow Escape for the rest of the application.
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        clearSelection(state);
    }

    if (ImGui::BeginTabBar("##RenderGraphInspectorTabs")) {
        if (ImGui::BeginTabItem("Overview")) {
            drawOverview(view);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Passes")) {
            drawPassTable(view, state);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Resources")) {
            drawResourceTable(view, state);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Timeline")) {
            drawTimeline(view, state);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    drawSelectionInspector(view, state);
}

} // namespace crisp
