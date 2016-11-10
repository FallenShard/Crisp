#include "VulkanRenderer.hpp"

namespace crisp
{
    namespace
    {

    }

    VulkanRenderer::VulkanRenderer(SurfaceCreator surfCreatorCallback, std::vector<const char*>&& extensions)
    {
        m_context = std::make_unique<VulkanContext>(surfCreatorCallback, std::forward<std::vector<const char*>>(extensions));

        m_device = std::make_unique<VulkanDevice>(m_context.get());
    }
}