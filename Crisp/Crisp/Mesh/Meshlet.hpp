#pragma once

#include <cstdint>

namespace crisp {

struct Meshlet {
    uint32_t vertexOffset{0};
    uint32_t triangleOffset{0};

    uint32_t vertexCount{0};
    uint32_t triangleCount{0};
};

} // namespace crisp
