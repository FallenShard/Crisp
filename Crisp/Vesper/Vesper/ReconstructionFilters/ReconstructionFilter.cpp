#include "ReconstructionFilter.hpp"

namespace vesper
{
    ReconstructionFilter::ReconstructionFilter()
    {
    }

    ReconstructionFilter::~ReconstructionFilter()
    {
    }

    float ReconstructionFilter::getRadius() const
    {
        return m_radius;
    }

    void ReconstructionFilter::initLookupTable()
    {
        for (int i = 0; i < FilterResolution; i++)
        {
            for (int j = 0; j < FilterResolution; j++)
            {
                float posX = m_radius * j / FilterResolution;
                float posY = m_radius * i / FilterResolution;
                m_lookupTable[i][j] = eval(posX, posY);
            }
        }
        for (int i = 0; i < FilterResolution + 1; i++)
            m_lookupTable[i][FilterResolution] = m_lookupTable[FilterResolution][i] = 0.0f;

        m_lookupFactor = FilterResolution / m_radius;
    }
}