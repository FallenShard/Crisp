#include <Crisp/Renderer/RenderGraph/RenderGraphGui.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <numeric>
#include <optional>
#include <string>
#include <vector>

#include <imgui.h>

namespace crisp {
namespace {

enum class SelectionType : uint8_t { None, Pass, Resource };

struct InspectorState {
    SelectionType selectionType{SelectionType::None};
    uint32_t selectedIndex{0};
    ImGuiTextFilter passFilter;
    ImGuiTextFilter resourceFilter;
    ImGuiTextFilter timelineFilter;
};

struct ResourceLifetime {
    uint32_t firstPass{RenderGraphPassHandle::kInvalidId};
    uint32_t lastPass{RenderGraphPassHandle::kInvalidId};
};

constexpr ImGuiTableFlags kTableFlags =
    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
    ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;

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

ResourceLifetime getLifetime(const rg::RenderGraph& renderGraph, const RenderGraphResource& resource) {
    ResourceLifetime lifetime;
    if (resource.producer.id < renderGraph.getPassCount()) {
        lifetime.firstPass = resource.producer.id;
        lifetime.lastPass = resource.producer.id;
    }
    for (const auto pass : resource.readPasses) {
        if (pass.id < renderGraph.getPassCount()) {
            lifetime.firstPass = std::min(lifetime.firstPass, pass.id);
            lifetime.lastPass = std::max(lifetime.lastPass, pass.id);
        }
    }
    return lifetime;
}

std::string resourceLabel(const RenderGraphResource& resource) {
    return resource.name + " v" + std::to_string(resource.version);
}

template <typename Flags>
void appendFlag(std::string& value, const Flags flags, const Flags flag, const char* name) {
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
    appendFlag(value, flags, VkImageUsageFlags{VK_IMAGE_USAGE_TRANSFER_SRC_BIT}, "Transfer src");
    appendFlag(value, flags, VkImageUsageFlags{VK_IMAGE_USAGE_TRANSFER_DST_BIT}, "Transfer dst");
    appendFlag(value, flags, VkImageUsageFlags{VK_IMAGE_USAGE_SAMPLED_BIT}, "Sampled");
    appendFlag(value, flags, VkImageUsageFlags{VK_IMAGE_USAGE_STORAGE_BIT}, "Storage");
    appendFlag(value, flags, VkImageUsageFlags{VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT}, "Color attachment");
    appendFlag(value, flags, VkImageUsageFlags{VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT}, "Depth attachment");
    appendFlag(value, flags, VkImageUsageFlags{VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT}, "Input attachment");
    return value.empty() ? "None" : value;
}

std::string bufferUsage(const VkBufferUsageFlags flags) {
    std::string value;
    appendFlag(value, flags, VkBufferUsageFlags{VK_BUFFER_USAGE_TRANSFER_SRC_BIT}, "Transfer src");
    appendFlag(value, flags, VkBufferUsageFlags{VK_BUFFER_USAGE_TRANSFER_DST_BIT}, "Transfer dst");
    appendFlag(value, flags, VkBufferUsageFlags{VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT}, "Uniform texel");
    appendFlag(value, flags, VkBufferUsageFlags{VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT}, "Storage texel");
    appendFlag(value, flags, VkBufferUsageFlags{VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT}, "Uniform");
    appendFlag(value, flags, VkBufferUsageFlags{VK_BUFFER_USAGE_STORAGE_BUFFER_BIT}, "Storage");
    appendFlag(value, flags, VkBufferUsageFlags{VK_BUFFER_USAGE_INDEX_BUFFER_BIT}, "Index");
    appendFlag(value, flags, VkBufferUsageFlags{VK_BUFFER_USAGE_VERTEX_BUFFER_BIT}, "Vertex");
    appendFlag(value, flags, VkBufferUsageFlags{VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT}, "Indirect");
    return value.empty() ? "None" : value;
}

std::string pipelineStages(const VkPipelineStageFlags2 flags) {
    std::string value;
    appendFlag(value, flags, VkPipelineStageFlags2{VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT}, "Top");
    appendFlag(value, flags, VkPipelineStageFlags2{VK_PIPELINE_STAGE_2_TRANSFER_BIT}, "Transfer");
    appendFlag(value, flags, VkPipelineStageFlags2{VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT}, "Compute");
    appendFlag(value, flags, VkPipelineStageFlags2{VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT}, "Vertex shader");
    appendFlag(value, flags, VkPipelineStageFlags2{VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT}, "Fragment shader");
    appendFlag(value, flags, VkPipelineStageFlags2{VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT}, "Vertex input");
    appendFlag(value, flags, VkPipelineStageFlags2{VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT}, "Index input");
    appendFlag(value, flags, VkPipelineStageFlags2{VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT}, "Color output");
    appendFlag(value, flags, VkPipelineStageFlags2{VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT}, "Early tests");
    appendFlag(value, flags, VkPipelineStageFlags2{VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT}, "Late tests");
    appendFlag(value, flags, VkPipelineStageFlags2{VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR}, "Ray tracing");
    appendFlag(
        value,
        flags,
        VkPipelineStageFlags2{VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR},
        "Acceleration structure");
    appendFlag(value, flags, VkPipelineStageFlags2{VK_PIPELINE_STAGE_2_HOST_BIT}, "Host");
    appendFlag(value, flags, VkPipelineStageFlags2{VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT}, "Bottom");
    appendFlag(value, flags, VkPipelineStageFlags2{VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT}, "All commands");
    return value.empty() ? "None" : value;
}

std::string accessFlags(const VkAccessFlags2 flags) {
    std::string value;
    appendFlag(value, flags, VkAccessFlags2{VK_ACCESS_2_TRANSFER_READ_BIT}, "Transfer read");
    appendFlag(value, flags, VkAccessFlags2{VK_ACCESS_2_TRANSFER_WRITE_BIT}, "Transfer write");
    appendFlag(value, flags, VkAccessFlags2{VK_ACCESS_2_SHADER_READ_BIT}, "Shader read");
    appendFlag(value, flags, VkAccessFlags2{VK_ACCESS_2_SHADER_WRITE_BIT}, "Shader write");
    appendFlag(value, flags, VkAccessFlags2{VK_ACCESS_2_SHADER_SAMPLED_READ_BIT}, "Sampled read");
    appendFlag(value, flags, VkAccessFlags2{VK_ACCESS_2_SHADER_STORAGE_READ_BIT}, "Storage read");
    appendFlag(value, flags, VkAccessFlags2{VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT}, "Storage write");
    appendFlag(value, flags, VkAccessFlags2{VK_ACCESS_2_UNIFORM_READ_BIT}, "Uniform read");
    appendFlag(value, flags, VkAccessFlags2{VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT}, "Input attachment read");
    appendFlag(value, flags, VkAccessFlags2{VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT}, "Vertex read");
    appendFlag(value, flags, VkAccessFlags2{VK_ACCESS_2_INDEX_READ_BIT}, "Index read");
    appendFlag(value, flags, VkAccessFlags2{VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT}, "Color write");
    appendFlag(value, flags, VkAccessFlags2{VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT}, "Depth write");
    appendFlag(value, flags, VkAccessFlags2{VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR}, "AS write");
    appendFlag(value, flags, VkAccessFlags2{VK_ACCESS_2_HOST_READ_BIT}, "Host read");
    appendFlag(value, flags, VkAccessFlags2{VK_ACCESS_2_HOST_WRITE_BIT}, "Host write");
    appendFlag(value, flags, VkAccessFlags2{VK_ACCESS_2_MEMORY_READ_BIT}, "Memory read");
    appendFlag(value, flags, VkAccessFlags2{VK_ACCESS_2_MEMORY_WRITE_BIT}, "Memory write");
    return value.empty() ? "None" : value;
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

void selectPass(InspectorState& state, const uint32_t passIndex) {
    state.selectionType = SelectionType::Pass;
    state.selectedIndex = passIndex;
}

void selectResource(InspectorState& state, const uint32_t resourceIndex) {
    state.selectionType = SelectionType::Resource;
    state.selectedIndex = resourceIndex;
}

bool drawResourceLink(
    const rg::RenderGraph& renderGraph, InspectorState& state, const RenderGraphResourceHandle handle) {
    if (handle.id >= renderGraph.getResourceCount()) {
        return false;
    }
    const auto label = resourceLabel(renderGraph.getResources()[handle.id]);
    ImGui::PushID(static_cast<int>(handle.id));
    const bool clicked = ImGui::SmallButton(label.c_str());
    ImGui::PopID();
    if (clicked) {
        selectResource(state, handle.id);
    }
    return clicked;
}

bool drawPassLink(const rg::RenderGraph& renderGraph, InspectorState& state, const RenderGraphPassHandle handle) {
    if (handle.id >= renderGraph.getPassCount()) {
        return false;
    }
    ImGui::PushID(static_cast<int>(handle.id));
    const bool clicked = ImGui::SmallButton(renderGraph.getPasses()[handle.id].name.c_str());
    ImGui::PopID();
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

void drawOverview(const rg::RenderGraph& renderGraph) {
    size_t imageCount = 0;
    size_t bufferCount = 0;
    size_t externalCount = 0;
    for (const auto& resource : renderGraph.getResources()) {
        imageCount += resource.type == ResourceType::Image ? 1 : 0;
        bufferCount += resource.type == ResourceType::Buffer ? 1 : 0;
        externalCount += resource.isExternal ? 1 : 0;
    }
    const auto physicalCount = renderGraph.getPhysicalImageCount() + renderGraph.getPhysicalBufferCount();
    const auto internalCount = renderGraph.getResourceCount() - externalCount;
    const auto savedAllocations = internalCount > physicalCount ? internalCount - physicalCount : 0;
    const auto timings = renderGraph.getGpuPassTimingsMs();
    std::optional<size_t> slowestPass;
    for (size_t i = 0; i < timings.size(); ++i) {
        if (timings[i] && (!slowestPass || *timings[i] > *timings[*slowestPass])) {
            slowestPass = i;
        }
    }

    if (ImGui::BeginTable("##RenderGraphOverview", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame)) {
        drawMetric("PASSES", std::to_string(renderGraph.getPassCount()));
        drawMetric("LOGICAL RESOURCES", std::to_string(renderGraph.getResourceCount()));
        drawMetric("IMAGES / BUFFERS", std::to_string(imageCount) + " / " + std::to_string(bufferCount));
        drawMetric("EXTERNAL", std::to_string(externalCount));
        drawMetric("PHYSICAL ALLOCATIONS", std::to_string(physicalCount));
        drawMetric("ALIASED ALLOCATIONS SAVED", std::to_string(savedAllocations));
        if (const auto graphTime = renderGraph.getGpuFrameTimingMs()) {
            char text[32];
            std::snprintf(text, std::size(text), "%.3f ms", *graphTime);
            drawMetric("GPU GRAPH", text);
        } else {
            drawMetric("GPU GRAPH", renderGraph.isGpuProfilingSupported() ? "Pending" : "Unsupported");
        }
        if (slowestPass) {
            char text[128];
            std::snprintf(
                text,
                std::size(text),
                "%s (%.3f ms)",
                renderGraph.getPasses()[*slowestPass].name.c_str(),
                *timings[*slowestPass]);
            drawMetric("SLOWEST PASS", text);
        } else {
            drawMetric("SLOWEST PASS", "Pending");
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    std::array<size_t, 3> typeCounts{};
    for (const auto& pass : renderGraph.getPasses()) {
        ++typeCounts[static_cast<size_t>(pass.type)];
    }
    for (const auto type : {PassType::Rasterizer, PassType::Compute, PassType::RayTracing}) {
        ImGui::TextColored(passColor(type), "%s: %zu", toString(type), typeCounts[static_cast<size_t>(type)]); // NOLINT
        if (type != PassType::RayTracing) {
            ImGui::SameLine(0.0f, 24.0f);
        }
    }
    ImGui::Spacing();
    ImGui::TextWrapped(
        "GPU timings are read asynchronously when a virtual frame is reused. Static graph metadata remains available "
        "while the first sample is pending.");
}

void sortPassIndices(
    std::vector<size_t>& indices,
    const rg::RenderGraph& renderGraph,
    const std::span<const std::optional<double>> timings,
    const ImGuiTableSortSpecs* specs) {
    if (!specs || specs->SpecsCount == 0) {
        return;
    }
    const auto& sort = specs->Specs[0];
    const auto compare = [&](const size_t lhsIndex, const size_t rhsIndex) {
        const auto& lhs = renderGraph.getPasses()[lhsIndex];
        const auto& rhs = renderGraph.getPasses()[rhsIndex];
        int result = 0;
        switch (sort.ColumnUserID) {
        case 0:
            result = lhsIndex < rhsIndex ? -1 : lhsIndex > rhsIndex ? 1 : 0;
            break;
        case 1:
            result = lhs.name.compare(rhs.name);
            break;
        case 2:
            result = static_cast<int>(lhs.type) - static_cast<int>(rhs.type);
            break;
        case 3: {
            const double lhsTime = lhsIndex < timings.size() ? timings[lhsIndex].value_or(-1.0) : -1.0;
            const double rhsTime = rhsIndex < timings.size() ? timings[rhsIndex].value_or(-1.0) : -1.0;
            result = lhsTime < rhsTime ? -1 : lhsTime > rhsTime ? 1 : 0;
            break;
        }
        case 4:
            result = lhs.inputs.size() < rhs.inputs.size() ? -1 : lhs.inputs.size() > rhs.inputs.size() ? 1 : 0;
            break;
        case 5:
            result = lhs.outputs.size() < rhs.outputs.size() ? -1 : lhs.outputs.size() > rhs.outputs.size() ? 1 : 0;
            break;
        default:
            break;
        }
        if (result == 0) {
            result = lhsIndex < rhsIndex ? -1 : lhsIndex > rhsIndex ? 1 : 0;
        }
        return sort.SortDirection == ImGuiSortDirection_Ascending ? result < 0 : result > 0;
    };
    std::sort(indices.begin(), indices.end(), compare);
}

void drawPassTable(const rg::RenderGraph& renderGraph, InspectorState& state) {
    state.passFilter.Draw("Filter passes", 240.0f);
    ImGui::SameLine();
    ImGui::TextDisabled("Click a row for details");
    const auto timings = renderGraph.getGpuPassTimingsMs();
    const auto graphTime = renderGraph.getGpuFrameTimingMs();
    std::vector<size_t> indices(renderGraph.getPassCount());
    std::iota(indices.begin(), indices.end(), 0);

    if (!ImGui::BeginTable("##RenderGraphPasses", 7, kTableFlags, ImVec2(0.0f, 340.0f))) {
        return;
    }
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, 32.0f, 0);
    ImGui::TableSetupColumn("Pass", ImGuiTableColumnFlags_WidthStretch, 2.0f, 1);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 88.0f, 2);
    ImGui::TableSetupColumn("GPU", ImGuiTableColumnFlags_WidthFixed, 76.0f, 3);
    ImGui::TableSetupColumn("Graph %", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed, 110.0f, 6);
    ImGui::TableSetupColumn("Inputs", ImGuiTableColumnFlags_WidthFixed, 54.0f, 4);
    ImGui::TableSetupColumn("Outputs", ImGuiTableColumnFlags_WidthFixed, 60.0f, 5);
    ImGui::TableHeadersRow();
    sortPassIndices(indices, renderGraph, timings, ImGui::TableGetSortSpecs());

    for (const size_t passIndex : indices) {
        const auto& pass = renderGraph.getPasses()[passIndex];
        if (!state.passFilter.PassFilter(pass.name.c_str())) {
            continue;
        }
        ImGui::PushID(static_cast<int>(passIndex));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%zu", passIndex); // NOLINT
        ImGui::TableNextColumn();
        const bool selected = state.selectionType == SelectionType::Pass && state.selectedIndex == passIndex;
        if (ImGui::Selectable(pass.name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
            selectPass(state, static_cast<uint32_t>(passIndex));
        }
        ImGui::TableNextColumn();
        ImGui::TextColored(passColor(pass.type), "%s", toString(pass.type)); // NOLINT
        ImGui::TableNextColumn();
        const auto timing = passIndex < timings.size() ? timings[passIndex] : std::nullopt;
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
            ImGui::ProgressBar(std::clamp(share, 0.0f, 1.0f), ImVec2(-1.0f, 0.0f), label);
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
}

void sortResourceIndices(
    std::vector<size_t>& indices, const rg::RenderGraph& renderGraph, const ImGuiTableSortSpecs* specs) {
    if (!specs || specs->SpecsCount == 0) {
        return;
    }
    const auto& sort = specs->Specs[0];
    const auto compare = [&](const size_t lhsIndex, const size_t rhsIndex) {
        const auto& lhs = renderGraph.getResources()[lhsIndex];
        const auto& rhs = renderGraph.getResources()[rhsIndex];
        const auto lhsLifetime = getLifetime(renderGraph, lhs);
        const auto rhsLifetime = getLifetime(renderGraph, rhs);
        int result = 0;
        switch (sort.ColumnUserID) {
        case 0:
            result = lhsIndex < rhsIndex ? -1 : lhsIndex > rhsIndex ? 1 : 0;
            break;
        case 1:
            result = lhs.name.compare(rhs.name);
            break;
        case 2:
            result = lhs.version < rhs.version ? -1 : lhs.version > rhs.version ? 1 : 0;
            break;
        case 3:
            result = static_cast<int>(lhs.type) - static_cast<int>(rhs.type);
            break;
        case 4:
            result = lhs.producer.id < rhs.producer.id ? -1 : lhs.producer.id > rhs.producer.id ? 1 : 0;
            break;
        case 5:
            result =
                lhsLifetime.firstPass < rhsLifetime.firstPass ? -1
                : lhsLifetime.firstPass > rhsLifetime.firstPass
                    ? 1
                    : 0;
            break;
        case 6:
            result =
                lhs.physicalResourceIndex < rhs.physicalResourceIndex ? -1
                : lhs.physicalResourceIndex > rhs.physicalResourceIndex
                    ? 1
                    : 0;
            break;
        default:
            break;
        }
        if (result == 0) {
            result = lhsIndex < rhsIndex ? -1 : lhsIndex > rhsIndex ? 1 : 0;
        }
        return sort.SortDirection == ImGuiSortDirection_Ascending ? result < 0 : result > 0;
    };
    std::sort(indices.begin(), indices.end(), compare);
}

void drawResourceTable(const rg::RenderGraph& renderGraph, InspectorState& state) {
    state.resourceFilter.Draw("Filter resources", 240.0f);
    ImGui::SameLine();
    ImGui::TextDisabled("Versions sharing a physical ID are aliased");
    std::vector<size_t> indices(renderGraph.getResourceCount());
    std::iota(indices.begin(), indices.end(), 0);

    if (!ImGui::BeginTable("##RenderGraphResources", 8, kTableFlags, ImVec2(0.0f, 340.0f))) {
        return;
    }
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, 32.0f, 0);
    ImGui::TableSetupColumn("Resource", ImGuiTableColumnFlags_WidthStretch, 2.0f, 1);
    ImGui::TableSetupColumn("v", ImGuiTableColumnFlags_WidthFixed, 28.0f, 2);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 58.0f, 3);
    ImGui::TableSetupColumn("Producer", ImGuiTableColumnFlags_WidthStretch, 1.2f, 4);
    ImGui::TableSetupColumn("Lifetime", ImGuiTableColumnFlags_WidthFixed, 72.0f, 5);
    ImGui::TableSetupColumn("Physical", ImGuiTableColumnFlags_WidthFixed, 66.0f, 6);
    ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthStretch, 1.7f, 7);
    ImGui::TableHeadersRow();
    sortResourceIndices(indices, renderGraph, ImGui::TableGetSortSpecs());

    for (const size_t resourceIndex : indices) {
        const auto& resource = renderGraph.getResources()[resourceIndex];
        const auto label = resourceLabel(resource);
        if (!state.resourceFilter.PassFilter(label.c_str())) {
            continue;
        }
        const auto lifetime = getLifetime(renderGraph, resource);
        ImGui::PushID(static_cast<int>(resourceIndex));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%zu", resourceIndex); // NOLINT
        ImGui::TableNextColumn();
        const bool selected = state.selectionType == SelectionType::Resource && state.selectedIndex == resourceIndex;
        if (ImGui::Selectable(resource.name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
            selectResource(state, static_cast<uint32_t>(resourceIndex));
        }
        ImGui::TableNextColumn();
        ImGui::Text("%u", resource.version); // NOLINT
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(toString(resource.type));
        ImGui::TableNextColumn();
        if (resource.producer.id < renderGraph.getPassCount()) {
            ImGui::TextUnformatted(renderGraph.getPasses()[resource.producer.id].name.c_str());
        } else {
            ImGui::TextDisabled("External");
        }
        ImGui::TableNextColumn();
        if (lifetime.firstPass < renderGraph.getPassCount()) {
            ImGui::Text("%u - %u", lifetime.firstPass, lifetime.lastPass); // NOLINT
        } else {
            ImGui::TextDisabled("--");
        }
        ImGui::TableNextColumn();
        if (resource.isExternal) {
            ImGui::TextDisabled("External");
        } else {
            ImGui::Text(
                "%c%u", resource.type == ResourceType::Image ? 'I' : 'B', resource.physicalResourceIndex); // NOLINT
        }
        ImGui::TableNextColumn();
        if (resource.type == ResourceType::Image) {
            const auto extent = renderGraph.getImageExtent({static_cast<uint32_t>(resourceIndex)});
            const auto& description = renderGraph.getImageDescription({static_cast<uint32_t>(resourceIndex)});
            ImGui::Text(
                "%ux%ux%u, %s", extent.width, extent.height, extent.depth, formatName(description.format)); // NOLINT
        } else if (resource.type == ResourceType::Buffer) {
            const auto& description = renderGraph.getBufferDescription({static_cast<uint32_t>(resourceIndex)});
            ImGui::TextUnformatted(byteSize(description.size).c_str());
        }
        ImGui::PopID();
    }
    ImGui::EndTable();
}

ImU32 physicalResourceColor(const RenderGraphResource& resource) {
    if (resource.isExternal) {
        return ImGui::GetColorU32(ImGuiCol_TextDisabled);
    }
    const auto typeOffset = static_cast<uint32_t>(resource.type) * 17u;
    const float hue = static_cast<float>((resource.physicalResourceIndex * 47u + typeOffset) % 101u) / 101.0f;
    return ImColor::HSV(hue, 0.55f, 0.82f);
}

void drawTimeline(const rg::RenderGraph& renderGraph, InspectorState& state) {
    state.timelineFilter.Draw("Filter resources", 240.0f);
    ImGui::SameLine();
    ImGui::TextDisabled("Bars show first-to-last pass use; colors identify physical allocations");
    if (renderGraph.getPassCount() == 0) {
        ImGui::TextDisabled("No passes");
        return;
    }

    constexpr float kLabelWidth = 190.0f;
    constexpr float kMinPassWidth = 34.0f;
    const float rowHeight = ImGui::GetTextLineHeightWithSpacing() + 4.0f;
    const float availableLaneWidth = std::max(100.0f, ImGui::GetContentRegionAvail().x - kLabelWidth);
    const float laneWidth = std::max(availableLaneWidth, kMinPassWidth * static_cast<float>(renderGraph.getPassCount()));
    const float totalWidth = kLabelWidth + laneWidth;
    const float cellWidth = laneWidth / static_cast<float>(renderGraph.getPassCount());
    const auto timings = renderGraph.getGpuPassTimingsMs();

    ImGui::BeginChild("##RenderGraphTimeline", ImVec2(0.0f, 370.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
    auto* drawList = ImGui::GetWindowDrawList();
    ImGui::InvisibleButton("##TimelineHeader", ImVec2(totalWidth, rowHeight * 1.6f));
    const ImVec2 headerMin = ImGui::GetItemRectMin();
    const ImVec2 headerMax = ImGui::GetItemRectMax();
    drawList->AddRectFilled(headerMin, headerMax, ImGui::GetColorU32(ImGuiCol_TableHeaderBg));
    drawList->AddText({headerMin.x + 5.0f, headerMin.y + 4.0f}, ImGui::GetColorU32(ImGuiCol_Text), "Execution order");
    for (size_t passIndex = 0; passIndex < renderGraph.getPassCount(); ++passIndex) {
        const auto& pass = renderGraph.getPasses()[passIndex];
        const ImVec2 min{headerMin.x + kLabelWidth + cellWidth * static_cast<float>(passIndex), headerMin.y};
        const ImVec2 max{min.x + cellWidth, headerMax.y};
        const bool selected = state.selectionType == SelectionType::Pass && state.selectedIndex == passIndex;
        drawList->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(passColor(pass.type)), 2.0f);
        drawList->AddRect(
            min, max, selected ? IM_COL32_WHITE : IM_COL32(20, 20, 20, 180), 2.0f, 0, selected ? 2.0f : 1.0f);
        const std::string indexLabel = std::to_string(passIndex);
        drawList->AddText({min.x + 4.0f, min.y + 4.0f}, IM_COL32(15, 15, 15, 255), indexLabel.c_str());
        if (ImGui::IsMouseHoveringRect(min, max)) {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                selectPass(state, static_cast<uint32_t>(passIndex));
            }
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(pass.name.c_str());
            ImGui::TextDisabled("%s", toString(pass.type)); // NOLINT
            if (passIndex < timings.size() && timings[passIndex]) {
                ImGui::Text("%.3f ms", *timings[passIndex]); // NOLINT
            }
            ImGui::EndTooltip();
        }
    }

    for (size_t resourceIndex = 0; resourceIndex < renderGraph.getResourceCount(); ++resourceIndex) {
        const auto& resource = renderGraph.getResources()[resourceIndex];
        const auto label = resourceLabel(resource);
        if (!state.timelineFilter.PassFilter(label.c_str())) {
            continue;
        }
        const auto lifetime = getLifetime(renderGraph, resource);
        ImGui::PushID(static_cast<int>(resourceIndex));
        ImGui::InvisibleButton("##TimelineResource", ImVec2(totalWidth, rowHeight));
        const ImVec2 rowMin = ImGui::GetItemRectMin();
        const ImVec2 rowMax = ImGui::GetItemRectMax();
        const bool selected = state.selectionType == SelectionType::Resource && state.selectedIndex == resourceIndex;
        if (selected) {
            drawList->AddRectFilled(rowMin, rowMax, ImGui::GetColorU32(ImGuiCol_Header));
        } else if ((resourceIndex & 1u) != 0) {
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
        for (size_t passIndex = 0; passIndex <= renderGraph.getPassCount(); ++passIndex) {
            const float x = rowMin.x + kLabelWidth + cellWidth * static_cast<float>(passIndex);
            drawList->AddLine({x, rowMin.y}, {x, rowMax.y}, ImGui::GetColorU32(ImGuiCol_Border));
        }
        if (lifetime.firstPass < renderGraph.getPassCount()) {
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
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(label.c_str());
            if (resource.isExternal) {
                ImGui::Text("%s, external", toString(resource.type)); // NOLINT
            } else {
                ImGui::Text(
                    "%s, physical %c%u",
                    toString(resource.type),
                    resource.type == ResourceType::Image ? 'I' : 'B',
                    resource.physicalResourceIndex); // NOLINT
            }
            if (lifetime.firstPass < renderGraph.getPassCount()) {
                ImGui::Text("Passes %u - %u", lifetime.firstPass, lifetime.lastPass); // NOLINT
            }
            ImGui::EndTooltip();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
}

void drawPassInspector(const rg::RenderGraph& renderGraph, InspectorState& state, const uint32_t passIndex) {
    const auto& pass = renderGraph.getPasses()[passIndex];
    const auto timings = renderGraph.getGpuPassTimingsMs();
    ImGui::TextUnformatted(pass.name.c_str());
    ImGui::SameLine();
    ImGui::TextColored(passColor(pass.type), "[%s]", toString(pass.type)); // NOLINT
    if (passIndex < timings.size() && timings[passIndex]) {
        ImGui::SameLine();
        ImGui::Text("%.3f ms", *timings[passIndex]); // NOLINT
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
            drawResourceLink(renderGraph, state, pass.inputs[inputIndex]);
            ImGui::TableNextColumn();
            if (inputIndex < pass.inputAccesses.size()) {
                const auto& access = pass.inputAccesses[inputIndex];
                ImGui::TextUnformatted(toString(access.usageType));
                ImGui::TableNextColumn();
                const auto stages = pipelineStages(access.stage.stage);
                const auto accesses = accessFlags(access.stage.access);
                ImGui::TextWrapped("%s / %s", stages.c_str(), accesses.c_str()); // NOLINT
            } else {
                ImGui::TextDisabled("--");
                ImGui::TableNextColumn();
                ImGui::TextDisabled("--");
            }
            ImGui::PopID();
        }
        for (size_t outputIndex = 0; outputIndex < pass.outputs.size(); ++outputIndex) {
            const auto handle = pass.outputs[outputIndex];
            const auto& resource = renderGraph.getResources()[handle.id];
            ImGui::PushID(static_cast<int>(pass.inputs.size() + outputIndex));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled("OUT");
            ImGui::TableNextColumn();
            drawResourceLink(renderGraph, state, handle);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(toString(resource.producerAccess.usageType));
            ImGui::TableNextColumn();
            const auto stages = pipelineStages(resource.producerAccess.stage.stage);
            const auto accesses = accessFlags(resource.producerAccess.stage.access);
            ImGui::TextWrapped("%s / %s", stages.c_str(), accesses.c_str()); // NOLINT
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::TextDisabled(
        "Attachments: %zu color%s",
        pass.colorAttachments.size(),
        pass.depthStencilAttachment ? ", depth/stencil" : ""); // NOLINT
}

void drawResourceInspector(const rg::RenderGraph& renderGraph, InspectorState& state, const uint32_t resourceIndex) {
    const auto& resource = renderGraph.getResources()[resourceIndex];
    const auto label = resourceLabel(resource);
    const auto lifetime = getLifetime(renderGraph, resource);
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
        if (!drawPassLink(renderGraph, state, resource.producer)) {
            ImGui::TextDisabled("External / none");
        }
        property("Consumers");
        bool hasConsumer = false;
        for (const auto consumer : resource.readPasses) {
            if (consumer.id >= renderGraph.getPassCount()) {
                continue;
            }
            if (hasConsumer) {
                ImGui::SameLine();
            }
            drawPassLink(renderGraph, state, consumer);
            hasConsumer = true;
        }
        if (!hasConsumer) {
            ImGui::TextDisabled("None");
        }
        property("Lifetime");
        if (lifetime.firstPass < renderGraph.getPassCount()) {
            ImGui::Text("Pass %u through %u", lifetime.firstPass, lifetime.lastPass); // NOLINT
        } else {
            ImGui::TextDisabled("Unused");
        }
        property("Allocation");
        if (resource.isExternal) {
            ImGui::TextUnformatted("External");
        } else {
            ImGui::Text(
                "Physical %c%u",
                resource.type == ResourceType::Image ? 'I' : 'B',
                resource.physicalResourceIndex); // NOLINT
        }

        const auto handle = RenderGraphResourceHandle{resourceIndex};
        if (resource.type == ResourceType::Image) {
            const auto& description = renderGraph.getImageDescription(handle);
            const auto extent = renderGraph.getImageExtent(handle);
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
            const auto& description = renderGraph.getBufferDescription(handle);
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

    if (!resource.isExternal) {
        bool hasAlias = false;
        for (size_t index = 0; index < renderGraph.getResourceCount(); ++index) {
            const auto& candidate = renderGraph.getResources()[index];
            if (index == resourceIndex || candidate.isExternal || candidate.type != resource.type ||
                candidate.physicalResourceIndex != resource.physicalResourceIndex) {
                continue;
            }
            if (!hasAlias) {
                ImGui::TextDisabled("Aliases");
            } else {
                ImGui::SameLine();
            }
            drawResourceLink(renderGraph, state, {static_cast<uint32_t>(index)});
            hasAlias = true;
        }
    }
}

void drawSelectionInspector(const rg::RenderGraph& renderGraph, InspectorState& state) {
    if (state.selectionType == SelectionType::Pass && state.selectedIndex >= renderGraph.getPassCount()) {
        state.selectionType = SelectionType::None;
    }
    if (state.selectionType == SelectionType::Resource && state.selectedIndex >= renderGraph.getResourceCount()) {
        state.selectionType = SelectionType::None;
    }
    if (state.selectionType == SelectionType::None) {
        return;
    }
    ImGui::Spacing();
    if (!ImGui::CollapsingHeader("Selection", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    if (state.selectionType == SelectionType::Pass) {
        drawPassInspector(renderGraph, state, state.selectedIndex);
    } else {
        drawResourceInspector(renderGraph, state, state.selectedIndex);
    }
}

} // namespace

void drawGui(const rg::RenderGraph& renderGraph) {
    static InspectorState state;
    if (ImGui::BeginTabBar("##RenderGraphInspectorTabs")) {
        if (ImGui::BeginTabItem("Overview")) {
            drawOverview(renderGraph);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Passes")) {
            drawPassTable(renderGraph, state);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Resources")) {
            drawResourceTable(renderGraph, state);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Timeline")) {
            drawTimeline(renderGraph, state);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    drawSelectionInspector(renderGraph, state);
}

} // namespace crisp
