#pragma once

#define NOMINMAX

#include <memory>
#include <atomic>

#include <tbb/concurrent_queue.h>

#include "ApplicationEnvironment.hpp"

#include <Vesper/RayTracer.hpp>
#include <Vesper/RayTracerUpdate.hpp>

#include <CrispCore/Timer.hpp>
#include "Math/Headers.hpp"
#include "Core/FrameTimeLogger.hpp"

namespace crisp
{
    class Window;
    class InputDispatcher;
    class VulkanRenderer;
    class BackgroundImage;
    class RayTracedImage;
    class Scene;

    namespace gui
    {
        class Form;
    }

    class Application
    {
    public:
        static constexpr const char* Title = "Crisp";
        static constexpr double TimePerFrame = 1.0 / 60.0;
        static constexpr int DefaultWindowWidth  = 1280;
        static constexpr int DefaultWindowHeight = 720;

        Application(ApplicationEnvironment* environment);
        ~Application();

        Application(const Application& other) = delete;
        Application(Application&& other) = delete;
        Application& operator=(const Application& other) = delete;
        Application& operator=(Application&& other) = delete;

        void run();
        void quit();

        void onResize(int width, int height);

        void createScene();

        void startRayTracing();
        void stopRayTracing();
        void openSceneFileFromDialog();
        void openSceneFile(std::string filename);
        void writeImageToExr();

        gui::Form* getForm() const;

        Event<float, float> rayTracerProgressed;

    private:
        void createWindow();
        void createRenderer();

        void processRayTracerUpdates();

        FrameTimeLogger<Timer<std::milli>> m_frameTimeLogger;

        std::unique_ptr<Window>          m_window;
        std::unique_ptr<InputDispatcher> m_inputDispatcher;
        std::unique_ptr<VulkanRenderer>  m_renderer;

        std::unique_ptr<BackgroundImage> m_backgroundImage;

        std::unique_ptr<gui::Form> m_guiForm;

        uint32_t m_numRayTracedChannels;
        std::string m_projectName;
        std::unique_ptr<RayTracedImage> m_rayTracedImage;
        tbb::concurrent_queue<vesper::RayTracerUpdate> m_rayTracerUpdateQueue;
        std::atomic<float> m_tracerProgress;
        std::atomic<float> m_timeSpentRendering;

        std::unique_ptr<vesper::RayTracer> m_rayTracer;
        std::vector<float> m_rayTracedImageData;

        std::unique_ptr<Scene> m_scene;
    };
}