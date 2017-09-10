#pragma once

#include <string>

#include "Control.hpp"

namespace crisp::gui
{
    class Label : public Control
    {
    public:
        Label(Form* parentForm, const std::string& text = "Example Text", unsigned int fontSize = 14);
        ~Label();

        void setFontSize(unsigned int fontSize);
        void setText(const std::string& text);
        glm::vec2 getTextExtent() const;

        virtual float getWidth() const override;
        virtual float getHeight() const override;
        virtual Rect<float> getAbsoluteBounds() const override;

        virtual void validate() override;
        virtual void draw(RenderSystem& visitor) override;

    private:
        std::string m_fontName;
        std::string m_text;
        glm::vec2   m_textExtent;

        unsigned int m_fontId;
        unsigned int m_transformId;
        unsigned int m_colorId;
        unsigned int m_textResourceId;
    };
}