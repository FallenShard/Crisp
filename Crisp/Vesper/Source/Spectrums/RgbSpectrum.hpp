#pragma once

#include <algorithm>
#include <glm/vec3.hpp>

namespace vesper
{
    struct RgbSpectrum
    {
        union
        {
            float values[3];
            struct
            {
                float r, g, b;
            };
        };

        RgbSpectrum(float value = 0.0f);
        RgbSpectrum(float red, float green, float blue);
        RgbSpectrum(const glm::vec3& vec);

        float& operator[](int index);
        RgbSpectrum& operator=(const RgbSpectrum& rgbSpectrum);
        RgbSpectrum& operator+=(const RgbSpectrum& rgbSpectrum);
        RgbSpectrum& operator-=(const RgbSpectrum& rgbSpectrum);
        RgbSpectrum& operator*=(const RgbSpectrum& rgbSpectrum);
        RgbSpectrum& operator/=(const RgbSpectrum& rgbSpectrum);
        const RgbSpectrum operator+(const RgbSpectrum& rgbSpectrum) const;
        const RgbSpectrum operator-(const RgbSpectrum& rgbSpectrum) const;
        const RgbSpectrum operator*(const RgbSpectrum& rgbSpectrum) const;
        const RgbSpectrum operator/(const RgbSpectrum& rgbSpectrum) const;

        RgbSpectrum& operator+=(float scalar);
        RgbSpectrum& operator-=(float scalar);
        RgbSpectrum& operator*=(float scalar);
        RgbSpectrum& operator/=(float scalar);
        const RgbSpectrum operator+(float scalar) const;
        const RgbSpectrum operator-(float scalar) const;
        const RgbSpectrum operator*(float scalar) const;
        const RgbSpectrum operator/(float scalar) const;
        

        float getLuminance() const;
        float maxCoeff() const;

        RgbSpectrum clamp() const;

        bool isValid() const;
        bool isZero() const;

        RgbSpectrum toSrgb() const;
    };

    const RgbSpectrum operator+(float scalar, const RgbSpectrum& rgbSpectrum);
    const RgbSpectrum operator*(float scalar, const RgbSpectrum& rgbSpectrum);
}