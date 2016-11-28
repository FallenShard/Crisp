#include "Application.hpp"

#include <iostream>
#include <iomanip>
#include <string>
#include <chrono>

#include <glm/gtc/type_ptr.hpp>

#include "Vulkan/VulkanRenderer.hpp"
#include "Core/InputDispatcher.hpp"
#include "Core/Timer.hpp"
#include "Core/Image.hpp"
#include "Core/Picture.hpp"
#include "Core/FrameTimeLogger.hpp"
#include "IO/FileUtils.hpp"

#include "GUI/RenderSystem.hpp"
#include "GUI/TopLevelGroup.hpp"
#include "GUI/Button.hpp"

namespace crisp
{
    namespace
    {
        std::string ApplicationTitle = "Crisp";

        glm::vec4 clearColor = glm::vec4(0.0f);

        FrameTimeLogger<Timer<std::milli>> frameTimeLogger(500);

        constexpr double timePerFrame = 1.0 / 60.0;

        Application* app; // as static pointer for global mouse position retrieval
    }

    Application::Application()
        : m_window(nullptr, glfwDestroyWindow)
        , m_tracerProgress(0.0f)
    {
        std::cout << "Initializing application...\n";

        createWindow();
        app = this;
        initVulkan();

        m_inputDispatcher = std::make_unique<InputDispatcher>(m_window.get());

        m_guiTopLevelGroup = std::make_unique<gui::TopLevelGroup>(std::make_unique<gui::RenderSystem>(m_renderer.get()));

        m_inputDispatcher->windowResized.subscribe<Application, &Application::onResize>(this);

        frameTimeLogger.onFpsUpdated.subscribe<gui::TopLevelGroup, &gui::TopLevelGroup::setFpsString>(m_guiTopLevelGroup.get());
        
        m_inputDispatcher->mouseMoved.subscribe<gui::TopLevelGroup, &gui::TopLevelGroup::onMouseMoved>(m_guiTopLevelGroup.get());
        m_inputDispatcher->mouseButtonDown.subscribe<gui::TopLevelGroup, &gui::TopLevelGroup::onMousePressed>(m_guiTopLevelGroup.get());
        m_inputDispatcher->mouseButtonUp.subscribe<gui::TopLevelGroup, &gui::TopLevelGroup::onMouseReleased>(m_guiTopLevelGroup.get());

        m_rayTracer = std::make_unique<vesper::RayTracer>();
        m_rayTracer->setImageSize(DefaultWindowWidth, DefaultWindowHeight);
        m_rayTracer->setProgressUpdater([this](float value, float renderTime, vesper::ImageBlockEventArgs eventArgs)
        {
            m_tracerProgress = value;
            m_timeSpentRendering = renderTime;
            m_rayTracerUpdateQueue.push(eventArgs);
        });

        m_guiTopLevelGroup->getButton()->setClickCallback([this]()
        {
            auto sceneFile = "Resources/VesperScenes/" + FileUtils::openFileDialog();

            m_rayTracer->initializeScene(sceneFile);
            m_rayTracer->start();
            m_rayTracingStarted = true;
        });

        m_rayTracedImage = std::make_unique<Picture>(DefaultWindowWidth, DefaultWindowHeight, VK_FORMAT_R32G32B32A32_SFLOAT, *m_renderer);
    }

    Application::~Application()
    {
        m_rayTracer.reset();
        m_rayTracedImage.reset();
        m_guiTopLevelGroup.reset();
        m_renderer.reset();
        m_window.reset();
        glfwTerminate();
    }

    void Application::initDependencies()
    {
        if (glfwInit() == GLFW_FALSE)
        {
            std::cerr << "Could not initialize GLFW library!" << std::endl;
        }
    }

    void Application::run()
    {
        std::cout << "Hello world!\n";

        frameTimeLogger.start();
        Timer<std::milli> updateTimer;
        double timeSinceLastUpdate = 0.0;
        while (!glfwWindowShouldClose(m_window.get()))
        {
            auto timeDelta = updateTimer.restart() / 1000.0;
            timeSinceLastUpdate += timeDelta;

            while (timeSinceLastUpdate > timePerFrame)
            {
                glfwPollEvents();
                m_guiTopLevelGroup->update(timePerFrame);
                
                processRayTracerUpdates();
                m_guiTopLevelGroup->setTracerProgress(m_tracerProgress, m_timeSpentRendering);
            
                timeSinceLastUpdate -= timePerFrame;
            }

            m_rayTracedImage->draw();
            m_guiTopLevelGroup->draw();

            m_renderer->drawFrame();

            frameTimeLogger.restart();
        }

        m_renderer->finish();
    }

    void Application::onResize(int width, int height)
    {
        std::cout << "New window dims: (" << width << ", " << height << ")\n";

        m_renderer->resize(width, height);

        m_rayTracedImage->resize();
        m_guiTopLevelGroup->resize(width, height);
    }

    void Application::createWindow()
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        m_window.reset(glfwCreateWindow(DefaultWindowWidth, DefaultWindowHeight, ApplicationTitle.c_str(), nullptr, nullptr));
    }

    void Application::initVulkan()
    {
        SurfaceCreator surfaceCreatorCallback = [this](VkInstance instance, const VkAllocationCallbacks* allocCallbacks, VkSurfaceKHR* surface) -> VkResult
        {
            return glfwCreateWindowSurface(instance, this->m_window.get(), allocCallbacks, surface);
        };
        
        std::vector<const char*> extensions;
        unsigned int glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        for (unsigned int i = 0; i < glfwExtensionCount; i++)
            extensions.push_back(glfwExtensions[i]);
        
        m_renderer = std::make_unique<VulkanRenderer>(surfaceCreatorCallback, std::forward<std::vector<const char*>>(extensions));
    }

    glm::vec2 Application::getMousePosition()
    {
        double x, y;
        glfwGetCursorPos(app->m_window.get(), &x, &y);
        return glm::vec2(static_cast<float>(x), static_cast<float>(y));
    }

    void Application::processRayTracerUpdates()
    {
        while (!m_rayTracerUpdateQueue.empty())
        {
            vesper::ImageBlockEventArgs update;
            if (m_rayTracerUpdateQueue.try_pop(update))
                m_rayTracedImage->updateTexture(update);
        }
    }
}