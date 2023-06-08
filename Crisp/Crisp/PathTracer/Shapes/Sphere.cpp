#include "Sphere.hpp"

#include <Crisp/Math/Warp.hpp>
#include <Crisp/PathTracer/Samplers/Sampler.hpp>

namespace crisp
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
        float q = (b > 0) ? -0.5f * (b + std::sqrtf(D)) : -0.5f * (b - std::sqrtf(D));
        x0 = q / a;
        x1 = c / q;
    }

    if (x0 > x1)
        std::swap(x0, x1);

    return true;
}

inline bool analyticIntersection(const glm::vec3& center, float radius, const RTCRay& ray, float& t)
{
    glm::vec3 centerToOrigin(ray.org_x - center.x, ray.org_y - center.y, ray.org_z - center.z);

    float b = 2.0f * (ray.dir_x * centerToOrigin.x + ray.dir_y * centerToOrigin.y + ray.dir_z * centerToOrigin.z);
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
} // namespace

Sphere::Sphere(const VariantMap& params)
{
    m_center = params.get<glm::vec3>("center", glm::vec3(0.0f));
    m_radius = params.get<float>("radius", 1.0f);

    m_boundingBox.expandBy(m_center);
    m_boundingBox.expandBy(m_radius);
}

Sphere::~Sphere() {}

void Sphere::fillBounds(const RTCBoundsFunctionArguments* args)
{
    const Sphere* s = static_cast<const Sphere*>(args->geometryUserPtr);
    args->bounds_o->lower_x = s->m_center.x - s->m_radius;
    args->bounds_o->lower_y = s->m_center.y - s->m_radius;
    args->bounds_o->lower_z = s->m_center.z - s->m_radius;
    args->bounds_o->upper_x = s->m_center.x + s->m_radius;
    args->bounds_o->upper_y = s->m_center.y + s->m_radius;
    args->bounds_o->upper_z = s->m_center.z + s->m_radius;
}

void Sphere::rayIntersect(const RTCIntersectFunctionNArguments* args)
{
    const Sphere* sphere = static_cast<const Sphere*>(args->geometryUserPtr);

    RTCRayN* rays = RTCRayHitN_RayN(args->rayhit, args->N);
    RTCHitN* hits = RTCRayHitN_HitN(args->rayhit, args->N);

    for (unsigned int i = 0; i < args->N; ++i)
    {
        RTCRay ray = rtcGetRayFromRayN(rays, args->N, i);
        float t;
        if (analyticIntersection(sphere->m_center, sphere->m_radius, ray, t))
        {
            RTCRayN_tfar(rays, args->N, i) = t;
            RTCHitN_geomID(hits, args->N, i) = sphere->m_geometryId;
            RTCHitN_primID(hits, args->N, i) = 0;
        }
    }
}

void Sphere::rayOcclude(const RTCOccludedFunctionNArguments* args)
{
    const Sphere* sphere = reinterpret_cast<const Sphere*>(args->geometryUserPtr);

    for (unsigned int i = 0; i < args->N; ++i)
    {
        RTCRay ray = rtcGetRayFromRayN(args->ray, args->N, i);

        float t;
        if (analyticIntersection(sphere->m_center, sphere->m_radius, ray, t))
        {
            RTCRayN_tfar(args->ray, args->N, i) = -std::numeric_limits<float>::infinity();
        }
    }
}

void Sphere::fillIntersection(unsigned int /*triangleId*/, const Ray3& ray, Intersection& its) const
{
    its.p = ray.o + ray.d * its.tHit;
    glm::vec3 normal = glm::normalize(its.p - m_center);

    its.geoFrame = CoordinateFrame(normal);
    its.shFrame = CoordinateFrame(normal);

    auto alpha = asinf(normal.z);
    auto beta = atan2f(normal.y, normal.x);

    const float u = clamp(0.5f + beta * InvTwoPI<float>, 0.0f, 1.0f);
    const float v = clamp(0.5f - alpha * InvPI<float>, 0.0f, 1.0f);
    its.uv = {u, 1.0f - v};

    its.shape = this;
}

void Sphere::sampleSurface(Shape::Sample& shapeSample, Sampler& sampler) const
{
    float invRad2 = 1.0f / (m_radius * m_radius);
    // Uniform sampling
    // glm::vec3 q = warp::squareToUniformSphere(sampler.next2D());
    // shapeSample.p = m_center + m_radius * q;
    // shapeSample.n = q;
    // shapeSample.pdf = invRad2 * warp::squareToUniformSpherePdf();

    // Spherical cap sampling
    glm::vec3 centerToRef = shapeSample.ref - m_center;

    float dist = glm::length(centerToRef);

    centerToRef = glm::normalize(centerToRef);
    CoordinateFrame frame(centerToRef);

    float cosThetaMax = std::min(m_radius / dist, 1.0f - Ray3::Epsilon);
    glm::vec3 localQ = warp::squareToUniformSphereCap(sampler.next2D(), cosThetaMax);
    glm::vec3 worldQ = frame.toWorld(localQ);
    shapeSample.p = m_center + m_radius * worldQ;
    shapeSample.n = worldQ;
    shapeSample.pdf = invRad2 * warp::squareToUniformSphereCapPdf(cosThetaMax);
}

float Sphere::pdfSurface(const Shape::Sample& shapeSample) const
{
    float invRad2 = 1.0f / (m_radius * m_radius);
    // return invRad2 * warp::squareToUniformSpherePdf();

    glm::vec3 centerToRef = shapeSample.ref - m_center;

    float dist = glm::length(centerToRef);

    centerToRef = glm::normalize(centerToRef);
    CoordinateFrame frame(centerToRef);

    float cosThetaMax = std::min(m_radius / dist, 1.0f - Ray3::Epsilon);

    return invRad2 * warp::squareToUniformSphereCapPdf(cosThetaMax);
}

bool Sphere::addToAccelerationStructure(RTCDevice device, RTCScene scene)
{
    m_geometry = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_USER);
    rtcSetGeometryUserPrimitiveCount(m_geometry, 1);
    rtcSetGeometryUserData(m_geometry, this);
    rtcSetGeometryBoundsFunction(m_geometry, &fillBounds, this);
    rtcSetGeometryIntersectFunction(m_geometry, &rayIntersect);
    rtcSetGeometryOccludedFunction(m_geometry, &rayOcclude);

    rtcCommitGeometry(m_geometry);
    m_geometryId = rtcAttachGeometry(scene, m_geometry);
    return true;
}
} // namespace crisp