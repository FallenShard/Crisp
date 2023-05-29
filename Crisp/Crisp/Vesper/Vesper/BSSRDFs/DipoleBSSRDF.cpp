#include "DipoleBSSRDF.hpp"

#include <functional>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

#include "Cameras/Camera.hpp"
#include "Core/Scene.hpp"
#include "Integrators/Integrator.hpp"
#include "IrradianceTree.hpp"
#include "Samplers/SamplerFactory.hpp"
#include "Shapes/Shape.hpp"
#include <Crisp/Math/Constants.hpp>
#include <Crisp/Math/Octree.hpp>
#include <Crisp/Math/Warp.hpp>

namespace crisp
{
namespace
{
inline float Fdr(float eta)
{
    if (eta >= 1.0f)
        return -1.4399f / (eta * eta) + 0.7099f / eta + 0.0636f * eta + 0.6681f;
    else
        return -0.4399f + 0.7099f / eta - 0.3319f / (eta * eta) + 0.0636f / (eta * eta * eta);
}

struct DipoleIrradianceQuery
{
    DipoleIrradianceQuery(const Spectrum& zPos, const Spectrum& zNeg, const Spectrum& sigmaTr, const Spectrum& alpha)
        : zPos(zPos)
        , zNeg(zNeg)
        , sigmaTr(sigmaTr)
        , alpha(alpha)
    {
    }

    Spectrum operator()(float sqrDist) const
    {
        Spectrum dPos = Spectrum(sqrDist) + zPos * zPos;
        dPos.r = std::sqrt(dPos.r);
        dPos.g = std::sqrt(dPos.g);
        dPos.b = std::sqrt(dPos.b);

        Spectrum dNeg = Spectrum(sqrDist) + zNeg * zNeg;
        dNeg.r = std::sqrt(dNeg.r);
        dNeg.g = std::sqrt(dNeg.g);
        dNeg.b = std::sqrt(dNeg.b);

        Spectrum c1 = zPos * (dPos * sigmaTr + Spectrum(1.0f));
        Spectrum c2 = zNeg * (dNeg * sigmaTr + Spectrum(1.0f));

        Spectrum radiance = InvFourPI<> * ((c1 * (-sigmaTr * dPos).exp()) / (dPos * dPos * dPos) +
                                           (c2 * (-sigmaTr * dNeg).exp()) / (dNeg * dNeg * dNeg));
        return radiance.clamp();
    }

    const Spectrum& zPos;
    const Spectrum& zNeg;
    const Spectrum& sigmaTr;
    const Spectrum& alpha;
};

struct SubsurfaceParams
{
    Spectrum sigmaA;
    Spectrum sigmaPrimeS;
    float eta;
    // TODO: add presets
};

static std::unordered_map<std::string, SubsurfaceParams> materials = {
    {"Ketchup", {{0.061f, 0.97f, 1.45f}, {0.18f, 0.07f, 0.03f}, 1.3f}}
};

struct IrradianceTask
{
    IrradianceTask(int numLightSamples, const Scene* scene, const Integrator* integrator)
        : numLightSamples(numLightSamples)
        , scene(scene)
        , integrator(integrator)
    {
    }

    int numLightSamples;
    const Scene* scene;
    const Integrator* integrator;

    void operator()(
        std::vector<IrradiancePoint>& out, const std::vector<SurfacePoint>& in, size_t start, size_t end) const
    {
        std::default_random_engine engine(std::random_device{}());
        std::uniform_real_distribution<float> distrib(0.0f, 1.0f);
        auto sampler = SamplerFactory::create("independent", VariantMap());

        for (size_t i = start; i < end; i++)
        {
            Spectrum color(0.0f);
            for (int s = 0; s < numLightSamples; s++)
            {
                Light::Sample sample(in[i].p);
                Intersection its;
                its.p = in[i].p;
                Spectrum lightVal = scene->sampleLight(its, *sampler, sample);
                auto cosFactor = glm::dot(in[i].n, sample.wi);

                if (!(cosFactor <= 0.0f || lightVal.isZero()))
                    color += cosFactor * lightVal;

                CoordinateFrame frame(in[i].n);
                glm::vec3 dir =
                    frame.toWorld(warp::squareToCosineHemisphere(glm::vec2(distrib(engine), distrib(engine))));
                Ray3 ray(in[i].p, dir);

                // TODO: Request ONLY INDIRECT
                color += integrator->Li(scene, *sampler, ray, Illumination::Indirect) * PI<>;
            }

            color /= static_cast<float>(numLightSamples);
            out[i] = IrradiancePoint(in[i], color);
        }
    }
};

class Executor
{
public:
    Executor(int numThreads)
        : m_threads(numThreads)
    {
    }

    template <typename OutType, typename InType, typename Func>
    void map(std::vector<OutType>& out, const std::vector<InType>& in, Func&& f)
    {
        size_t itemsPerThread = out.size() / m_threads.size();
        size_t leftOver = out.size() % m_threads.size();
        size_t addedLeftOver = 0;
        for (size_t i = 0; i < m_threads.size(); i++)
        {
            size_t start = i * itemsPerThread + addedLeftOver;
            size_t end = start + itemsPerThread;
            if (leftOver > 0)
            {
                end++;
                addedLeftOver++;
                leftOver--;
            }

            m_threads[i] = std::thread(
                [&out, &in, f, start, end]
                {
                    f(out, in, start, end);
                });
        }

        for (auto& thread : m_threads)
            thread.join();
    }

private:
    std::vector<std::thread> m_threads;
};
} // namespace

DipoleBSSRDF::DipoleBSSRDF(const VariantMap& /*params*/)
{
    std::string mat = "Ketchup"; // params.get("material", "");
    SubsurfaceParams ssParams = materials[mat];
    m_sigmaA = ssParams.sigmaA;
    m_sigmaPrimeS = ssParams.sigmaPrimeS;
    m_eta = ssParams.eta;

    m_maxError = 0.5f;
    m_distMultiplier = 1.0f;
    m_sampleTrials = 1'000'000;
    m_debugMode = false;
    float scale = 50.0f;

    m_sigmaA *= scale;
    m_sigmaPrimeS *= scale;
    m_sigmaPrimeT = m_sigmaA + m_sigmaPrimeS;

    Spectrum meanFreePath = Spectrum(1.0f) / m_sigmaPrimeT;

    m_minDist = meanFreePath[0];
    for (int lambda = 1; lambda < 3; lambda++)
        m_minDist = std::min(m_minDist, meanFreePath[lambda]);

    m_minDist = m_minDist * m_distMultiplier / std::sqrtf(20.0f);

    m_fdr = Fdr(1.0f / m_eta);

    float A = (1.0f + m_fdr) / (1.0f - m_fdr);

    m_sigmaTr = m_sigmaA * m_sigmaPrimeT * 3.0f;
    m_sigmaTr.r = std::sqrt(m_sigmaTr.r);
    m_sigmaTr.g = std::sqrt(m_sigmaTr.g);
    m_sigmaTr.b = std::sqrt(m_sigmaTr.b);

    m_alphaPrime = m_sigmaPrimeS / m_sigmaPrimeT;

    m_zPos = Spectrum(1.0f) / m_sigmaPrimeT;
    m_zNeg = m_zPos * (1.0f + 4.0f / 3.0f * A);
}

void DipoleBSSRDF::preprocess(const Shape* shape, const Scene* scene)
{
    auto sampler = SamplerFactory::create("independent", VariantMap());

    Ray3 dummy;
    Spectrum value = scene->getCamera()->sampleRay(dummy, glm::vec2(0.0f, 0.0f), sampler->next2D());
    glm::vec3 cameraPos = dummy.o;

    BoundingBox3 shapeBounds = shape->getBoundingBox();
    float expandFactor = 0.001f * std::powf(shapeBounds.getVolume(), 1.0f / 3.0f);
    shapeBounds.expandBy(expandFactor);
    m_octree = std::make_unique<Octree<SurfacePoint>>(shapeBounds);

    Shape::Sample shapeSample(cameraPos);

    std::vector<SurfacePoint> surfacePoints;
    for (int i = 0; i < m_sampleTrials; i++)
    {
        glm::vec2 sample = sampler->next2D();
        shape->sampleSurface(shapeSample, *sampler);

        PoissonCheck check(m_minDist, shapeSample.p);
        m_octree->lookup(shapeSample.p, check);
        if (!check.failed)
        {
            glm::vec3 delta(m_minDist);
            float area = PI<> * m_minDist * m_minDist / 4.0f;
            SurfacePoint surfPt(shapeSample.p, shapeSample.n, area);
            m_octree->add(surfPt, BoundingBox3(shapeSample.p - delta, shapeSample.p + delta));
            surfacePoints.emplace_back(surfPt);
        }

        if (i % (m_sampleTrials / 10) == 0)
            std::cout << static_cast<float>(i) / m_sampleTrials * 100.0f << "% complete...\n";
    }

    m_irrPts.resize(surfacePoints.size());
    int numLightSamples = 16;
    const Integrator* integrator = scene->getIntegrator();

    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads <= 1)
        IrradianceTask(numLightSamples, scene, integrator)(m_irrPts, surfacePoints, 0, surfacePoints.size());
    else
    {
        Executor exec(numThreads);
        exec.map(m_irrPts, surfacePoints, IrradianceTask(numLightSamples, scene, integrator));
    }

    float areaPerElement = 1.0f / shape->pdfSurface(Shape::Sample()) / m_irrPts.size();

    for (int i = 0; i < m_irrPts.size(); i++)
    {
        m_irrPts[i].area = areaPerElement;
    }

    std::cout << "Building the irradiance cache...\n";

    m_ssOctree = std::make_unique<IrradianceTree>();
    m_ssOctree->root = std::make_unique<IrradianceNode>();
    for (const auto& pt : m_irrPts)
        m_ssOctree->boundingBox.expandBy(pt.p);

    for (auto& pt : m_irrPts)
        m_ssOctree->root->insert(m_ssOctree->boundingBox, &pt);
    m_ssOctree->root->initHierarchy();

    Spectrum leafIrradiance = m_ssOctree->root->getLeafIrradiance();

    std::cout << "LEAF: " << leafIrradiance.r << " " << leafIrradiance.g << " " << leafIrradiance.b << std::endl;
}

Spectrum DipoleBSSRDF::eval(const Sample& sample) const
{
    float cosThetaO = glm::dot(sample.n, sample.wo);
    if (cosThetaO <= 0.0f)
        return Spectrum(0.0f);

    if (m_debugMode)
    {
        PoissonCheck check(m_minDist / 5.0f, sample.p);
        m_octree->lookup(sample.p, check);
        if (check.failed)
            return Spectrum(1.0f, 0.0f, 0.0f);
        else
            return Spectrum(0.0f, 1.0f, 0.0f);
    }

    return Spectrum();
}
} // namespace crisp