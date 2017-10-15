#pragma once

#include <vector>
#include <string>

#include "ApplicationEnvironment.hpp"
#include "Math/Headers.hpp"

namespace crisp
{
    class Window
    {
    public:
        Window(int x, int y, int width, int height, std::string title);
        ~Window();

        static glm::ivec2 getDesktopResolution();
        static void pollEvents();

        bool shouldClose() const;
        void close();

        GLFWwindow* getHandle() const;

        static std::vector<std::string> getVulkanExtensions();
        VkResult createRenderingSurface(VkInstance instance, const VkAllocationCallbacks* allocCallbacks, VkSurfaceKHR* surface) const;

    private:
        GLFWwindow* m_window;
    };
}