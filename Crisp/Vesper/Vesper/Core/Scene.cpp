#include "Scene.hpp"

#include "Samplers/Independent.hpp"
#include "Integrators/Normals.hpp"
#include "Cameras/Perspective.hpp"
#include "Shapes/Mesh.hpp"
#include "Lights/PointLight.hpp"
#include "BSDFs/BSDF.hpp"

#include "Math/Octree.hpp"

namespace vesper
{
    Scene::Scene()
        : m_device(rtcNewDevice(nullptr))
        , m_scene(nullptr)
    {
        m_scene = rtcDeviceNewScene(m_device, RTC_SCENE_STATIC, RTC_INTERSECT1);

        m_integrator = std::make_unique<NormalsIntegrator>();
        m_sampler = std::make_unique<IndependentSampler>();
        m_camera = std::make_unique<PerspectiveCamera>();

        m_imageSize = m_camera->getImageSize();
    }

    Scene::~Scene()
    {
        rtcDeleteScene(m_scene);
        rtcDeleteDevice(m_device);
    }

    void Scene::setIntegrator(std::unique_ptr<Integrator> integrator)
    {
        m_integrator = std::move(integrator);
    }

    void Scene::setSampler(std::unique_ptr<Sampler> sampler)
    {
        m_sampler = std::move(sampler);
    }

    void Scene::setCamera(std::unique_ptr<Camera> camera)
    {
        m_camera = std::move(camera);
    }

    void Scene::addShape(std::unique_ptr<Shape> shape, BSDF* bsdf, Light* light)
    {
        if (shape->addToAccelerationStructure(m_scene))
        {
            if (light)
            {
                shape->setLight(light);
                light->setShape(shape.get());
            }

            shape->setBSDF(bsdf);

            m_boundingBox.expandBy(shape->getBoundingBox());

            m_shapes.emplace_back(std::move(shape));
        }
    }

    void Scene::addLight(std::unique_ptr<Light> light)
    {
        m_lights.emplace_back(std::move(light));
    }

    void Scene::addEnvironmentLight(std::unique_ptr<Light> light)
    {
        m_lights.emplace_back(std::move(light));
        m_envLight = m_lights.back().get();
    }

    void Scene::addBSDF(std::unique_ptr<BSDF> bsdf)
    {
        m_bsdfs.emplace_back(std::move(bsdf));
    }

    void Scene::finishInitialization()
    {
        auto center = m_boundingBox.getCenter();
        auto radius = m_boundingBox.radius();
        m_boundingSphere = glm::vec4(center, radius);
        if (m_envLight)
        {
            m_envLight->setBoundingSphere(m_boundingSphere);
        }

        rtcCommit(m_scene);
    }

    const Sampler* Scene::getSampler() const
    {
        return m_sampler.get();
    }

    const Integrator* Scene::getIntegrator() const
    {
        return m_integrator.get();
    }

    const Camera* Scene::getCamera() const
    {
        return m_camera.get();
    }

    std::vector<std::unique_ptr<Shape>>& Scene::getShapes()
    {
        return m_shapes;
    }

    Light* Scene::getRandomLight(float sample) const
    {
        auto numLights = m_lights.size();
        auto index = std::min(static_cast<size_t>(std::floor(numLights * sample)), numLights - 1);
        return numLights == 0 ? nullptr : m_lights[index].get();
    }

    float Scene::getLightPdf() const
    {
        return 1.0f / static_cast<float>(m_lights.size());
    }

    Light* Scene::getEnvironmentLight() const
    {
        return m_envLight;
    }

    Spectrum Scene::sampleLight(const Intersection& its, Sampler& sampler, Light::Sample& lightSample) const
    {
        auto light = getRandomLight(sampler.next1D());
        if (!light)
            return Spectrum(0.0f);

        Spectrum lightContrib = light->sample(lightSample, sampler);

        if (lightSample.pdf == 0.0f)
            return Spectrum(0.0f);

        lightContrib /= getLightPdf();
        lightSample.pdf *= getLightPdf();
        lightSample.light = light;
        return lightContrib;
    }

    Spectrum Scene::evalEnvLight(const Ray3& ray) const
    {
        Light::Sample sample(ray.o, ray(1.0f), ray.d);
        sample.wi = ray.d;
        return m_envLight ? m_envLight->eval(sample) : Spectrum(0.0f);
    }

    bool Scene::rayIntersect(const Ray3& ray, Intersection& its) const
    {
        RTCRay rtcRay;

        for (auto i = 0; i < ray.o.length(); i++)
        {
            rtcRay.org[i] = ray.o[i];
            rtcRay.dir[i] = ray.d[i];
        }

        rtcRay.tnear = ray.minT;
        rtcRay.tfar  = ray.maxT;
        rtcRay.geomID = RTC_INVALID_GEOMETRY_ID;
        rtcRay.primID = RTC_INVALID_GEOMETRY_ID;
        rtcRay.instID = RTC_INVALID_GEOMETRY_ID;
        rtcRay.mask = 0xFFFFFFFF;
        rtcRay.time = ray.time;

        rtcIntersect(m_scene, rtcRay);

        if (rtcRay.geomID != RTC_INVALID_GEOMETRY_ID)
        {
            its.tHit = rtcRay.tfar;
            its.uv.x = rtcRay.u;
            its.uv.y = rtcRay.v;

            m_shapes[rtcRay.geomID]->fillIntersection(rtcRay.primID, ray, its);

            return true;
        }

        return false;
    }

    bool Scene::rayIntersect(const Ray3& shadowRay) const
    {
        RTCRay rtcRay;

        for (int i = 0; i < shadowRay.o.length(); i++)
        {
            rtcRay.org[i] = shadowRay.o[i];
            rtcRay.dir[i] = shadowRay.d[i];
        }
        rtcRay.tnear = shadowRay.minT;
        rtcRay.tfar  = shadowRay.maxT;
        rtcRay.geomID = RTC_INVALID_GEOMETRY_ID;
        rtcRay.primID = RTC_INVALID_GEOMETRY_ID;
        rtcRay.instID = RTC_INVALID_GEOMETRY_ID;
        rtcRay.mask = 0xFFFFFFFF;
        rtcRay.time = shadowRay.time;

        rtcOccluded(m_scene, rtcRay);
        return rtcRay.geomID == 0;
    }

    BoundingBox3 Scene::getBoundingBox() const
    {
        return m_boundingBox;
    }

    glm::vec4 Scene::getBoundingSphere() const
    {
        return m_boundingSphere;
    }
}