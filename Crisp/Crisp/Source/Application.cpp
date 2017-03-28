#include "Application.hpp"

#include <iostream>
#include <iomanip>
#include <string>
#include <chrono>

#include "Vulkan/VulkanRenderer.hpp"
#include "Core/InputDispatcher.hpp"
#include "Core/Timer.hpp"
#include "Core/Picture.hpp"

#include "IO/FileUtils.hpp"
#include "IO/Image.hpp"
#include "IO/OpenEXRWriter.hpp"

#include "GUI/RenderSystem.hpp"
#include "GUI/Form.hpp"
#include "GUI/Button.hpp"
#include "GUI/VesperGui.hpp"

namespace crisp
{
    Application::Application()
        : m_window(nullptr, glfwDestroyWindow)
        , m_tracerProgress(0.0f)
        , m_frameTimeLogger(std::make_unique<FrameTimeLogger<Timer<std::milli>>>(200.0))
        , m_numRayTracedChannels(4)
    {
        std::cout << "Initializing application...\n";

        createWindow();
        createRenderer();

        m_inputDispatcher = std::make_unique<InputDispatcher>(m_window.get());
        m_inputDispatcher->windowResized.subscribe<Application, &Application::onResize>(this);

        m_backgroundPicture = std::make_unique<StaticPicture>("Resources/Textures/crisp.png", VK_FORMAT_R8G8B8A8_UNORM, *m_renderer);

        // Create and connect GUI with the mouse
        m_guiForm = std::make_unique<gui::Form>(std::make_unique<gui::RenderSystem>(m_renderer.get()));
        m_inputDispatcher->mouseMoved.subscribe<gui::Form, &gui::Form::onMouseMoved>(m_guiForm.get());
        m_inputDispatcher->mouseButtonPressed.subscribe<gui::Form, &gui::Form::onMousePressed>(m_guiForm.get());
        m_inputDispatcher->mouseButtonReleased.subscribe<gui::Form, &gui::Form::onMouseReleased>(m_guiForm.get());
        m_frameTimeLogger->onLoggerUpdated.subscribe<gui::Form, &gui::Form::setFpsString>(m_guiForm.get());

        // Create ray tracer and add a handler for image block updates
        m_rayTracer = std::make_unique<vesper::RayTracer>();
        m_rayTracer->setImageSize(DefaultWindowWidth, DefaultWindowHeight);
        m_rayTracer->setProgressUpdater([this](vesper::RayTracerUpdate update)
        {
            m_tracerProgress = static_cast<float>(update.pixelsRendered) / static_cast<float>(update.numPixels);
            m_timeSpentRendering = update.totalTimeSpentRendering;
            m_rayTracerUpdateQueue.emplace(update);
            rayTracerProgressed(m_tracerProgress, m_timeSpentRendering);
        });
        

        auto crispButton = m_guiForm->getControlById<gui::Button>("crispButton");
        crispButton->setClickCallback([this, form = m_guiForm.get()]()
        {
            form->postGuiUpdate([this, form]()
            {
                form->addMemoryUsagePanel();
                form->addStatusBar();

                m_scene = std::make_unique<Scene>(m_renderer.get(), m_inputDispatcher.get(), this);
            });

            form->fadeOutAndRemove("welcomePanel", 0.5f);
        });

        auto vesperButton = m_guiForm->getControlById<gui::Button>("vesperButton");
        vesperButton->setClickCallback([this, form = m_guiForm.get()]()
        {
            form->postGuiUpdate([this, form]()
            {
                form->addMemoryUsagePanel();
                form->addStatusBar();

                gui::VesperGui vesperGui;
                form->add(vesperGui.buildSceneOptions(form));
                form->add(vesperGui.buildProgressBar(form));

                vesperGui.setupInputCallbacks(form, this);
            });

            form->fadeOutAndRemove("welcomePanel", 0.5f);
        });

        //crispButton->click();
    }

    Application::~Application()
    {
    }

    void Application::run()
    {
        std::cout << "Hello world from Crisp! The application is up and running!\n";

        m_frameTimeLogger->restart();
        Timer<std::milli> updateTimer;
        double timeSinceLastUpdate = 0.0;
        while (!glfwWindowShouldClose(m_window.get()))
        {
            auto timeDelta = updateTimer.restart() / 1000.0;
            timeSinceLastUpdate += timeDelta;

            while (timeSinceLastUpdate > TimePerFrame)
            {
                glfwPollEvents();

                m_guiForm->update(TimePerFrame);

                if (m_scene)
                    m_scene->update(static_cast<float>(TimePerFrame));

                processRayTracerUpdates();

                timeSinceLastUpdate -= TimePerFrame;
            }

            if (m_scene)
                m_scene->render();
            else
                m_backgroundPicture->draw();

            if (m_rayTracedImage)
                m_rayTracedImage->draw();

            m_guiForm->draw();

            m_renderer->drawFrame();

            m_frameTimeLogger->update();
        }

        m_renderer->finish();
    }

    void Application::onResize(int width, int height)
    {
        std::cout << "New window dims: (" << width << ", " << height << ")\n";

        m_renderer->resize(width, height);
        m_backgroundPicture->resize(width, height);

        m_guiForm->resize(width, height);
        
        if (m_scene)
            m_scene->resize(width, height);

        if (m_rayTracedImage)
            m_rayTracedImage->resize(width, height);
    }

    void Application::startRayTracing()
    {
        m_rayTracer->start();
    }

    void Application::stopRayTracing()
    {
        m_rayTracer->stop();
    }

    void Application::openSceneFile()
    {
        auto openedFile = FileUtils::openFileDialog();
        if (openedFile == "")
            return;

        m_projectName = openedFile.substr(0, openedFile.length() - 4);
        m_rayTracer->initializeScene("Resources/VesperScenes/" + openedFile);

        auto imageSize = m_rayTracer->getImageSize();
        m_rayTracedImage = std::make_unique<Picture>(imageSize.x, imageSize.y, VK_FORMAT_R32G32B32A32_SFLOAT, *m_renderer);
        m_rayTracedImageData.resize(m_numRayTracedChannels * imageSize.x * imageSize.y);
    }

    void Application::writeImageToExr()
    {
        std::string outputDirectory = "Output";
        FileUtils::createDirectory(outputDirectory);
        OpenEXRWriter writer;
        auto imageSize = m_rayTracer->getImageSize();
        std::cout << "Writing EXR image..." << std::endl;
        writer.write(outputDirectory + "/" + m_projectName + ".exr", m_rayTracedImageData.data(), imageSize.x, imageSize.y, true);
    }

    gui::Form* Application::getForm() const
    {
        return m_guiForm.get();
    }

    void Application::createWindow()
    {
        // This hint is needed for Vulkan-based renderers
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        m_window.reset(glfwCreateWindow(DefaultWindowWidth, DefaultWindowHeight, Title, nullptr, nullptr));
    }

    void Application::createRenderer()
    {
        // Extensions required by the windowing library (GLFW)
        std::vector<const char*> extensions;
        unsigned int glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        for (unsigned int i = 0; i < glfwExtensionCount; i++)
            extensions.push_back(glfwExtensions[i]);

        // Window creation surface callback from the windowing library
        auto surfaceCreator = [this](VkInstance instance, const VkAllocationCallbacks* allocCallbacks, VkSurfaceKHR* surface) -> VkResult
        {
            return glfwCreateWindowSurface(instance, this->m_window.get(), allocCallbacks, surface);
        };
        
        m_renderer = std::make_unique<VulkanRenderer>(surfaceCreator, std::forward<std::vector<const char*>>(extensions));
    }

    void Application::processRayTracerUpdates()
    {
        while (!m_rayTracerUpdateQueue.empty())
        {
            vesper::RayTracerUpdate update;
            if (m_rayTracerUpdateQueue.try_pop(update))
            {
                m_rayTracedImage->postTextureUpdate(update);

                auto imageSize = m_rayTracer->getImageSize();
                for (int y = 0; y < update.height; y++)
                {
                    for (int x = 0; x < update.width; x++)
                    {
                        size_t rowIdx = update.y + y;
                        size_t colIdx = update.x + x;

                        for (int c = 0; c < 4; c++)
                        {
                            m_rayTracedImageData[(rowIdx * imageSize.x + colIdx) * 4 + c] = update.data[(y * update.width + x) * 4 + c];
                        }
                    }
                }
            }
        }
    }
}