#pragma once

#include <algorithm>
#include <glm/vec3.hpp>

namespace vesper
{
    struct RgbSpectrum
    {
#pragma warning(push)
#pragma warning(disable: 4201) // nameless struct
        union
        {
            float values[3];
            struct
            {
                float r, g, b;
            };
        };
#pragma warning(pop)

        RgbSpectrum(float value = 0.0f);
        RgbSpectrum(float red, float green, float blue);
        RgbSpectrum(const glm::vec3& vec);

        float& operator[](int index);
        const float& operator[](int index) const;
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
        bool isInfinite() const;
        bool isNaN() const;

        RgbSpectrum toSrgb() const;
    };

    const RgbSpectrum operator+(float scalar, const RgbSpectrum& rgbSpectrum);
    const RgbSpectrum operator*(float scalar, const RgbSpectrum& rgbSpectrum);
    std::ostream& operator<<(std::ostream& stream, const RgbSpectrum& spec);
}