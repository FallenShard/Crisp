#include "Scene.hpp"

#include <Crisp/PathTracer/BSDFs/BSDF.hpp>
#include <Crisp/PathTracer/Core/Intersection.hpp>
#include <Crisp/PathTracer/Core/Scene.hpp>
#include <Crisp/PathTracer/Lights/Light.hpp>
#include <Crisp/PathTracer/Samplers/Sampler.hpp>
#include <Crisp/PathTracer/Shapes/Shape.hpp>

#include <Crisp/PathTracer/Cameras/Perspective.hpp>
#include <Crisp/PathTracer/Integrators/Normals.hpp>
#include <Crisp/PathTracer/Lights/PointLight.hpp>
#include <Crisp/PathTracer/Samplers/Independent.hpp>
#include <Crisp/PathTracer/Shapes/Mesh.hpp>

#include <Crisp/Core/Logger.hpp>

namespace crisp {
namespace {
auto logger = spdlog::stderr_color_mt("pt::Scene");

void logEmbreeError(void*, RTCError code, const char* str) {
    logger->error("Error code {} - {}", static_cast<uint32_t>(code), str);
}
} // namespace

namespace pt {
Scene::Scene()
    : m_device(rtcNewDevice(nullptr))
    , m_scene(rtcNewScene(m_device))
    , m_sampler(nullptr)
    , m_integrator(nullptr)
    , m_camera(nullptr)
    , m_envLight(nullptr)
    , m_imageSize{}
    , m_boundingSphere{} {
    rtcSetDeviceErrorFunction(m_device, logEmbreeError, this);

    m_integrator = std::make_unique<NormalsIntegrator>();
    m_sampler = std::make_unique<IndependentSampler>();
    m_camera = std::make_unique<PerspectiveCamera>();

    m_imageSize = m_camera->getImageSize();
}

Scene::~Scene() {
    rtcReleaseScene(m_scene);
    rtcReleaseDevice(m_device);
}

void Scene::setIntegrator(std::unique_ptr<Integrator> integrator) {
    m_integrator = std::move(integrator);
}

void Scene::setSampler(std::unique_ptr<Sampler> sampler) {
    m_sampler = std::move(sampler);
}

void Scene::setCamera(std::unique_ptr<Camera> camera) {
    m_camera = std::move(camera);
}

void Scene::addShape(std::unique_ptr<Shape> shape, BSDF* bsdf, Light* light) {
    if (shape->addToAccelerationStructure(m_device, m_scene)) {
        if (light) {
            shape->setLight(light);
            light->setShape(shape.get());
        }

        shape->setBSDF(bsdf);

        m_boundingBox.expandBy(shape->getBoundingBox());

        m_shapes.emplace_back(std::move(shape));
    }
}

void Scene::addLight(std::unique_ptr<Light> light) {
    m_lights.emplace_back(std::move(light));
}

void Scene::addEnvironmentLight(std::unique_ptr<Light> light) {
    m_lights.emplace_back(std::move(light));
    m_envLight = m_lights.back().get();
}

void Scene::addBSDF(std::unique_ptr<BSDF> bsdf) {
    m_bsdfs.emplace_back(std::move(bsdf));
}

void Scene::finishInitialization() {
    auto center = m_boundingBox.getCenter();
    auto radius = m_boundingBox.radius();
    m_boundingSphere = glm::vec4(center, radius);
    if (m_envLight) {
        m_envLight->setBoundingSphere(m_boundingSphere);
    }

    rtcCommitScene(m_scene);

    int err = rtcGetDeviceError(m_device);
    if (err != RTC_ERROR_NONE) {
        logger->error("Embree error: {}", err);
    }
}

const Sampler* Scene::getSampler() const {
    return m_sampler.get();
}

const Integrator* Scene::getIntegrator() const {
    return m_integrator.get();
}

const Camera* Scene::getCamera() const {
    return m_camera.get();
}

std::vector<std::unique_ptr<Shape>>& Scene::getShapes() {
    return m_shapes;
}

Light* Scene::getRandomLight(float sample) const {
    const auto numLights = m_lights.size();
    if (numLights == 0) {
        return nullptr;
    }

    const auto index = std::min(static_cast<size_t>(std::floor(static_cast<float>(numLights) * sample)), numLights - 1);
    return m_lights[index].get();
}

float Scene::getLightPdf() const {
    return 1.0f / static_cast<float>(m_lights.size());
}

Light* Scene::getEnvironmentLight() const {
    return m_envLight;
}

Spectrum Scene::sampleLight(const Intersection& /*its*/, Sampler& sampler, Light::Sample& lightSample) const {
    auto light = getRandomLight(sampler.next1D());
    if (!light) {
        return {0.0f};
    }

    Spectrum lightContrib = light->sample(lightSample, sampler);

    if (lightSample.pdf == 0.0f) {
        return {0.0f};
    }

    lightContrib /= getLightPdf();
    lightSample.pdf *= getLightPdf();
    lightSample.light = light;
    return lightContrib;
}

Spectrum Scene::evalEnvLight(const Ray3& ray) const {
    Light::Sample sample(ray.o, ray(1.0f), ray.d);
    sample.wi = ray.d;
    return m_envLight ? m_envLight->eval(sample) : Spectrum(0.0f);
}

bool Scene::rayIntersect(const Ray3& ray, Intersection& its) const {
    RTCRayHit rayHit; // NOLINT
    rayHit.ray.org_x = ray.o.x;
    rayHit.ray.org_y = ray.o.y;
    rayHit.ray.org_z = ray.o.z;
    rayHit.ray.dir_x = ray.d.x;
    rayHit.ray.dir_y = ray.d.y;
    rayHit.ray.dir_z = ray.d.z;
    rayHit.ray.tnear = ray.minT;
    rayHit.ray.tfar = ray.maxT;
    rayHit.ray.mask = 0xFFFFFFFF;
    rayHit.ray.time = ray.time;
    rayHit.ray.flags = 0;

    rayHit.hit.geomID = RTC_INVALID_GEOMETRY_ID;

    rtcIntersect1(m_scene, &rayHit);

    if (rayHit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
        its.tHit = rayHit.ray.tfar;
        its.uv.x = rayHit.hit.u;
        its.uv.y = rayHit.hit.v;

        m_shapes[rayHit.hit.geomID]->fillIntersection(rayHit.hit.primID, ray, its);

        return true;
    }

    return false;
}

bool Scene::rayIntersect(const Ray3& shadowRay) const {
    RTCRay rtcRay; // NOLINT
    rtcRay.org_x = shadowRay.o.x;
    rtcRay.org_y = shadowRay.o.y;
    rtcRay.org_z = shadowRay.o.z;
    rtcRay.dir_x = shadowRay.d.x;
    rtcRay.dir_y = shadowRay.d.y;
    rtcRay.dir_z = shadowRay.d.z;
    rtcRay.tnear = shadowRay.minT;
    rtcRay.tfar = shadowRay.maxT;
    rtcRay.mask = 0xFFFFFFFF;
    rtcRay.time = shadowRay.time;

    rtcOccluded1(m_scene, &rtcRay);
    return rtcRay.tfar < 0.0f;
}

BoundingBox3 Scene::getBoundingBox() const {
    return m_boundingBox;
}

glm::vec4 Scene::getBoundingSphere() const {
    return m_boundingSphere;
}
} // namespace pt
} // namespace crisp