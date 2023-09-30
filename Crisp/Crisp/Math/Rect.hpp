#pragma once

#include <Crisp/Math/Headers.hpp>

#include <iostream>

namespace crisp {
template <typename T>
struct Rect {
    T x;
    T y;
    T width;
    T height;

    template <typename U>
    bool contains(U x, U y) {
        return x >= this->x && x < this->x + width && y >= this->y && y < this->y + height;
    }

    bool operator!=(const Rect& other) {
        return x != other.x || y != other.y || width != other.width || height != other.height;
    }

    Rect merge(const Rect& other) {
        T resRight = std::max(x + width, other.x + other.width);
        T resBottom = std::max(y + height, other.y + other.height);
        T resLeft = std::min(x, other.x);
        T resTop = std::min(y, other.y);
        return {resLeft, resTop, resRight - resLeft, resBottom - resTop};
    }
};

template <typename T>
std::ostream& operator<<(std::ostream& out, const Rect<T>& rect) {
    std::cout << "[ " << rect.x << ", " << rect.y << ", " << rect.width << ", " << rect.height << " ]";
    return out;
}
} // namespace crisp