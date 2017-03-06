#pragma once

#include <vector>

namespace vesper
{
    struct RayTracerUpdate
    {
        int pixelsRendered;
        int numPixels;
        float totalTimeSpentRendering;
        int x;
        int y;
        int width;
        int height;
        std::vector<float> data;
    };
}