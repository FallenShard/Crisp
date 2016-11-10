#pragma once

#define NOMINMAX

#include <memory>
#include <atomic>

//#define USE_VULKAN

#ifdef USE_VULKAN
#include <vulkan/vulkan.h>
#else
#include <glad/glad.h>
#endif

#include <glm/glm.hpp>

#include <GLFW/glfw3.h>

#include <tbb/concurrent_queue.h>

#include <RayTracer.hpp>
#include <ImageBlockEventArgs.hpp>

namespace crisp
{
    class InputDispatcher;
    class VulkanRenderer;
    class OpenGLRenderer;
    
    namespace gui
    {
        class TopLevelGroup;
    }

    class Application
    {
    public:
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
        void initVulkan();
        void initOpenGL();

        void processRayTracerUpdates();

        std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> m_window;

        //std::unique_ptr<VulkanRenderer> m_renderer;
        std::unique_ptr<OpenGLRenderer> m_renderer;

        GLuint m_backgroundTex;
        GLuint m_rayTracerTex;
        GLuint m_sampler;
        bool m_rayTracingStarted = false;

        std::unique_ptr<InputDispatcher> m_inputDispatcher;
        std::unique_ptr<gui::TopLevelGroup> m_guiTopLevelGroup;

        std::atomic<float> m_tracerProgress;
        std::atomic<float> m_timeSpentRendering;

        std::unique_ptr<vesper::RayTracer> m_rayTracer;
        tbb::concurrent_queue<vesper::ImageBlockEventArgs> m_rayTracerUpdateQueue;
    };
}