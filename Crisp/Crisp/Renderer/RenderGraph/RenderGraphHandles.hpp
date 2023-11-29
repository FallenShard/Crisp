#pragma once

#include <cstdint>

namespace crisp {

struct RenderGraphPassHandle {
    static constexpr uint32_t kInvalidId{~0u};
    static constexpr uint32_t kExternalPass{~0u};

    uint32_t id{kInvalidId};
};

struct RenderGraphResourceHandle {
    static constexpr uint32_t kInvalidId{~0u};

    uint32_t id{kInvalidId};
};

} // namespace crisp