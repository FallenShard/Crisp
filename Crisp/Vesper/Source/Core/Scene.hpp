#pragma once

#include <vector>
#include <memory>

#include <embree2/rtcore.h>
#include <embree2/rtcore_ray.h>

#include "Spectrums/Spectrum.hpp"
#include "Core/Intersection.hpp"
#include "Math/Ray.hpp"
#include "Lights/Light.hpp"

namespace vesper
{
    class Integrator;
    class Sampler;
    class Camera;
    class Shape;
    class BSDF;

    class Mesh;

    class Scene
    {
    public:
        Scene();
        ~Scene();

        void setIntegrator(std::unique_ptr<Integrator> integrator);
        void setSampler(std::unique_ptr<Sampler> sampler);
        void setCamera(std::unique_ptr<Camera> camera);
        void addShape(std::unique_ptr<Shape> shape, BSDF* bsdf, Light* light = nullptr);
        void addLight(std::unique_ptr<Light> light);
        void addBSDF(std::unique_ptr<BSDF> bsdf);

        void finishInitialization();

        const Sampler* getSampler() const;
        const Integrator* getIntegrator() const;
        const Camera* getCamera() const;

        const Light* getRandomLight(float sample) const;
        float getLightPdf() const;
        Spectrum sampleLight(const Intersection& its, Sampler& sampler, Light::Sample& lightSample) const;

        bool rayIntersect(const Ray3& ray, Intersection& its) const;
        bool rayIntersect(const Ray3& shadowRay) const;

        void addMesh(const Mesh* mesh);

    private:
        RTCDevice m_device;
        RTCScene m_scene;

        std::unique_ptr<Sampler>  m_sampler;
        std::unique_ptr<Integrator> m_integrator;
        std::unique_ptr<Camera> m_camera;

        std::vector<std::unique_ptr<Shape>> m_shapes;
        std::vector<std::unique_ptr<Light>> m_lights;
        std::vector<std::unique_ptr<BSDF>> m_bsdfs;

        glm::ivec2 m_imageSize;

        struct Vertex
        {
            float x, y, z, w;
        };

        struct Face
        {
            unsigned int v0, v1, v2;
        };
    };
}