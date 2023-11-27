#include <Crisp/Math/AliasTable.hpp>

#include <algorithm>
#include <map>

namespace crisp {
namespace {
struct AliasTableConstructionElement {
    float tau;
    uint32_t i;
    uint32_t j;
};
} // namespace

AliasTable createAliasTable(const std::vector<float>& weights) {
    float avgWeight = 0.0f;
    for (const float w : weights) {
        avgWeight += w;
    }
    avgWeight /= static_cast<float>(weights.size());

    std::multimap<float, uint32_t> sortedSamples;
    for (uint32_t i = 0; i < weights.size(); ++i) {
        sortedSamples.insert({weights[i], i});
    }

    std::vector<AliasTableConstructionElement> tuples;
    tuples.reserve(weights.size());
    for (uint32_t i = 0; i < weights.size(); ++i) {
        auto lowest = sortedSamples.begin();
        auto highest = --sortedSamples.end();
        const AliasTableConstructionElement el{
            .tau = lowest->first / avgWeight,
            .i = lowest->second,
            .j = highest->second,
        };
        tuples.push_back(el);

        if (sortedSamples.size() == 1) {
            break;
        }

        const float newWeightJ = highest->first - (avgWeight - lowest->first);

        sortedSamples.erase(lowest);
        sortedSamples.erase(highest);
        sortedSamples.insert({newWeightJ, el.j});
    }

    std::ranges::sort(tuples, [](const auto& a, const auto& b) { return a.i < b.i; });

    std::vector<AliasTableElement> table;
    table.reserve(tuples.size());
    for (const auto& el : tuples) {
        table.push_back({el.tau, el.j});
    }

    return table;
}
} // namespace crisp