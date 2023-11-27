#pragma once

#include <vector>

namespace crisp {

struct AliasTableElement {
    float tau;
    uint32_t j;
};

using AliasTable = std::vector<AliasTableElement>;

AliasTable createAliasTable(const std::vector<float>& weights);
} // namespace crisp