#pragma once

#include <Crisp/GUI/RenderSystem.hpp>

namespace crisp::gui
{
    struct RectDrawComponent
    {
        RenderSystem* renderSystem;
        unsigned int transformId;
        unsigned int colorId;

        RectDrawComponent(RenderSystem* renderSystem)
            : renderSystem(renderSystem)
            , transformId(renderSystem->registerTransformResource())
            , colorId(renderSystem->registerColorResource())
        {
        }

        ~RectDrawComponent()
        {
            renderSystem->unregisterTransformResource(transformId);
            renderSystem->unregisterColorResource(colorId);
        }

        inline void update(const glm::vec4& color)
        {
            renderSystem->updateColorResource(colorId, color);
        }

        inline void update(const glm::mat4& transform)
        {
            renderSystem->updateTransformResource(transformId, transform);
        }

        inline void draw(float depthLayer) const
        {
            renderSystem->drawQuad(transformId, colorId, depthLayer);
        }
    };
}