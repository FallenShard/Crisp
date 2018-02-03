#pragma once

#include <memory>
#include <atomic>
#include <string>
#include <vector>

#include <tbb/concurrent_queue.h>

#include <Vesper/RayTracerUpdate.hpp>
#include <CrispCore/Event.hpp>
#include "Scene.hpp"

namespace vesper
{
    class RayTracer;
}

namespace crisp
{
    class Application;
    class VulkanRenderer;
    class RayTracedImage;

    class RayTracerScene : public Scene
    {
    public:
        RayTracerScene(VulkanRenderer* renderer, Application* app);
        ~RayTracerScene();

        virtual void resize(int width, int height) override;
        virtual void update(float dt) override;
        virtual void render() override;

        Event<float, float> rayTracerProgressed;

        void openSceneFileFromDialog();
        void startRendering();
        void stopRendering();
        void writeImageToExr();

    private:
        void openSceneFile(const std::string& filename);

        VulkanRenderer* m_renderer;
        Application* m_app;

        uint32_t m_numChannels;
        std::string m_projectName;
        std::unique_ptr<RayTracedImage> m_image;
        tbb::concurrent_queue<vesper::RayTracerUpdate> m_updateQueue;
        std::atomic<float> m_progress;
        std::atomic<float> m_timeSpentRendering;

        std::unique_ptr<vesper::RayTracer> m_rayTracer;
        std::vector<float> m_imageData;
    };
}