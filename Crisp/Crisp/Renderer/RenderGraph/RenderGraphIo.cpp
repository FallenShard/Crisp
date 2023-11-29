#include <Crisp/Renderer/RenderGraph/RenderGraphIo.hpp>

#include <fstream>
#include <ranges>

namespace crisp {
namespace {
class GraphVizWriter {
public:
    explicit GraphVizWriter(std::ofstream&& file)
        : m_file(std::move(file)) {
        m_file << "digraph RenderGraph {\n";
    }

    ~GraphVizWriter() {
        m_file << "}\n";
    }

    GraphVizWriter(const GraphVizWriter&) = delete;
    GraphVizWriter& operator=(const GraphVizWriter&) = delete;

    GraphVizWriter(GraphVizWriter&&) noexcept = delete;
    GraphVizWriter& operator=(GraphVizWriter&&) noexcept = delete;

    void addNode(
        const size_t idx, const std::string_view label, const std::string_view shape, const std::string_view color) {
        m_file << fmt::format(R"({} [label="{}", style="filled", shape="{}", color="{}"];)", idx, label, shape, color);
        m_file << '\n';
    }

    void addEdge(const size_t src, const size_t dst) {
        m_file << "    " << src << " -> " << dst << ";";
        m_file << '\n';
    }

private:
    std::ofstream m_file;
};
} // namespace

Result<> toGraphViz(const rg::RenderGraph& renderGraph, const std::filesystem::path& outputPath) {
    std::ofstream outputFile(outputPath);
    if (!outputFile) {
        return resultError("Failed to open output file: {}!", outputPath.string());
    }

    GraphVizWriter graphViz(std::move(outputFile));

    for (auto&& [idx, res] : std::views::enumerate(renderGraph.getResources())) {
        const char* color = res.type == ResourceType::Image ? "springgreen" : "peachpuff";
        graphViz.addNode(
            idx,
            fmt::format(
                "{} R: {}, PROD: {}, V: {}",
                res.name,
                res.readPasses.size(),
                renderGraph.getPass(res.producer).name,
                res.version),
            "ellipse",
            color);
    }

    for (auto&& [i, node] : std::views::enumerate(renderGraph.getPasses())) {
        const auto vertexIdx = i + renderGraph.getResources().size();
        graphViz.addNode(vertexIdx, fmt::format("{}", node.name), "box", "deepskyblue");

        for (const auto& neighbor : node.inputs) {
            graphViz.addEdge(neighbor.id, vertexIdx);
        }

        for (const auto& neighbor : node.outputs) {
            graphViz.addEdge(vertexIdx, neighbor.id);
        }
    }

    spdlog::trace("Graph visualization saved to: {}", outputPath.string());

    return kResultSuccess;
}

} // namespace crisp