#pragma once

#include <vector>

namespace vesper
{
    struct ImageBlockEventArgs
    {
        int x;
        int y;
        int width;
        int height;
        std::vector<float> data;
    };
}