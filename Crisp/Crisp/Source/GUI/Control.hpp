#pragma once

#include <string>
#include <memory>

#include "Math/Headers.hpp"

#include "Math/Rect.hpp"
#include "RenderSystem.hpp"

#include "GUI/Enums.hpp"

namespace crisp::gui
{
    class Form;

    class Control
    {
    public:
        Control(Form* form);
        virtual ~Control();

        void setId(std::string&& id);
        std::string getId() const;

        void setParent(Control* parent);
        Control* getParent() const;

        void setAnchor(Anchor anchor);
        Anchor getAnchor() const;

        void setSizingPolicy(SizingPolicy horizontal, SizingPolicy vertical, float widthPercent = 1.0f, float heightPercent = 1.0f);

        void setHorizontalSizingPolicy(SizingPolicy sizing, float widthPercent = 1.0f);
        SizingPolicy getHorizontalSizingPolicy() const;

        void setVerticalSizingPolicy(SizingPolicy sizing, float heightPercent = 1.0f);
        SizingPolicy getVerticalSizingPolicy() const;

        void setPosition(const glm::vec2& position);
        glm::vec2 getPosition() const;

        void setSizeHint(const glm::vec2& sizeHint);
        glm::vec2 getSize() const;

        virtual float getWidth() const;
        virtual float getHeight() const;

        void setPadding(glm::vec2&& padding);
        glm::vec2 getPadding() const;

        void setDepthOffset(float depthOffset);
        float getDepthOffset() const;

        void setScale(float scale);
        float setScale() const;

        void setColor(glm::vec4 color);
        glm::vec4 getColor() const;

        virtual void setOpacity(float opacity);
        float getOpacity() const;

        virtual Rect<float> getAbsoluteBounds() const;
        const glm::mat4& getModelMatrix() const;

        virtual void onMouseMoved(float x, float y);
        virtual void onMouseEntered();
        virtual void onMouseExited();
        virtual void onMousePressed(float x, float y);
        virtual void onMouseReleased(float x, float y);

        virtual void setValidationFlags(ValidationFlags validationFlags);
        void clearValidationFlags();
        ValidationFlags getValidationFlags() const;
        virtual bool needsValidation();
        virtual void validate() = 0;

        virtual void draw(RenderSystem& renderSystem) = 0;

        template <typename T>
        T* getTypedControlById(std::string id) { return static_cast<T*>(getControlById(id)); }
        virtual Control* getControlById(const std::string& id);

        unsigned int getRootDistance() const;
        void printDebugId() const;

    protected:
        glm::vec2 getParentAbsolutePosition() const;
        float getParentAbsoluteDepth() const;
        glm::vec2 getParentPadding() const;
        glm::vec2 getParentAbsoluteSize() const;

        float getParentAbsoluteOpacity() const;

        glm::vec2 getAbsolutePosition() const;
        float getAbsoluteDepth() const;

        std::string m_id;
            
        Control* m_parent;

        Anchor       m_anchor;
        SizingPolicy m_horizontalSizingPolicy;
        SizingPolicy m_verticalSizingPolicy;
        glm::vec2    m_parentSizePercent;

        glm::vec2 m_position;
        glm::vec2 m_sizeHint;
        glm::vec2 m_padding;

        glm::mat4 m_M; // Captures absolute position and scale etc.

        float m_scale;
        float m_depthOffset;

        BitFlags<Validation> m_validationFlags;

        Form* m_form;
        RenderSystem* m_renderSystem;

        glm::vec4 m_color;  // Holds absolute values
        float m_opacity;    // Holds control opacity
    };
}