#include "Application.hpp"

#include <iostream>
#include <iomanip>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <chrono>

#include "Core/InputDispatcher.hpp"
#include "Core/Timer.hpp"
#include "Core/Image.hpp"
#include "FrameTimeLogger.hpp"

#include "GUI/TopLevelGroup.hpp"
#include "GUI/Button.hpp"

#include "OpenGL/OpenGLRenderer.hpp"
#include "OpenGL/OpenGLDiagnostics.hpp"

#include "RayTracer.hpp"

#include "ShaderLoader.hpp"

#include <glad/wglext.h>

namespace crisp
{
    namespace
    {
        int DefaultWindowWidth = 960;
        int DefaultWindowHeight = 540;
        std::string ApplicationTitle = "Crisp (Also known as Nemanja is a yappie :0)";

        glm::vec4 clearColor = glm::vec4(0.0f);

        FrameTimeLogger<Timer<std::milli>> frameTimeLogger(500);

        constexpr double timePerFrame = 1.0 / 60.0;

        Application* app;

        GLuint m_fsProg;
        GLuint m_quadTexProg;
        GLuint m_quadVbo;
        GLuint m_quadIbo;
        GLuint m_quadVao;
    }

    Application::Application()
        : m_window(nullptr, glfwDestroyWindow)
        , m_tracerProgress(0.0f)
    {
        std::cout << "Initializing our world...\n";

        //if (useVulkan)
        //    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        m_window.reset(glfwCreateWindow(DefaultWindowWidth, DefaultWindowHeight, ApplicationTitle.c_str(), nullptr, nullptr));
        app = this;

        //initVulkan();
        initOpenGL();

        glCreateTextures(GL_TEXTURE_2D, 1, &m_rayTracerTex);
        glTextureStorage2D(m_rayTracerTex, 1, GL_RGB32F, DefaultWindowWidth, DefaultWindowHeight);
        glClearTexSubImage(m_rayTracerTex, 1, 0, 0, 0, DefaultWindowWidth, DefaultWindowHeight, 0, GL_RGB, GL_FLOAT, glm::value_ptr(clearColor));

        auto image = std::make_unique<Image>("Resources/Textures/crisp.png", true);

        glCreateTextures(GL_TEXTURE_2D, 1, &m_backgroundTex);
        glTextureStorage2D(m_backgroundTex, 1, GL_RGBA8, image->getWidth(), image->getHeight());
        glTextureSubImage2D(m_backgroundTex, 0, 0, 0, image->getWidth(), image->getHeight(), GL_RGBA, GL_UNSIGNED_BYTE, image->getData());

        glCreateSamplers(1, &m_sampler);
        glSamplerParameteri(m_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glSamplerParameteri(m_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glSamplerParameteri(m_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glSamplerParameteri(m_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindSampler(0, m_sampler);

        std::vector<glm::vec2> vertices =
        {
            { -1.0f, -1.0f },
            { +1.0f, -1.0f },
            { +1.0f, +1.0f },
            { -1.0f, +1.0f }
        };
        glCreateBuffers(1, &m_quadVbo);
        glNamedBufferStorage(m_quadVbo, vertices.size() * sizeof(glm::vec2), vertices.data(), GL_MAP_WRITE_BIT);

        std::vector<glm::u16vec3> faces =
        {
            { 0, 1, 2 },
            { 0, 2, 3 }
        };
        glCreateBuffers(1, &m_quadIbo);
        glNamedBufferStorage(m_quadIbo, faces.size() * sizeof(glm::u16vec3), faces.data(), GL_MAP_WRITE_BIT);

        glCreateVertexArrays(1, &m_quadVao);

        glEnableVertexArrayAttrib(m_quadVao, 0);
        glVertexArrayAttribFormat(m_quadVao, 0, 2, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribBinding(m_quadVao, 0, 0);
        glVertexArrayVertexBuffer(m_quadVao, 0, m_quadVbo, 0, sizeof(glm::vec2));

        glVertexArrayElementBuffer(m_quadVao, m_quadIbo);

        m_inputDispatcher = std::make_unique<InputDispatcher>(m_window.get());
        m_guiTopLevelGroup = std::make_unique<gui::TopLevelGroup>();

        m_inputDispatcher->windowResized.subscribe<Application, &Application::onResize>(this);

        frameTimeLogger.onFpsUpdated.subscribe<gui::TopLevelGroup, &gui::TopLevelGroup::setFpsString>(m_guiTopLevelGroup.get());

        m_inputDispatcher->mouseMoved.subscribe<gui::TopLevelGroup, &gui::TopLevelGroup::onMouseMoved>(m_guiTopLevelGroup.get());
        m_inputDispatcher->mouseButtonDown.subscribe<gui::TopLevelGroup, &gui::TopLevelGroup::onMousePressed>(m_guiTopLevelGroup.get());
        m_inputDispatcher->mouseButtonUp.subscribe<gui::TopLevelGroup, &gui::TopLevelGroup::onMouseReleased>(m_guiTopLevelGroup.get());

        ShaderLoader loader;
        m_quadTexProg = loader.load("gamma-correct-vert.glsl", "gamma-correct-frag.glsl");
        m_fsProg = loader.load("fullscreen-quad-vert.glsl", "fullscreen-quad-frag.glsl");
        glProgramUniform1i(m_quadTexProg, glGetUniformLocation(m_quadTexProg, "tex"), 0);
        glProgramUniform1i(m_fsProg, glGetUniformLocation(m_fsProg, "tex"), 0);

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
            m_rayTracer->initializeScene("Resources/VesperScenes/example.json");
            m_rayTracer->start();
            m_rayTracingStarted = true;
        });

        //PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
        //wglSwapIntervalEXT(0);

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glClearDepth(1.0);
        glDepthRange(0.0, 1.0);
    }

    Application::~Application()
    {
        glDeleteTextures(1, &m_rayTracerTex);
        glDeleteTextures(1, &m_backgroundTex);
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

            glClear(GL_DEPTH_BUFFER_BIT);
            glClearBufferfv(GL_COLOR, 0, glm::value_ptr(clearColor));
            
            glDisable(GL_DEPTH_TEST);
            if (!m_rayTracingStarted)
            {
                glUseProgram(m_fsProg);
                glBindTextureUnit(0, m_backgroundTex);
                glBindVertexArray(m_quadVao);
                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
                glBindVertexArray(0);
            }
            else
            {
                glUseProgram(m_quadTexProg);
                glBindTextureUnit(0, m_rayTracerTex);
                glBindVertexArray(m_quadVao);
                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
                glBindVertexArray(0);
            }

            glEnable(GL_DEPTH_TEST);
            m_guiTopLevelGroup->draw();

            glfwSwapBuffers(m_window.get());

            frameTimeLogger.restart();
        }
    }

    void Application::onResize(int width, int height)
    {
        std::cout << "New dims: (" << width << ", " << height << ")\n";

        glViewport(0, 0, width, height);

        m_guiTopLevelGroup->resize(width, height);
    }

    void Application::initVulkan()
    {
        //SurfaceCreator surfaceCreatorCallback = [this](VkInstance instance, const VkAllocationCallbacks* allocCallbacks, VkSurfaceKHR* surface) -> VkResult
        //{
        //    return glfwCreateWindowSurface(instance, this->m_window.get(), allocCallbacks, surface);
        //};
        //
        //std::vector<const char*> extensions;
        //unsigned int glfwExtensionCount = 0;
        //const char** glfwExtensions;
        //glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        //for (unsigned int i = 0; i < glfwExtensionCount; i++)
        //    extensions.push_back(glfwExtensions[i]);
        //
        //m_renderer = std::make_unique<VulkanRenderer>(surfaceCreatorCallback, std::forward<std::vector<const char*>>(extensions));
    }

    void Application::initOpenGL()
    {
        glfwMakeContextCurrent(m_window.get());
        gladLoadGLLoader((GLADloadproc)(glfwGetProcAddress));

        m_renderer = std::make_unique<OpenGLRenderer>();

#ifdef _DEBUG
        OpenGLDiagnostics::enable();
#endif
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
                glTextureSubImage2D(m_rayTracerTex, 0, update.x, update.y, update.width, update.height, GL_RGB, GL_FLOAT, update.data.data());
        }
    }
}