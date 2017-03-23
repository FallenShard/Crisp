#pragma once

#define NOMINMAX

#include <memory>
#include <atomic>

#include <tbb/concurrent_queue.h>

#include "ApplicationEnvironment.hpp"

#include <Vesper/RayTracer.hpp>
#include <Vesper/RayTracerUpdate.hpp>

#include "Math/Headers.hpp"
#include "Core/Timer.hpp"
#include "Core/FrameTimeLogger.hpp"
#include "Core/Scene.hpp"
#include "Core/StaticPicture.hpp"

namespace crisp
{
    class InputDispatcher;
    class VulkanRenderer;
    class Picture;

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

        Application();
        ~Application();

        Application(const Application& other) = delete;
        Application(Application&& other) = delete;
        Application& operator=(const Application& other) = delete;
        Application& operator=(Application&& other) = delete;

        void run();

        void onResize(int width, int height);

        void startRayTracing();
        void stopRayTracing();
        void openSceneFile();
        void writeImageToExr();

        gui::Form* getForm() const;

        Event<void, float, float> rayTracerProgressed;

    private:
        void createWindow();
        void createRenderer();

        void processRayTracerUpdates();

        std::unique_ptr<FrameTimeLogger<Timer<std::milli>>> m_frameTimeLogger;

        std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> m_window;
        std::unique_ptr<VulkanRenderer> m_renderer;
        std::unique_ptr<InputDispatcher> m_inputDispatcher;

        std::unique_ptr<StaticPicture> m_backgroundPicture;

        std::unique_ptr<gui::Form> m_guiForm;

        uint32_t m_numRayTracedChannels;
        std::string m_projectName;
        std::unique_ptr<Picture> m_rayTracedImage;
        tbb::concurrent_queue<vesper::RayTracerUpdate> m_rayTracerUpdateQueue;
        std::atomic<float> m_tracerProgress;
        std::atomic<float> m_timeSpentRendering;

        std::unique_ptr<vesper::RayTracer> m_rayTracer;
        std::vector<float> m_rayTracedImageData;

        std::unique_ptr<Scene> m_scene;
    };
}