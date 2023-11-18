#pragma once

#include <vector>

namespace crisp {

struct AliasTableElement {
    float tau;
    uint32_t i;
    uint32_t j;
};

struct AliasTablePackedElement {
    float tau;
    uint32_t j;
};

using AliasTable = std::vector<AliasTablePackedElement>;

AliasTable createAliasTable(const std::vector<float>& weights);
} // namespace crisp