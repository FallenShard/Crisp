#pragma once

#include <vector>

#include "VertexAttributeTraits.hpp"

namespace crisp
{
    struct VertexAttributeLayout
    {
        std::string name;
        VertexAttribute type;
    };

    struct VertexStream
    {
        uint32_t binding;
        uint32_t stride;
        std::vector<VertexAttribute> attributes;
    };

    struct VertexLayout
    {
        VertexLayout() {}
        VertexLayout(int numStreams) : streams(numStreams) {}

        std::vector<VertexStream> streams;

    };
}