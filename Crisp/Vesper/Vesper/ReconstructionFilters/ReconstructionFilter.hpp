#pragma once

#include <array>

namespace crisp
{
    class ReconstructionFilter
    {
    public:
        ReconstructionFilter();
        virtual ~ReconstructionFilter();

        inline float evalDiscrete(float x, float y) const
        {
            return m_lookupTable[static_cast<int>(std::abs(y) * m_lookupFactor)][static_cast<int>(std::abs(x) * m_lookupFactor)];
        }

        float getRadius() const;

        virtual float eval(float x, float y) const = 0;

    protected:
        void initLookupTable();

        static const int FilterResolution = 32;

        float m_radius;
        std::array<std::array<float, FilterResolution + 1>, FilterResolution + 1> m_lookupTable;
        float m_lookupFactor;
    };
}