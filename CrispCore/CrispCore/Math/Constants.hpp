#pragma once

namespace crisp
{
    template<class T = float>
    static constexpr T PI = T(3.14159265358979323846);

    template<class T = float>
    static constexpr T InvPI = T(1.0) / PI<T>;

    template<class T = float>
    static constexpr T InvTwoPI = T(1.0) / (T(2.0) * PI<T>);

    template<class T = float>
    static constexpr T InvFourPI = T(1.0) / (T(4.0) * PI<T>);

    template<class T = float>
    static constexpr T SqrtTwo = T(1.41421356237309504880);

    template<class T = float>
    static constexpr T InvSqrtTwo = T(1.0) / SqrtTwo<T>;
}