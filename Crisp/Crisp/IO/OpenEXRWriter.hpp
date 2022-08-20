#pragma once

#include <string>

namespace crisp
{
    class OpenEXRWriter
    {
    public:
        OpenEXRWriter();
        ~OpenEXRWriter();

        void write(const std::string& fileName, const float* data, unsigned int width, unsigned int height, bool flipYAxis = false);
    };
}
