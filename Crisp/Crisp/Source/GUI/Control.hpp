#pragma once

#include <string>
#include <memory>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>

#include "Math/Rect.hpp"
#include "RenderSystem.hpp"

namespace crisp
{
    namespace gui
    {
        class Control
        {
        public:
            Control();
            virtual ~Control();

            void setId(std::string id);
            std::string getId() const;

            void setParent(Control* parent);

            void setPosition(const glm::vec2& position);
            glm::vec2 getPosition() const;

            void setSize(const glm::vec2& size);
            virtual glm::vec2 getSize() const;

            void setPadding(glm::vec2&& padding);
            glm::vec2 getPadding() const;

            void setDepth(float depth);
            float getDepth() const;

            void setScale(float scale);
            float setScale() const;

            Rect<float> getAbsoluteBounds() const;
            const glm::mat4& getModelMatrix() const;

            virtual void onMouseMoved(float x, float y);
            virtual void onMouseEntered();
            virtual void onMouseExited();
            virtual void onMousePressed(float x, float y);
            virtual void onMouseReleased(float x, float y);

            virtual void invalidate();
            virtual bool needsValidation();
            virtual void validate() = 0;
            void setValidationStatus(bool validationStatus);
            void applyParentProperties();

            virtual void draw(RenderSystem& renderSystem) = 0;

            template <typename T>
            T* getTypedControlById(std::string id)
            {
                return dynamic_cast<T*>(getControlById(id));
            }

            virtual Control* getControlById(const std::string& id);

            unsigned int getDistanceToRoot() const;

            unsigned int getTransformId() const;

        protected:
            std::string m_id;
            unsigned int m_transformId;

            Control* m_parent;

            glm::vec2 m_absolutePosition;

            glm::vec2 m_position;
            glm::vec2 m_size;
            glm::vec2 m_padding;
            glm::mat4 m_M;

            float m_scale;
            float m_opacity;
            float m_depth;

            bool m_isValidated;

            RenderSystem* m_renderSystem;
        };
    }
}