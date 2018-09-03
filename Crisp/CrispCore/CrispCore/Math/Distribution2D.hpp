#pragma once

#include "Distribution1D.hpp"

namespace crisp
{
    class Distribution2D
    {
    public:
        Distribution2D();
        Distribution2D(size_t numCols = 0, size_t numRows = 0);
        Distribution2D(const std::vector<std::vector<float>>& func);
        ~Distribution2D();

    private:
        std::vector<Distribution1D> m_phiPdfs;
        Distribution1D m_thetaPdf;
    };
}