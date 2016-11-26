#pragma once

#define NOMINMAX

#include <memory>
#include <atomic>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <tbb/concurrent_queue.h>

#include <RayTracer.hpp>
#include <ImageBlockEventArgs.hpp>

namespace crisp
{
    class InputDispatcher;
    class VulkanRenderer;
    class OpenGLRenderer;
    class Picture;

    namespace gui
    {
        class TopLevelGroup;
    }

    class Application
    {
    public:
        static constexpr int DefaultWindowWidth  = 960;
        static constexpr int DefaultWindowHeight = 540;

        Application();
        ~Application();

        Application(const Application& other) = delete;
        Application& operator=(const Application& other) = delete;
        Application& operator=(Application&& other) = delete;

        static void initDependencies();

        static glm::vec2 getMousePosition();

        void run();

        void onResize(int width, int height);

    private:
        void createWindow();
        void initVulkan();

        void processRayTracerUpdates();

        std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> m_window;
        std::unique_ptr<VulkanRenderer> m_renderer;
        std::unique_ptr<InputDispatcher> m_inputDispatcher;

        std::unique_ptr<gui::TopLevelGroup> m_guiTopLevelGroup;

        std::unique_ptr<Picture> m_background;
        std::unique_ptr<vesper::RayTracer> m_rayTracer;
        tbb::concurrent_queue<vesper::ImageBlockEventArgs> m_rayTracerUpdateQueue;
        std::atomic<float> m_tracerProgress;
        std::atomic<float> m_timeSpentRendering;
        bool m_rayTracingStarted = false;
    };
}