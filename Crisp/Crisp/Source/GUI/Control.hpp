#pragma once

#include <string>
#include <memory>

#include "Math/Headers.hpp"

#include "Math/Rect.hpp"
#include "RenderSystem.hpp"

#include "GUI/Enums.hpp"

namespace crisp
{
    namespace gui
    {
        class Form;

        class Control
        {
        public:
            Control(Form* form);
            virtual ~Control();

            void setId(std::string id);
            std::string getId() const;

            void setParent(Control* parent);
            Control* getParent() const;

            void setAnchor(Anchor anchor);
            Anchor getAnchor() const;

            void setWidthSizingBehavior(Sizing sizing, float widthPercent = 1.0f);
            Sizing getWidthSizingBehavior() const;

            void setHeightSizingBehavior(Sizing sizing, float heightPercent = 1.0f);
            Sizing getHeightSizingBehavior() const;

            void setPosition(const glm::vec2& position);
            glm::vec2 getPosition() const;

            void setSize(const glm::vec2& size);
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

            Rect<float> getAbsoluteBounds() const;
            const glm::mat4& getModelMatrix() const;

            virtual void onMouseMoved(float x, float y);
            virtual void onMouseEntered();
            virtual void onMouseExited();
            virtual void onMousePressed(float x, float y);
            virtual void onMouseReleased(float x, float y);

            virtual void setValidationFlags(Validation validation);
            void clearValidationFlags();
            Validation getValidationFlags() const;
            virtual bool needsValidation();
            virtual void validate();

            virtual void draw(RenderSystem& renderSystem) = 0;

            template <typename T>
            T* getTypedControlById(std::string id) { return dynamic_cast<T*>(getControlById(id)); }
            virtual Control* getControlById(const std::string& id);

            unsigned int getTransformId() const;

            unsigned int getRootDistance() const;
            void printDebugId() const;

        protected:
            glm::vec2 getParentAbsolutePosition() const;
            float getParentAbsoluteDepth() const;
            glm::vec2 getParentPadding() const;

            float getParentAbsoluteOpacity() const;

            glm::vec2 getAbsolutePosition() const;
            float getAbsoluteDepth() const;

            std::string m_id;
            
            Control* m_parent;

            Anchor    m_anchor;
            Sizing    m_widthSizingBehavior;
            Sizing    m_heightSizingBehavior;
            glm::vec2 m_parentSizePercent;

            glm::vec2 m_position;
            glm::vec2 m_size;
            glm::vec2 m_padding;
            
            glm::mat4 m_M; // Captures absolute position and scale etc.

            float m_scale;
            float m_depthOffset;

            Validation m_validationFlags;

            Form* m_form;
            RenderSystem* m_renderSystem;
            unsigned int m_transformId;

            glm::vec4 m_color;  // Holds absolute values
            float m_opacity;    // Holds control opacity
            unsigned int m_colorId;
        };
    }
}