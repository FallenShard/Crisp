#pragma once

#include <vector>

namespace vesper
{
    struct RayTracerUpdate
    {
        int blocksRendered;
        int numBlocks;
        float totalTimeSpentRendering;
        int x;
        int y;
        int width;
        int height;
        std::vector<float> data;
    };
}