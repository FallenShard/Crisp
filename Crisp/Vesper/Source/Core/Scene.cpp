#include "Scene.hpp"

#include "Samplers/Independent.hpp"
#include "Integrators/Normals.hpp"
#include "Cameras/Perspective.hpp"
#include "Shapes/Mesh.hpp"
#include "Lights/PointLight.hpp"
#include "BSDFs/Lambertian.hpp"

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
        if (shape->addToAccelerationStructure(this))
        {
            if (light)
            {
                shape->setLight(light);
                light->setShape(shape.get());
            }

            shape->setBSDF(bsdf);

            m_shapes.emplace_back(std::move(shape));
        }
    }

    void Scene::addLight(std::unique_ptr<Light> light)
    {
        m_lights.emplace_back(std::move(light));
    }

    void Scene::addBSDF(std::unique_ptr<BSDF> bsdf)
    {
        m_bsdfs.emplace_back(std::move(bsdf));
    }

    void Scene::finishInitialization()
    {
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

    const Light* Scene::getRandomLight(float sample) const
    {
        auto numLights = m_lights.size();
        auto index = std::min(static_cast<size_t>(std::floor(numLights * sample)), numLights - 1);
        return m_lights[index].get();
    }

    float Scene::getLightPdf() const
    {
        return 1.0f / static_cast<float>(m_lights.size());
    }

    Spectrum Scene::sampleLight(const Intersection& its, Sampler& sampler, Light::Sample& lightSample) const
    {
        auto light = getRandomLight(sampler.next1D());

        Spectrum lightContrib = light->sample(lightSample, sampler);

        if (lightSample.pdf != 0)
        {
            lightContrib *= 1.0f / getLightPdf();
            lightSample.pdf *= getLightPdf();
            return lightContrib;
        }

        return Spectrum(0.0f);
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

    void Scene::addMesh(const Mesh* mesh)
    {
        unsigned int geomId = rtcNewTriangleMesh(m_scene, RTC_GEOMETRY_STATIC, mesh->getNumTriangles(), mesh->getNumVertices(), 1);

        Vertex* vertices = static_cast<Vertex*>(rtcMapBuffer(m_scene, geomId, RTC_VERTEX_BUFFER));
        auto& positions = mesh->getVertexPositions();
        for (int i = 0; i < positions.size(); i++)
        {
            vertices[i].x = positions[i].x;
            vertices[i].y = positions[i].y;
            vertices[i].z = positions[i].z;
            vertices[i].w = 1.f;
        }
        rtcUnmapBuffer(m_scene, geomId, RTC_VERTEX_BUFFER);
        
        Face* faces = static_cast<Face*>(rtcMapBuffer(m_scene, geomId, RTC_INDEX_BUFFER));
        auto& triangles = mesh->getTriangleIndices();
        for (int i = 0; i < mesh->getTriangleIndices().size(); i++)
        {
            faces[i].v0 = triangles[i].x;
            faces[i].v1 = triangles[i].y;
            faces[i].v2 = triangles[i].z;
        }
        rtcUnmapBuffer(m_scene, geomId, RTC_INDEX_BUFFER);
    }
}