#pragma once

#include <memory>
#include <vector>

#pragma warning(push)
#pragma warning(disable : 4324) // alignment warning
#include <embree4/rtcore.h>
#include <embree4/rtcore_ray.h>
#pragma warning(pop)

#include <Crisp/Math/BoundingBox.hpp>
#include <Crisp/Math/Ray.hpp>
#include <Crisp/PathTracer/Core/Intersection.hpp>
#include <Crisp/PathTracer/Lights/Light.hpp>
#include <Crisp/PathTracer/Spectra/Spectrum.hpp>

namespace crisp {
class Integrator;
class Sampler;
class Camera;
class Shape;
class BSDF;
class Mesh;

namespace pt {

class Scene {
public:
    Scene();
    ~Scene();

    void setIntegrator(std::unique_ptr<Integrator> integrator);
    void setSampler(std::unique_ptr<Sampler> sampler);
    void setCamera(std::unique_ptr<Camera> camera);
    void addShape(std::unique_ptr<Shape> shape, BSDF* bsdf, Light* light = nullptr);
    void addLight(std::unique_ptr<Light> light);
    void addEnvironmentLight(std::unique_ptr<Light> light);
    void addBSDF(std::unique_ptr<BSDF> bsdf);

    void finishInitialization();

    const Sampler* getSampler() const;
    const Integrator* getIntegrator() const;
    const Camera* getCamera() const;
    std::vector<std::unique_ptr<Shape>>& getShapes();

    Light* getRandomLight(float sample) const;
    float getLightPdf() const;
    Spectrum sampleLight(const Intersection& its, Sampler& sampler, Light::Sample& lightSample) const;

    Spectrum evalEnvLight(const Ray3& ray) const;
    Light* getEnvironmentLight() const;

    bool rayIntersect(const Ray3& ray, Intersection& its) const;
    bool rayIntersect(const Ray3& shadowRay) const;

    BoundingBox3 getBoundingBox() const;
    glm::vec4 getBoundingSphere() const;

private:
    RTCDevice m_device;
    RTCScene m_scene;

    std::unique_ptr<Sampler> m_sampler;
    std::unique_ptr<Integrator> m_integrator;
    std::unique_ptr<Camera> m_camera;

    std::vector<std::unique_ptr<Shape>> m_shapes;
    std::vector<std::unique_ptr<Light>> m_lights;
    std::vector<std::unique_ptr<BSDF>> m_bsdfs;

    Light* m_envLight;

    glm::ivec2 m_imageSize;

    glm::vec4 m_boundingSphere;
    BoundingBox3 m_boundingBox;
};
} // namespace pt
} // namespace crisp