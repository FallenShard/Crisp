#pragma once

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

#include <tbb/concurrent_queue.h>

#include "ImageBlock.hpp"
#include "RayTracerUpdate.hpp"

namespace crisp
{
    class Scene;
    class Sampler;

    class RayTracer
    {
    public:
        RayTracer();
        ~RayTracer();

        void initializeScene(const std::string& fileName);
        void start();
        void stop();

        void setImageSize(int width, int height);
        glm::ivec2 getImageSize() const;

        void setProgressUpdater(std::function<void(RayTracerUpdate&&)> callback);

    private:
        void updateProgress(RayTracerUpdate&& update, float blockRenderTime);
        void generateImageBlocks(int width, int height);

        void renderBlock(ImageBlock& block, Sampler& sampler, const Scene* scene);

        enum class RenderStatus
        {
            Free,
            Busy,
            Interrupted,
            Done
        };

        std::unique_ptr<Scene> m_scene;
        ImageBlock m_image;
        tbb::concurrent_queue<ImageBlock::Descriptor> m_descriptorQueue;

        std::function<void(RayTracerUpdate&&)> m_progressUpdater;

        std::thread m_renderThread;
        std::atomic<RenderStatus> m_renderStatus;
        std::atomic<float> m_progress;
        std::mutex m_imageMutex;

        float m_timeSpentRendering;
        int m_pixelsRendered;
        int m_blocksRendered;
        int m_totalBlocks;
    };
}