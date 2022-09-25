#include "RayTracer.hpp"

#include <chrono>
#include <iostream>
#include <pmmintrin.h>
#include <xmmintrin.h>

#include <tbb/blocked_range.h>
#include <tbb/concurrent_vector.h>
#include <tbb/parallel_for.h>

#include "Cameras/Camera.hpp"
#include "Core/Scene.hpp"
#include "Integrators/Integrator.hpp"
#include "Samplers/Sampler.hpp"

#include "Core/XmlSceneParser.hpp"

namespace crisp
{
namespace
{
const int DefaultImageWidth = 800;
const int DefaultImageHeight = 600;
const int BlockSize = 64;
} // namespace

RayTracer::RayTracer()
    : m_renderStatus(RenderStatus::Free)
    , m_progress(1.f)
    , m_scene(nullptr)
{
    // Recommended for embree before thread creation
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
}

RayTracer::~RayTracer()
{
    stop();
}

void RayTracer::initializeScene(const std::string& fileName)
{
    if (m_renderStatus == RenderStatus::Busy)
        return;

    XmlSceneParser xmlParser;
    m_scene = xmlParser.parse(fileName);
    if (m_scene)
    {
        m_image.initialize(m_scene->getCamera()->getImageSize(), m_scene->getCamera()->getReconstructionFilter());
        m_image.clear();
    }
    else
    {
        std::cerr << "Failed to load scene!" << std::endl;
    }
}

void RayTracer::start()
{
    if (m_renderStatus == RenderStatus::Busy || !m_scene)
        return;

    m_blocksRendered = 0;
    m_pixelsRendered = 0;
    m_timeSpentRendering = 0.0f;

    m_renderStatus = RenderStatus::Busy;
    m_renderThread = std::thread(
        [this]
        {
            auto size = m_image.getSize();
            generateImageBlocks(size.x, size.y);

            std::cout << "Using " << tbb::this_task_arena::max_concurrency() << " thread(s)." << std::endl;

            tbb::concurrent_vector<std::unique_ptr<Sampler>> samplers(m_totalBlocks);
            for (auto& it : samplers)
                it = m_scene->getSampler()->clone();

            auto renderImageBlocks = [&](const tbb::blocked_range<int>& range)
            {
                for (int i = range.begin(); i < range.end(); ++i)
                {
                    if (m_renderStatus == RenderStatus::Interrupted)
                        break;

                    ImageBlock::Descriptor desc;
                    if (m_descriptorQueue.try_pop(desc))
                    {
                        auto t1 = std::chrono::high_resolution_clock::now();
                        ImageBlock currBlock(desc.offset, desc.size, m_scene->getCamera()->getReconstructionFilter());
                        renderBlock(currBlock, *samplers.at(i), m_scene.get());
                        auto t2 = std::chrono::high_resolution_clock::now();
                        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();

                        RayTracerUpdate update;
                        update.x = desc.offset.x;
                        update.y = desc.offset.y;
                        update.width = desc.size.x;
                        update.height = desc.size.y;
                        update.data = currBlock.getRaw();
                        updateProgress(std::move(update), duration / 1'000'000'000.0f);
                    }
                }
            };

            tbb::blocked_range<int> range(0, static_cast<int>(m_descriptorQueue.unsafe_size()));

            const_cast<Integrator*>(m_scene->getIntegrator())->preprocess(m_scene.get());

            auto t1 = std::chrono::high_resolution_clock::now();
            tbb::parallel_for(range, renderImageBlocks);
            auto t2 = std::chrono::high_resolution_clock::now();

            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();

            std::cout << "Finished rendering scene. Computed in " << duration / 1'000'000'000.0 << " s. " << std::endl;

            if (m_renderStatus != RenderStatus::Interrupted)
            {
                m_renderThread.detach();
            }

            m_renderStatus = RenderStatus::Done;
        });
}

void RayTracer::stop()
{
    if (m_renderStatus == RenderStatus::Busy)
    {
        m_renderStatus = RenderStatus::Interrupted;
        m_renderThread.join();
        m_renderStatus = RenderStatus::Free;
        std::cout << "Rendering cancelled." << std::endl;
    }
}

void RayTracer::setImageSize(int width, int height)
{
    m_image.initialize(glm::ivec2(width, height), nullptr);
}

glm::ivec2 RayTracer::getImageSize() const
{
    return m_image.getSize();
}

void RayTracer::setProgressUpdater(std::function<void(RayTracerUpdate&&)> callback)
{
    m_progressUpdater = callback;
}

void RayTracer::updateProgress(RayTracerUpdate&& update, float blockRenderTime)
{
    std::lock_guard<std::mutex> lock(m_imageMutex);

    m_blocksRendered++;
    m_pixelsRendered += update.width * update.height;
    m_timeSpentRendering += blockRenderTime;
    update.totalTimeSpentRendering = m_timeSpentRendering;
    update.pixelsRendered = m_pixelsRendered;
    update.numPixels = m_image.getSize().x * m_image.getSize().y;

    if (m_progressUpdater)
    {
        m_progressUpdater(std::move(update));
    }
}

void RayTracer::generateImageBlocks(int width, int height)
{
    m_descriptorQueue.clear();
    int numRows = (height - 1) / BlockSize + 1;
    int numCols = (width - 1) / BlockSize + 1;

    std::vector<ImageBlock::Descriptor> descriptors;
    int currRow = 0;
    int currCol = 0;
    while (currRow < height)
    {
        int currHeight = BlockSize;
        if (currRow + BlockSize > height)
            currHeight = height - currRow;

        currCol = 0;
        while (currCol < width)
        {
            int currWidth = BlockSize;
            if (currCol + BlockSize > width)
                currWidth = width - currCol;

            ImageBlock::Descriptor desc(currCol, currRow, currWidth, currHeight);
            descriptors.push_back(desc);

            currCol += BlockSize;
        }

        currRow += BlockSize;
    }

    std::vector<std::vector<unsigned int>> indexMat(numRows, std::vector<unsigned>(numCols));
    for (int i = 0; i < numRows; ++i)
        for (int j = 0; j < numCols; ++j)
            indexMat[i][j] = i * numCols + j;

    std::vector<unsigned int> indices;

    int i;
    int k = 0;
    int l = 0;
    int m = numRows;
    int n = numCols;

    while (k < m && l < n)
    {
        for (i = l; i < n; ++i)
            indices.push_back(indexMat[k][i]);
        k++;
        for (i = k; i < m; ++i)
            indices.push_back(indexMat[i][n - 1]);
        n--;

        if (k < m)
        {
            for (i = n - 1; i >= l; --i)
                indices.push_back(indexMat[m - 1][i]);
            m--;
        }

        if (l < n)
        {
            for (i = m - 1; i >= k; --i)
                indices.push_back(indexMat[i][l]);
            l++;
        }
    }

    for (auto idx = 0; idx < indices.size() / 2; ++idx)
        std::swap(indices[idx], indices[indices.size() - 1 - idx]);

    for (auto idx : indices)
        m_descriptorQueue.push(descriptors[idx]);

    m_blocksRendered = 0;
    m_pixelsRendered = 0;
    m_totalBlocks = static_cast<int>(m_descriptorQueue.unsafe_size());
}

void RayTracer::renderBlock(ImageBlock& block, Sampler& sampler, const Scene* scene)
{
    block.clear();
    glm::ivec2 offset = block.getOffset();

    const Camera* camera = scene->getCamera();
    const Integrator* integrator = scene->getIntegrator();
    size_t numSamples = sampler.getSampleCount();
    sampler.prepare();

    for (int y = 0; y < block.getFullSize().y; ++y)
    {
        for (int x = 0; x < block.getFullSize().x; ++x)
        {
            for (int s = 0; s < numSamples; s++)
            {
                glm::vec2 pixelSample(x + offset.x + sampler.next1D(), y + offset.y + sampler.next1D());
                glm::vec2 apertureSample = sampler.next2D();

                Ray3 ray;
                Spectrum response = camera->sampleRay(ray, pixelSample, apertureSample);
                Spectrum radiance = integrator->Li(scene, sampler, ray);
                block.addSample(pixelSample, response * radiance);
            }
        }
    }
}
} // namespace crisp