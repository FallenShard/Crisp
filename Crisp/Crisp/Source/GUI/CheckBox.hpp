#pragma once

#include <memory>
#include <functional>

#include <CrispCore/Event.hpp>

#include "Control.hpp"
#include "Animation/PropertyAnimation.hpp"

namespace crisp::gui
{
    class Label;

    class CheckBox : public Control
    {
    public:
        CheckBox(Form* parentForm);
        virtual ~CheckBox();

        bool isChecked() const;
        void setText(const std::string& text);

        void setEnabled(bool enabled);
        void setIdleColor(const glm::vec3& color);
        void setPressedColor(const glm::vec3& color);
        void setHoverColor(const glm::vec3& color);

        virtual glm::vec2 getSize() const;
        virtual Rect<float> getAbsoluteBounds() const;

        virtual void onMouseEntered(float x, float y) override;
        virtual void onMouseExited(float x, float y) override;
        virtual void onMousePressed(float x, float y) override;
        virtual void onMouseReleased(float x, float y) override;

        virtual void validate() override;

        virtual void draw(const RenderSystem& renderSystem) const override;

        Event<bool> checked;

    private:
        enum class State
        {
            Idle,
            Hover,
            Pressed,
            Disabled
        };

        void updateTexCoordResource();
        void setState(State state);

        State m_state;
        bool  m_isChecked;

        glm::vec3 m_hoverColor;
        glm::vec3 m_idleColor;
        glm::vec3 m_pressedColor;
        glm::vec3 m_disabledColor;

        std::shared_ptr<PropertyAnimation<glm::vec4, Easing::SlowOut>> m_colorAnim;

        std::unique_ptr<Label> m_label;

        unsigned int m_transformId;
        unsigned int m_colorId;
        unsigned int m_texCoordResourceId;
    };
}