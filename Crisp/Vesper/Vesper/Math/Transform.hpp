#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include "Ray.hpp"

namespace vesper
{
    struct Transform
    {
        glm::mat4 mat;
        glm::mat4 inv;

        Transform()
            : mat(1.0f)
            , inv(1.0f)
        {
        }

        Transform(const glm::mat4& mat)
            : mat(mat)
            , inv(glm::inverse(mat))
        {
        }

        Transform(const glm::mat4& mat, const glm::mat4& inv)
            : mat(mat)
            , inv(inv)
        {
        }

        Transform invert() const
        {
            return Transform(inv, mat);
        }

        glm::vec3 transformPoint(const glm::vec3& point) const
        {
            auto temp = mat * glm::vec4(point, 1.0f);
            return glm::vec3(temp) / temp.w;
        }

        glm::vec3 transformDir(const glm::vec3& dir) const
        {
            return glm::normalize(glm::vec3(mat * glm::vec4(dir, 0.0f)));
        }
        
        glm::vec3 transformNormal(const glm::vec3& normal) const
        {
            return glm::normalize(glm::vec3(glm::transpose(inv) * glm::vec4(normal, 0.0f)));
        }

        Transform operator*(const Transform& t) const
        {
            return Transform(mat * t.mat, t.inv * inv);
        }

        Ray3 operator*(const Ray3& ray) const
        {
            auto tempO = mat * glm::vec4(ray.o, 1.0f);
            auto tempD = mat * glm::vec4(ray.d, 0.0f);

            return Ray3(glm::vec3(tempO) / tempO.w, glm::vec3(tempD), ray.minT, ray.maxT);
        }

        Transform& translate(const glm::vec3& v)
        {
            mat = glm::translate(v) * mat;
            inv = inv * glm::translate(-v);
            return *this;
        }

        Transform& scale(const glm::vec3& v)
        {
            mat = glm::scale(v) * mat;
            inv = inv * glm::scale(1.0f / v);
            return *this;
        }

        Transform& rotate(float angle, const glm::vec3& axis)
        {
            mat = glm::rotate(angle, axis) * mat;
            inv = inv * glm::rotate(-angle, axis);
            return *this;
        }

        static Transform createTranslation(const glm::vec3& v)
        {
            return glm::translate(v);
        }

        static Transform createScale(const glm::vec3& v)
        {
            return glm::scale(v);
        }

        static Transform createRotation(float angle, const glm::vec3& axis)
        {
            return glm::rotate(angle, glm::normalize(axis));
        }
    };
}