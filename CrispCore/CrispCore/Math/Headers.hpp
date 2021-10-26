#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#pragma warning(push)
#pragma warning(disable: 4201) // nameless struct
#pragma warning(disable: 4127)
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/euler_angles.hpp>
#pragma warning(pop)

#include <utility>
#include <type_traits>

namespace glm
{
    template <std::size_t I, glm::length_t N, typename T, qualifier Q>
    constexpr auto& get(glm::vec<N, T, Q>& v) noexcept { return v[I]; }

    template <std::size_t I, glm::length_t N, typename T, qualifier Q>
    constexpr const auto& get(const glm::vec<N, T, Q>& v) noexcept { return v[I]; }

    template <std::size_t I, glm::length_t N, typename T, qualifier Q>
    constexpr auto&& get(glm::vec<N, T, Q>&& v) noexcept { return std::move(v[I]); }
}

namespace std
{
    template <glm::length_t N, typename T, glm::qualifier Q>
    struct tuple_size<glm::vec<N, T, Q>> : integral_constant<std::size_t, N> { };

    template <std::size_t I, glm::length_t N, typename T, glm::qualifier Q>
    struct tuple_element<I, glm::vec<N, T, Q>>
    {
        using type = T;
    };
}
