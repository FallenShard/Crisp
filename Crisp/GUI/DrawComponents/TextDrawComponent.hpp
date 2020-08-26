#pragma once

#include "GUI/RenderSystem.hpp"

namespace crisp::gui
{
    struct TextDrawComponent
    {
        RenderSystem* renderSystem;
        unsigned int transformId;
        unsigned int colorId;
        unsigned int fontId;
        unsigned int textGeometryId;

        TextDrawComponent(RenderSystem* renderSystem, const std::string& text, const std::string& fontName, unsigned int fontSize)
            : renderSystem(renderSystem)
            , transformId(renderSystem->registerTransformResource())
            , colorId(renderSystem->registerColorResource())
            , fontId(renderSystem->getFont(fontName, fontSize))
            , textGeometryId(renderSystem->registerTextResource(text, fontId))
        {
        }

        ~TextDrawComponent()
        {
            renderSystem->unregisterTransformResource(transformId);
            renderSystem->unregisterColorResource(colorId);
            renderSystem->unregisterTextResource(textGeometryId);
        }

        inline void update(const glm::vec4& color)
        {
            renderSystem->updateColorResource(colorId, color);
        }

        inline void update(const glm::mat4& transform)
        {
            renderSystem->updateTransformResource(transformId, transform);
        }

        inline glm::vec2 update(const std::string& text)
        {
            return renderSystem->updateTextResource(textGeometryId, text, fontId);
        }

        inline void setFont(const std::string& fontName, unsigned int fontSize)
        {
            fontId = renderSystem->getFont(fontName, fontSize);
        }

        inline void draw(float depthLayer) const
        {
            renderSystem->drawText(textGeometryId, transformId, colorId, depthLayer);
        }

        inline glm::vec2 getTextExtent(const std::string& text) const
        {
            return renderSystem->queryTextExtent(text, fontId);
        }
    };
}