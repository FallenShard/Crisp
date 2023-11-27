#pragma once

#include <cstdint>
#include <string>

namespace crisp {
struct TriangleMeshView {
    std::string tag;
    uint32_t firstIndex;
    uint32_t indexCount;

    inline TriangleMeshView()
        : firstIndex(0)
        , indexCount(0) {}

    inline TriangleMeshView(std::string&& tag, const uint32_t firstIndex, const uint32_t indexCount)
        : tag(std::move(tag))
        , firstIndex(firstIndex)
        , indexCount(indexCount) {}
};
} // namespace crisp
