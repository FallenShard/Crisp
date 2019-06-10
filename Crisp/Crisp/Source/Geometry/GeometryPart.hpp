#pragma once

#include <cstdint>
#include <string>

namespace crisp
{
    struct GeometryPart
    {
        std::string tag;
        uint32_t first;
        uint32_t count;

        inline GeometryPart() : first(0), count(0) {}
        inline GeometryPart(const std::string& t, uint32_t f, uint32_t c) : tag(t), first(f), count(c) {}
    };
}
