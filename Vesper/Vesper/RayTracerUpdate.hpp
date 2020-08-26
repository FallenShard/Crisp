#pragma once

#include <vector>

namespace crisp
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