#pragma once

#include <memory>
#include <string>

#include <Crisp/Math/Headers.hpp>
#include <Crisp/Math/Rect.hpp>

#include <Crisp/Gui/GuiEnums.hpp>
#include <Crisp/Gui/RenderSystem.hpp>

namespace crisp::gui {
class Form;

class Control {
public:
    Control(Form* form);
    virtual ~Control();

    void setId(std::string&& id);
    std::string getId() const;

    void setParent(Control* parent);
    Control* getParent() const;

    void setAnchor(Anchor anchor);
    Anchor getAnchor() const;

    void setOrigin(Origin origin);
    Origin getOrigin() const;

    void setSizingPolicy(
        SizingPolicy horizontal, SizingPolicy vertical, float widthPercent = 1.0f, float heightPercent = 1.0f);

    void setHorizontalSizingPolicy(SizingPolicy sizing, float widthPercent = 1.0f);
    SizingPolicy getHorizontalSizingPolicy() const;

    void setVerticalSizingPolicy(SizingPolicy sizing, float heightPercent = 1.0f);
    SizingPolicy getVerticalSizingPolicy() const;

    void setPositionX(float xPos);
    void setPositionY(float yPos);
    void setPosition(const glm::vec2& position);
    glm::vec2 getPosition() const;

    void setWidthHint(float widthHint);
    void setHeightHint(float heightHint);
    void setSizeHint(const glm::vec2& sizeHint);
    glm::vec2 getSize() const;

    virtual float getWidth() const;
    virtual float getHeight() const;

    void setPadding(glm::vec2&& paddingX, glm::vec2 paddingY);
    void setPadding(glm::vec2&& padding);
    glm::vec2 getPaddingX() const;
    glm::vec2 getPaddingY() const;

    void setDepthOffset(float depthOffset);
    float getDepthOffset() const;

    void setScale(float scale);
    float setScale() const;

    void setColor(glm::vec3 color);
    void setColor(glm::vec4 color);
    glm::vec4 getColor() const;

    void setOpacity(float opacity);
    float getOpacity() const;

    virtual Rect<float> getAbsoluteBounds() const;
    const glm::mat4& getModelMatrix() const;

    virtual Rect<float> getInteractionBounds() const;
    virtual void onMouseMoved(float x, float y);
    virtual void onMouseEntered(float x, float y);
    virtual void onMouseExited(float x, float y);
    virtual bool onMousePressed(float x, float y);
    virtual bool onMouseReleased(float x, float y);

    void setValidationFlags(ValidationFlags validationFlags);
    void clearValidationFlags();
    bool needsValidation();
    void validateAndClearFlags();
    virtual void validate() = 0;

    virtual void draw(const RenderSystem& renderSystem) const = 0;

    template <typename T>
    T* getTypedControlById(std::string id) {
        return static_cast<T*>(getControlById(id));
    }

    virtual Control* getControlById(const std::string& id);

    unsigned int getRootDistance() const;
    virtual void printDebugId() const;
    virtual void visit(std::function<void(Control*)> func);

protected:
    glm::vec2 getParentAbsolutePosition() const;
    float getParentAbsoluteDepth() const;
    glm::vec2 getParentPaddingX() const;
    glm::vec2 getParentPaddingY() const;
    glm::vec2 getParentAbsoluteSize() const;

    float getParentAbsoluteOpacity() const;

    glm::vec2 getAnchorOffset() const;
    glm::vec2 getOriginOffset() const;
    glm::vec2 getOffsetDirection() const;
    glm::vec2 getAbsolutePosition() const;
    float getAbsoluteDepth() const;

    std::string m_id;

    Control* m_parent;

    Origin m_origin;
    Anchor m_anchor;
    SizingPolicy m_horizontalSizingPolicy;
    SizingPolicy m_verticalSizingPolicy;
    glm::vec2 m_parentSizePercent; // Used only when filling up the parent control

    glm::vec2 m_position;
    glm::vec2 m_sizeHint;
    glm::vec2 m_paddingX; // Minimum distance of children from this element's borders [left, right]
    glm::vec2 m_paddingY; // Padding [top, bottom]

    glm::mat4 m_M; // Captures absolute position and scale etc.

    float m_scale;
    float m_depthOffset;

    bool m_needsValidation; // Do we need to recalculate this widget's attributes

    Form* m_form;
    RenderSystem* m_renderSystem;

    glm::vec4 m_color; // Holds absolute values
    float m_opacity;   // Holds control opacity
};
} // namespace crisp::gui