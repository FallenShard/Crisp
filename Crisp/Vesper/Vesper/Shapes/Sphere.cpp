#include "Sphere.hpp"

#include "Math/Warp.hpp"
#include "Samplers/Sampler.hpp"

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

        inline bool analyticIntersection(const glm::vec3& center, float radius, const RTCRay& ray, float& t)
        {
            glm::vec3 centerToOrigin(ray.org[0] - center.x, ray.org[1] - center.y, ray.org[2] - center.z);

            float b = 2.0f * (ray.dir[0] * centerToOrigin.x + ray.dir[1] * centerToOrigin.y + ray.dir[2] * centerToOrigin.z);
            float c = glm::dot(centerToOrigin, centerToOrigin) - radius * radius;

            float discrim = b * b - 4.0f * c;

            if (discrim < 0.0f) // both complex solutions, no intersection
            {
                return false;
            }
            else if (discrim < Ray3::Epsilon) // Discriminant is practically zero
            {
                t = -0.5f * b;
            }
            else
            {
                float factor = (b > 0.0f) ? -0.5f * (b + sqrtf(discrim)) : -0.5f * (b - sqrtf(discrim));
                float t0 = factor;
                float t1 = c / factor;

                if (t0 > t1) // Prefer the smaller t, select larger if smaller is less than tNear
                {
                    t = t1 < ray.tnear ? t0 : t1;
                }
                else
                {
                    t = t0 < ray.tnear ? t1 : t0;
                }
            }

            if (t < ray.tnear || t > ray.tfar)
                return false;
            else
                return true;
        }
    }

    Sphere::Sphere(const VariantMap& params)
    {
        m_center = params.get<glm::vec3>("center", glm::vec3(0.0f));
        m_radius = params.get<float>("radius", 1.0f);

        m_boundingBox.expandBy(m_center);
        m_boundingBox.expandBy(m_radius);
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

        float t;
        if (analyticIntersection(sphere.m_center, sphere.m_radius, ray, t))
        {
            ray.tfar = t;
            ray.geomID = sphere.m_geomId;
            ray.primID = static_cast<unsigned int>(item);
        }
    }

    void Sphere::rayOcclude(const Sphere* spheres, RTCRay& ray, size_t item)
    {
        const Sphere& sphere = spheres[item];

        float t;
        if (analyticIntersection(sphere.m_center, sphere.m_radius, ray, t))
        {
            ray.geomID = 0;
        }
    }

    void Sphere::fillIntersection(unsigned int triangleId, const Ray3& ray, Intersection& its) const
    {
        its.p = ray.o + ray.d * its.tHit;
        glm::vec3 normal = glm::normalize(its.p - m_center);

        its.geoFrame = CoordinateFrame(normal);
        its.shFrame  = CoordinateFrame(normal);

        auto alpha = asinf(normal.z);
        auto beta = atan2f(normal.y, normal.x);

        float u = clamp(0.5f + beta * InvTwoPI, 0.0f, 1.0f);
        float v = clamp(0.5f - alpha * InvPI, 0.0f, 1.0f);
        its.uv = { u, 1.0f - v };

        its.shape = this;
    }

    void Sphere::sampleSurface(Shape::Sample& shapeSample, Sampler& sampler) const
    {
        float invRad2 = 1.0f / (m_radius * m_radius);
        // Uniform sampling
        //glm::vec3 q = Warp::squareToUniformSphere(sampler.next2D());
        //shapeSample.p = m_center + m_radius * q;
        //shapeSample.n = q;
        //shapeSample.pdf = invRad2 * Warp::squareToUniformSpherePdf();

        // Spherical cap sampling
        glm::vec3 centerToRef = shapeSample.ref - m_center;

        float dist = glm::length(centerToRef);

        centerToRef = glm::normalize(centerToRef);
        CoordinateFrame frame(centerToRef);

        float cosThetaMax = std::min(m_radius / dist, 1.0f - Ray3::Epsilon);
        glm::vec3 localQ = Warp::squareToUniformSphereCap(sampler.next2D(), cosThetaMax);
        glm::vec3 worldQ = frame.toWorld(localQ);
        shapeSample.p = m_center + m_radius * worldQ;
        shapeSample.n = worldQ;
        shapeSample.pdf = invRad2 * Warp::squareToUniformSphereCapPdf(cosThetaMax);
    }

    float Sphere::pdfSurface(const Shape::Sample& shapeSample) const
    {
        float invRad2 = 1.0f / (m_radius * m_radius);
        //return invRad2 * Warp::squareToUniformSpherePdf();

        glm::vec3 centerToRef = shapeSample.ref - m_center;

        float dist = glm::length(centerToRef);

        centerToRef = glm::normalize(centerToRef);
        CoordinateFrame frame(centerToRef);

        float cosThetaMax = std::min(m_radius / dist, 1.0f - Ray3::Epsilon);

        return invRad2 * Warp::squareToUniformSphereCapPdf(cosThetaMax);
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