#include "Sphere.hpp"

namespace vesper
{
    namespace
    {
        inline bool solveQuadratic(float a, float b, float c, float& x0, float& x1)
        {
            float D = b * b - 4.0f * a * c;
            if (D < 0)
            {
                return false;
            }
            else if (D < Ray3::Epsilon)
            {
                x0 = x1 = -0.5f * b / a;
            }
            else
            {
                float q = (b > 0) ? -0.5f * (b + std::sqrtf(D)) :
                                    -0.5f * (b - std::sqrtf(D));
                x0 = q / a;
                x1 = c / q;
            }

            if (x0 > x1)
                std::swap(x0, x1);

            return true;
        }
    }
    Sphere::Sphere(const VariantMap& params)
    {
        m_center = params.get<glm::vec3>("center", glm::vec3(0.0f));
        m_radius = params.get<float>("radius", 0.2f);


    }

    Sphere::~Sphere()
    {

    }

    void Sphere::fillBounds(const Sphere* spheres, size_t item, RTCBounds* bounds)
    {
        auto& s = spheres[item];
        bounds->lower_x = s.m_center.x - s.m_radius;
        bounds->lower_y = s.m_center.y - s.m_radius;
        bounds->lower_z = s.m_center.z - s.m_radius;
        bounds->upper_x = s.m_center.x + s.m_radius;
        bounds->upper_y = s.m_center.y + s.m_radius;
        bounds->upper_z = s.m_center.z + s.m_radius;
    }

    void Sphere::rayIntersect(const Sphere* spheres, RTCRay& ray, size_t item)
    {
        const Sphere& sphere = spheres[item];

        //glm::vec3 l = sphere.m_center - glm::vec3(ray.org[0], ray.org[1], ray.org[2]);
        //float s = (l.x * ray.dir[0] + l.y * ray.dir[1] + l.z * ray.dir[2]);
        //float l2 = glm::dot(l, l);
        //float r2 = sphere.m_radius * sphere.m_radius;
        //
        //if (s < 0.0f && l2 > r2)
        //    return;
        //
        //float m2 = l2 - s * s;
        //if (m2 > r2)
        //    return;
        //
        //float q = std::sqrtf(r2 - m2);
        //float t = (l2 > r2) ? s - q : s + q;
        //
        //if ((ray.tnear < t) && (t < ray.tfar))
        //{
        //    ray.u = 0.0f;
        //    ray.v = 0.0f;
        //    ray.tfar = t;
        //    ray.geomID = sphere.m_geomId;
        //    ray.primID = static_cast<unsigned int>(item);
        //}

        //float t0, t1;
        //glm::vec3 L = glm::vec3(ray.org[0], ray.org[1], ray.org[2]) - sphere.m_center;
        //float a = 1.0f;
        //float b = 2.0f * (L.x * ray.dir[0] + L.y * ray.dir[1] + L.z * ray.dir[2]);
        //float c = glm::dot(L, L) - sphere.m_radius * sphere.m_radius;
        //
        //if (!solveQuadratic(a, b, c, t0, t1))
        //    return; 
        //
        //if (t0 > t1) std::swap(t0, t1);
        //
        //if (t0 < 0.0f)// || t0 < ray.tnear)
        //{
        //    t0 = t1;
        //    if (t0 < 0.0f)// || t0 < ray.tnear)
        //        return;
        //}
        //
        //if (t0 > ray.tfar)
        //{
        //    return;
        //}
        //
        //ray.u = 0.0f;
        //ray.v = 0.0f;
        //ray.tfar = t0;
        //ray.geomID = sphere.m_geomId;
        //ray.primID = static_cast<unsigned int>(item);
        //ray.Ng[0] = ray.org[0] + t0 * ray.dir[0] - sphere.m_center[0];
        //ray.Ng[1] = ray.org[1] + t0 * ray.dir[1] - sphere.m_center[1];
        //ray.Ng[2] = ray.org[2] + t0 * ray.dir[2] - sphere.m_center[2];
        glm::vec3 cToO = glm::vec3(ray.org[0], ray.org[1], ray.org[2]) - sphere.m_center;
        
        const float B = 2.0f * (cToO.x * ray.dir[0] + cToO.y * ray.dir[1] + cToO.z * ray.dir[2]);
        const float C = glm::dot(cToO, cToO) - sphere.m_radius * sphere.m_radius;
        const float D = B * B - 4.0f * C;
        
        if (D < 0.0f) // No intersection
            return;
        
        const float Q = std::sqrtf(D);
        const float t0 = 0.5f * (-B - Q);
        const float t1 = 0.5f * (-B + Q);
        
        if ((ray.tnear < t0) && (t0 < ray.tfar))
        {
            ray.u = 0.0f;
            ray.v = 0.0f;
            ray.tfar = t0;
            ray.geomID = sphere.m_geomId;
            ray.primID = static_cast<unsigned int>(item);
            ray.Ng[0] = ray.org[0] + t0 * ray.dir[0] - sphere.m_center[0];
            ray.Ng[1] = ray.org[1] + t0 * ray.dir[1] - sphere.m_center[1];
            ray.Ng[2] = ray.org[2] + t0 * ray.dir[2] - sphere.m_center[2];
            return;
        }
        
        if ((ray.tnear < t1) && (t1 < ray.tfar))
        {
            ray.u = 0.0f;
            ray.v = 0.0f;
            ray.tfar = t1;
            ray.geomID = sphere.m_geomId;
            ray.primID = static_cast<unsigned int>(item);
            ray.Ng[0] = ray.org[0] + t0 * ray.dir[0] - sphere.m_center[0];
            ray.Ng[1] = ray.org[1] + t0 * ray.dir[1] - sphere.m_center[1];
            ray.Ng[2] = ray.org[2] + t0 * ray.dir[2] - sphere.m_center[2];
            return;
        }
    }

    void Sphere::rayOcclude(const Sphere* spheres, RTCRay& ray, size_t item)
    {
        const Sphere& sphere = spheres[item];
        glm::vec3 v = glm::vec3(ray.org[0], ray.org[1], ray.org[2]) - sphere.m_center;
        const float A = ray.dir[0] * ray.dir[0] + ray.dir[1] * ray.dir[1] + ray.dir[2] * ray.dir[2];
        const float B = 2.0f * (v.x * ray.dir[0] + v.y * ray.dir[1] + v.z * ray.dir[2]);
        const float C = glm::dot(v, v) - sphere.m_radius * sphere.m_radius;
        const float D = B*B - 4.0f*A*C;
        if (D < 0.0f) return;
        const float Q = std::sqrtf(D);
        const float rcpA = 1.0f / A;
        const float t0 = 0.5f*rcpA*(-B - Q);
        const float t1 = 0.5f*rcpA*(-B + Q);
        if ((ray.tnear < t0) & (t0 < ray.tfar)) {
            ray.geomID = 0;
        }
        if ((ray.tnear < t1) & (t1 < ray.tfar)) {
            ray.geomID = 0;
        }
    }

    void Sphere::fillIntersection(unsigned int triangleId, const Ray3& ray, Intersection& its) const
    {
        its.p = ray.o + ray.d * its.tHit;
        glm::vec3 normal = glm::normalize(its.p - m_center);

        its.geoFrame = CoordinateFrame(normal);
        its.shFrame = CoordinateFrame(normal);

        auto alpha = asinf(normal.z);
        auto beta = atan2f(normal.y, normal.x);

        float u = clamp(0.5f + beta * InvTwoPI, 0.0f, 1.0f);
        float v = clamp(0.5f - alpha * InvPI, 0.0f, 1.0f);
        its.uv = { u ,v };

        its.shape = this;
    }

    void Sphere::sampleSurface(Shape::Sample& shapeSample, Sampler& sampler) const
    {

    }

    float Sphere::pdfSurface(const Shape::Sample& shapeSample) const
    {
        return 0.0f;
    }

    bool Sphere::addToAccelerationStructure(RTCScene scene)
    {
        m_geomId = rtcNewUserGeometry(scene, 1);
        rtcSetUserData(scene, m_geomId, this);
        rtcSetBoundsFunction(scene, m_geomId, (RTCBoundsFunc)&fillBounds);
        rtcSetIntersectFunction(scene, m_geomId, (RTCIntersectFunc)&rayIntersect);
        rtcSetOccludedFunction(scene, m_geomId, (RTCOccludedFunc)&rayOcclude);

        return true;
    }
}